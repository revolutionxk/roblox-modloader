#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rml::memory
{
	class IRttiBackend
	{
	public:
		using Sink = void (*)(void* context, std::string_view name, void** vtable);

		virtual ~IRttiBackend() = default;
		virtual void enumerate(Sink sink, void* context) const = 0;
	};

	[[nodiscard]] std::unique_ptr<IRttiBackend> create_rtti_backend();

	class RML_EXPORT RttiIndex
	{
	public:
		RttiIndex(std::unique_ptr<IRttiBackend> backend, std::filesystem::path cache_file);
		~RttiIndex();

		RttiIndex(const RttiIndex&) = delete;
		RttiIndex& operator=(const RttiIndex&) = delete;

		[[nodiscard]] std::optional<void**> find(std::string_view cpp_name);
		[[nodiscard]] std::optional<void**> find_matching(std::string_view canonical_glob);
		void prefetch(std::span<const std::string_view> cpp_names);
		void trim();

	private:
		struct TableEntry
		{
			std::uint64_t hash;
			std::uint32_t rva;
		};

		static constexpr std::uint32_t k_missing = 0xFFFFFFFF;

		void load();
		void save() const;
		void ensure_table();
		[[nodiscard]] std::optional<void**> to_vtable(std::uint32_t rva) const;
		[[nodiscard]] std::uint32_t lookup_table(std::uint64_t hash) const;

		std::unique_ptr<IRttiBackend> m_backend;
		std::filesystem::path m_cache_file;
		std::uintptr_t m_base{};
		std::uint32_t m_image_size{};
		std::mutex m_mutex;
		std::unordered_map<std::uint64_t, std::uint32_t> m_results;
		std::vector<TableEntry> m_table;
		bool m_table_built{};
	};

	[[nodiscard]] RML_EXPORT RttiIndex* rtti();
	void install_rtti(RttiIndex* index);
}
