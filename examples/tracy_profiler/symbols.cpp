#include "symbols.hpp"

#include "timers.hpp"

#include <RobloxModLoader/memory/rtti_index.hpp>
#include <RobloxModLoader/memory/string_anchor.hpp>
#include <RobloxModLoader/platform/memory/host_image.hpp>
#include <algorithm>
#include <charconv>
#include <deque>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <tuple>
#include <utility>

namespace tracy_profiler
{
	static constexpr std::size_t max_virtual_slots = 512;

	Symbols::Symbols(const Timers& timers, const std::uintptr_t image_base) :
	    m_timers(timers),
	    m_image_base(image_base)
	{
	}

	Symbols::~Symbols() = default;

	void Symbols::load_file(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file)
			return;

		std::unique_lock lock(m_mutex);
		std::string line;
		while (std::getline(file, line))
		{
			std::uintptr_t rva{};
			const auto* begin = line.data();
			const auto* end = begin + line.size();
			const auto [next, error] = std::from_chars(begin, end, rva, 16);
			if (error != std::errc{} || next == end || *next != ' ' || next + 1 == end)
				continue;
			m_names.insert_or_assign(m_image_base + rva, std::string(next + 1, end));
		}
	}

	void Symbols::add(const void* function, std::string name)
	{
		if (!function)
			return;

		std::unique_lock lock(m_mutex);
		m_names.try_emplace(reinterpret_cast<std::uintptr_t>(function), std::move(name));
	}

	void Symbols::index_rtti()
	{
		m_rtti = std::jthread([this](const std::stop_token& stop) {
			try
			{
				struct Candidate
				{
					std::size_t slots;
					const std::string* class_name;
					std::size_t slot;
				};

				struct Context
				{
					std::stop_token stop;
					std::deque<std::string> class_names;
					std::unordered_map<std::uintptr_t, Candidate> found;
				};

				const auto backend = rml::memory::create_rtti_backend();
				if (!backend)
					return;

				Context context{stop, {}, {}};
				backend->enumerate(
				    [](void* raw, const std::string_view class_name, void** vtable) {
					    auto& context = *static_cast<Context*>(raw);
					    if (context.stop.stop_requested() || !vtable)
						    return;

					    std::size_t slots = 0;
					    while (slots < max_virtual_slots)
					    {
						    const auto function = rml::memory::function_containing(vtable[slots]);
						    if (!function || function->start != vtable[slots])
							    break;
						    ++slots;
					    }

					    if (slots == 0)
						    return;

					    const auto& name = context.class_names.emplace_back(class_name);
					    for (std::size_t slot = 0; slot < slots; ++slot)
					    {
						    const Candidate candidate{slots, &name, slot};
						    const auto [entry, inserted] = context.found.try_emplace(reinterpret_cast<std::uintptr_t>(vtable[slot]), candidate);
						    const auto& current = entry->second;
						    if (!inserted
						        && std::tie(candidate.slots, *candidate.class_name) < std::tie(current.slots, *current.class_name))
							    entry->second = candidate;
					    }
				    },
				    &context);

				if (stop.stop_requested())
					return;

				std::unique_lock lock(m_mutex);
				for (const auto& [address, candidate] : context.found)
					m_names.try_emplace(address, std::format("{}::vfunc_{}", *candidate.class_name, candidate.slot));
			}
			catch (...)
			{
			}
		});
	}

	void Symbols::index_scope_owners(const void* get_token)
	{
		if (!get_token)
			return;

		std::vector<std::uintptr_t> owners;
		for (const auto& function : rml::memory::functions_calling(get_token))
			owners.push_back(reinterpret_cast<std::uintptr_t>(function.start));
		std::ranges::sort(owners);

		std::unique_lock lock(m_mutex);
		m_scope_owners = std::move(owners);
	}

	std::string Symbols::name_of(const std::uintptr_t start, const std::size_t size) const
	{
		std::optional<std::pair<std::uintptr_t, std::string>> pinned;
		{
			std::shared_lock lock(m_mutex);
			if (const auto it = m_names.find(start); it != m_names.end())
				return it->second;
			if (!std::ranges::binary_search(m_scope_owners, start))
				return std::format("sub_{:x}", start - m_image_base);
			if (const auto it = m_scope_names.find(start); it != m_scope_names.end())
				pinned = it->second;
		}

		if (pinned && !m_timers.shared_site(pinned->first))
			return pinned->second;

		auto scope = m_timers.scope_within(start, start + size);
		std::unique_lock lock(m_mutex);
		if (!scope)
		{
			m_scope_names.erase(start);
			return std::format("sub_{:x}", start - m_image_base);
		}

		const auto [entry, inserted] = m_scope_names.try_emplace(start, scope->site, scope->name);
		if (!inserted && entry->second.first != scope->site && m_timers.shared_site(entry->second.first))
			entry->second = {scope->site, std::move(scope->name)};
		return entry->second.second;
	}

	std::string Symbols::function_name(const std::uintptr_t address) const
	{
		const auto function = rml::memory::function_containing(reinterpret_cast<const void*>(address - 1));
		if (!function)
			return std::format("0x{:x}", address);
		return name_of(reinterpret_cast<std::uintptr_t>(function->start), function->size);
	}

	std::uintptr_t Symbols::image_base() const
	{
		return m_image_base;
	}

	std::size_t Symbols::scope_owner_count() const
	{
		std::shared_lock lock(m_mutex);
		return m_scope_owners.size();
	}

	bool Symbols::resolve(const std::uint64_t address, ___tracy_resolved_symbol& symbol) const
	{
		thread_local std::string buffer;

		const auto function = rml::memory::function_containing(reinterpret_cast<const void*>(address));
		if (!function)
			return false;

		const auto start = reinterpret_cast<std::uintptr_t>(function->start);
		buffer = name_of(start, function->size);

		const auto* image = rml::platform::studio_image_name().data();
		symbol = {buffer.c_str(), image, image, 0, static_cast<std::uint32_t>(function->size), start};
		return true;
	}

	int32_t Symbols::tracy_resolver(const uint64_t address, ___tracy_resolved_symbol* symbol, void* user)
	{
		try
		{
			return static_cast<const Symbols*>(user)->resolve(address, *symbol) ? 1 : 0;
		}
		catch (...)
		{
			return 0;
		}
	}
}
