#include "RobloxModLoader/memory/rtti_index.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/util/filesystem.hpp"
#include "RobloxModLoader/util/hash.hpp"
#include "RobloxModLoader/util/type_name.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>

RML_LOG_SCOPE("RttiIndex");

namespace rml::memory
{
	static constexpr std::uint32_t k_magic = 0x544C4D52;
	static constexpr std::uint32_t k_version = 2;

	static std::atomic<RttiIndex*> s_rtti{};

	RttiIndex* rtti()
	{
		return s_rtti.load(std::memory_order_acquire);
	}

	void install_rtti(RttiIndex* index)
	{
		s_rtti.store(index, std::memory_order_release);
	}

	static std::uint64_t query_hash(const std::string_view cpp_name)
	{
		return utils::fnv1a_64(utils::canonical_type_name(cpp_name));
	}

	static std::uint64_t glob_hash(const std::string_view glob)
	{
		return utils::fnv1a_64(glob, utils::fnv1a_64("glob:"));
	}

	RttiIndex::RttiIndex(std::unique_ptr<IRttiBackend> backend, std::filesystem::path cache_file) :
	    m_backend(std::move(backend)),
	    m_cache_file(std::move(cache_file))
	{
		const module image(platform::studio_image_name());
		m_base = image.begin().as<std::uintptr_t>();
		m_image_size = static_cast<std::uint32_t>(image.size());
		load();
	}

	RttiIndex::~RttiIndex() = default;

	std::optional<void**> RttiIndex::to_vtable(const std::uint32_t rva) const
	{
		if (rva == k_missing || rva >= m_image_size)
			return std::nullopt;
		return reinterpret_cast<void**>(m_base + rva);
	}

	void RttiIndex::load()
	{
		const auto bytes = utils::read_file(m_cache_file);
		if (!bytes || bytes->size() < 32)
			return;

		const auto* cursor = bytes->data();
		const auto* const end = cursor + bytes->size();
		const auto read = [&](auto& value) {
			if (cursor + sizeof(value) > end)
				return false;
			std::memcpy(&value, cursor, sizeof(value));
			cursor += sizeof(value);
			return true;
		};

		std::uint32_t magic{};
		std::uint32_t version{};
		platform::ImageIdentity identity{};
		std::uint32_t count{};
		if (!read(magic) || !read(version) || !read(identity.stamp) || !read(identity.image_size) || !read(count))
			return;
		if (magic != k_magic || version != k_version || identity != platform::studio_image_identity() || identity.image_size == 0)
			return;

		for (std::uint32_t i = 0; i < count; ++i)
		{
			std::uint64_t hash{};
			std::uint32_t rva{};
			if (!read(hash) || !read(rva))
				return;
			if (rva < m_image_size)
				m_results.emplace(hash, rva);
		}

		RML_DEBUG("Loaded {} RTTI results from {}", m_results.size(), m_cache_file.filename().string());
	}

	void RttiIndex::save() const
	{
		const auto identity = platform::studio_image_identity();
		if (identity.image_size == 0)
			return;

		std::string bytes;
		const auto write = [&bytes](const auto& value) {
			bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
		};

		write(k_magic);
		write(k_version);
		write(identity.stamp);
		write(identity.image_size);
		const auto found = std::ranges::count_if(m_results, [](const auto& result) { return result.second != k_missing; });
		if (found == 0)
			return;

		write(static_cast<std::uint32_t>(found));
		for (const auto& [hash, rva] : m_results)
		{
			if (rva == k_missing)
				continue;
			write(hash);
			write(rva);
		}

		if (!utils::write_file_atomic(m_cache_file, bytes))
			RML_WARN("Could not write {}", m_cache_file.string());
	}

	void RttiIndex::ensure_table()
	{
		if (m_table_built)
			return;
		m_table_built = true;

		const auto started = std::chrono::steady_clock::now();
		struct Context
		{
			std::vector<TableEntry>* table;
			std::uintptr_t base;
		} context{&m_table, m_base};

		m_backend->enumerate(
		    [](void* raw, const std::string_view name, void** vtable) {
			    auto& ctx = *static_cast<Context*>(raw);
			    ctx.table->push_back({utils::fnv1a_64(utils::canonical_type_name(name)), static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(vtable) - ctx.base)});
		    },
		    &context);

		std::ranges::stable_sort(m_table, {}, &TableEntry::hash);
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
		RML_INFO("Indexed {} RTTI classes in {} ms", m_table.size(), elapsed);
	}

	std::uint32_t RttiIndex::lookup_table(const std::uint64_t hash) const
	{
		const auto it = std::ranges::lower_bound(m_table, hash, {}, &TableEntry::hash);
		return it != m_table.end() && it->hash == hash ? it->rva : k_missing;
	}

	std::optional<void**> RttiIndex::find(const std::string_view cpp_name)
	{
		const auto hash = query_hash(cpp_name);
		std::scoped_lock lock(m_mutex);

		if (const auto it = m_results.find(hash); it != m_results.end())
			return to_vtable(it->second);

		ensure_table();
		const auto rva = lookup_table(hash);
		m_results.emplace(hash, rva);
		save();

		if (rva == k_missing)
			RML_WARN("No RTTI for '{}'", cpp_name);
		return to_vtable(rva);
	}

	std::optional<void**> RttiIndex::find_matching(const std::string_view canonical_glob)
	{
		const auto hash = glob_hash(canonical_glob);
		std::scoped_lock lock(m_mutex);

		if (const auto it = m_results.find(hash); it != m_results.end())
			return to_vtable(it->second);

		struct Context
		{
			std::string_view glob;
			std::uintptr_t base;
			std::uint32_t rva;
		} context{canonical_glob, m_base, k_missing};

		m_backend->enumerate(
		    [](void* raw, const std::string_view name, void** vtable) {
			    auto& ctx = *static_cast<Context*>(raw);
			    if (ctx.rva == k_missing && utils::glob_match(ctx.glob, utils::canonical_type_name(name)))
				    ctx.rva = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(vtable) - ctx.base);
		    },
		    &context);

		m_results.emplace(hash, context.rva);
		save();

		if (context.rva == k_missing)
			RML_WARN("No RTTI class matches '{}'", canonical_glob);
		return to_vtable(context.rva);
	}

	void RttiIndex::prefetch(const std::span<const std::string_view> cpp_names)
	{
		std::scoped_lock lock(m_mutex);

		bool changed = false;
		for (const auto name : cpp_names)
		{
			const auto hash = query_hash(name);
			if (m_results.contains(hash))
				continue;

			ensure_table();
			m_results.emplace(hash, lookup_table(hash));
			changed = true;
		}

		if (changed)
			save();
	}

	void RttiIndex::trim()
	{
		std::scoped_lock lock(m_mutex);
		m_table.clear();
		m_table.shrink_to_fit();
		m_table_built = false;
	}
}
