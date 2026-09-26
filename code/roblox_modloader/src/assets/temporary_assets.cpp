#include "RobloxModLoader/assets/temporary_assets.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/memory/string_anchor.hpp"
#include "RobloxModLoader/roblox/subsystem.hpp"
#include "RobloxModLoader/util/filesystem.hpp"

#include <cstring>
#include <format>
#include <fstream>
#include <mutex>
#include <span>

RML_LOG_SCOPE("Assets");

namespace rml::assets
{
	using Factory = RBX::ContentProviderTemporaryIdFactory;
	using FactoryHandle = RBX::SubsystemHandle<Factory>;

	static constexpr std::string_view k_factory_name = "ContentProviderTemporaryIdFactory";
	static constexpr std::string_view k_files_mutex_name = "TemporaryIdsForFilenames";
	static constexpr std::string_view k_temporary_id_format = "rbxtemp://%lld";
	static constexpr std::size_t k_startup_chain_depth = 4;

	struct ContentIdBits
	{
		alignas(RBX::ContentId) std::byte bytes[sizeof(RBX::ContentId)];
	};

	struct EngineFunctions
	{
		FactoryHandle* (*startup)();
		void* register_temporary_id_for_file;
		Factory* factory;
	};

	static std::expected<memory::AnchoredFunction, std::string> single_function_referencing(const std::string_view text)
	{
		const auto functions = memory::functions_referencing_string(text);
		if (functions.size() != 1)
			return std::unexpected(std::format("expected one function referencing '{}', found {}", text, functions.size()));
		return functions.front();
	}

	static std::expected<memory::AnchoredFunction, std::string> single_caller(const memory::AnchoredFunction& function)
	{
		const auto callers = memory::functions_calling(function.start);
		if (callers.size() != 1)
			return std::unexpected(std::format("expected one caller of {:#x}, found {}",
			    reinterpret_cast<std::uintptr_t>(function.start),
			    callers.size()));
		return callers.front();
	}

	static std::expected<memory::AnchoredFunction, std::string> find_startup()
	{
		auto function = single_function_referencing(k_files_mutex_name);
		for (std::size_t depth = 0; function && depth < k_startup_chain_depth; ++depth)
			function = single_caller(*function);
		return function;
	}

#if defined(RML_WINDOWS)
	static bool uses_field_offset(const memory::AnchoredFunction& function, const std::uint32_t offset)
	{
		const std::span<const std::uint8_t> code(static_cast<const std::uint8_t*>(function.start), function.size);
		for (std::size_t i = 0; i + 4 <= code.size(); ++i)
			if ((code[i] & 0xF8) == 0x48 && code[i + 1] == 0x8D && (code[i + 2] & 0xC0) == 0x40 && code[i + 3] == offset)
				return true;
		return false;
	}
#else
	static bool uses_field_offset(const memory::AnchoredFunction& function, const std::uint32_t offset)
	{
		const std::span<const std::uint32_t> code(static_cast<const std::uint32_t*>(function.start), function.size / 4);
		for (const auto instruction : code)
			if ((instruction & 0xFFC00000) == 0x91000000 && ((instruction >> 5) & 0x1F) != 31 && ((instruction >> 10) & 0xFFF) == offset)
				return true;
		return false;
	}
#endif

	static std::expected<memory::AnchoredFunction, std::string> find_register_for_file()
	{
		const auto from_temporary_id = single_function_referencing(k_temporary_id_format);
		if (!from_temporary_id)
			return std::unexpected(from_temporary_id.error());

		std::vector<memory::AnchoredFunction> matches;
		const auto callers = memory::functions_calling(from_temporary_id->start);
		RML_DEBUG("fromTemporaryId at 0x{:X} has {} callers",
		    reinterpret_cast<std::uintptr_t>(from_temporary_id->start),
		    callers.size());
		for (const auto& caller : callers)
		{
			const bool file_fields = uses_field_offset(caller, offsetof(Factory, files_mutex)) && uses_field_offset(caller, offsetof(Factory, files));
			const bool buffer_fields = uses_field_offset(caller, offsetof(Factory, buffers_mutex)) || uses_field_offset(caller, offsetof(Factory, buffers));
			if (file_fields && !buffer_fields)
				matches.push_back(caller);
		}

		if (matches.size() != 1)
			return std::unexpected(std::format("expected one registerTemporaryIdForFile candidate, found {}", matches.size()));
		return matches.front();
	}

	static std::expected<EngineFunctions, std::string> resolve()
	{
		const auto startup = find_startup();
		if (!startup)
			return std::unexpected("temporary id factory startup: " + startup.error());

		const auto register_for_file = find_register_for_file();
		if (!register_for_file)
			return std::unexpected("registerTemporaryIdForFile: " + register_for_file.error());

		EngineFunctions functions{};
		functions.startup = reinterpret_cast<FactoryHandle* (*)()>(startup->start);
		functions.register_temporary_id_for_file = register_for_file->start;

		const auto* handle = functions.startup();
		if (!handle || !handle->instance)
			return std::unexpected("temporary id factory startup returned no instance");

		functions.factory = handle->instance;
		if (!functions.factory->name || std::string_view(functions.factory->name) != k_factory_name
		    || functions.factory->name_length != k_factory_name.size())
			return std::unexpected("temporary id factory instance failed validation");

		RML_INFO("ContentProviderTemporaryIdFactory at 0x{:X} (startup 0x{:X}, registerTemporaryIdForFile 0x{:X}, {} file ids, {} buffer ids)",
		    reinterpret_cast<std::uintptr_t>(functions.factory),
		    reinterpret_cast<std::uintptr_t>(startup->start),
		    reinterpret_cast<std::uintptr_t>(register_for_file->start),
		    functions.factory->files.size(),
		    functions.factory->buffers.size());
		return functions;
	}

	static const std::expected<EngineFunctions, std::string>& engine()
	{
		static const auto resolved = resolve();
		return resolved;
	}

	std::expected<Factory*, std::string> temporary_id_factory()
	{
		const auto& functions = engine();
		if (!functions)
			return std::unexpected(functions.error());
		return functions->factory;
	}

	std::expected<RBX::ContentId, std::string> register_file(const std::filesystem::path& path)
	{
		const auto& functions = engine();
		if (!functions)
			return std::unexpected(functions.error());

		std::error_code error;
		const auto absolute = std::filesystem::absolute(path, error);
		if (error || !std::filesystem::is_regular_file(absolute, error))
			return std::unexpected(std::format("'{}' is not a readable file", path.string()));

		static std::mutex mutex;
		std::lock_guard lock(mutex);

		const std::string native = absolute.string();
		const auto before = functions->factory->files.size();

		ContentIdBits bits{};
		memory::call_returning_member<ContentIdBits>(functions->register_temporary_id_for_file, bits, functions->factory, &native);

		RBX::ContentId id;
		std::memcpy(static_cast<void*>(&id), bits.bytes, sizeof(bits));

		if (!id.is_temporary() || functions->factory->files.size() != before + 1)
			return std::unexpected(std::format("registerTemporaryIdForFile returned '{}' ({} -> {} file ids)",
			    id.to_string(),
			    before,
			    functions->factory->files.size()));

		RML_DEBUG("registered {} -> {}", id.to_string(), native);
		return id;
	}

	std::expected<RBX::ContentId, std::string> register_bytes(const std::filesystem::path& cache_file, const void* data, const std::size_t size)
	{
		if (!utils::write_file(cache_file, std::string_view(static_cast<const char*>(data), size)))
			return std::unexpected(std::format("could not write '{}'", cache_file.string()));

		return register_file(cache_file);
	}
}
