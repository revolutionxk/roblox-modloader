#include "RobloxModLoader/memory/module.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/symbol_resolver.hpp"

#if defined(RML_MACOS)
	#include <mach-o/nlist.h>
#endif

namespace rml::memory
{
	static std::pair<std::string_view, std::string_view> split_qualified_name(std::string_view signature)
	{
		const auto name = signature.substr(0, signature.find('('));

		const auto separator = name.rfind("::");
		if (separator == std::string_view::npos)
			return {{}, name};

		const auto scope = name.substr(0, separator);
		const auto scope_separator = scope.rfind("::");

		return {scope_separator == std::string_view::npos ? scope : scope.substr(scope_separator + 2),
		    name.substr(separator + 2)};
	}
	
	static int itanium_variant_rank(std::string_view mangled, std::string_view signature)
	{
		const auto [class_name, name] = split_qualified_name(signature);

		const auto rank_of = [mangled](const std::string_view (&tags)[3], const int (&ranks)[3]) {
			for (std::size_t i = 0; i < 3; ++i)
			{
				if (mangled.find(tags[i]) != std::string_view::npos)
					return ranks[i];
			}
			return 0;
		};

		if (name.starts_with('~'))
		{
			static constexpr std::string_view tags[]{"D1E", "D2E", "D0E"};
			static constexpr int ranks[]{0, 1, 100};
			return rank_of(tags, ranks);
		}

		if (!class_name.empty() && class_name == name)
		{
			static constexpr std::string_view tags[]{"C1E", "C2E", "C3E"};
			static constexpr int ranks[]{0, 1, 2};
			return rank_of(tags, ranks);
		}

		return 0;
	}

	module::module(std::string_view name) :range(nullptr, 0), m_name(name)
	{
		std::scoped_lock lk(m_mtx);
		try_get_module_locked();
	}

	module::module(std::filesystem::path path) :range(nullptr, 0), m_name(path.filename().string()), m_path(std::move(path))
	{
		std::scoped_lock lk(m_mtx);
		try_get_module_locked();
	}

	bool module::loaded() const noexcept
	{
		std::scoped_lock lk(m_mtx);
		return m_loaded;
	}

	size_t module::size() const noexcept
	{
		std::scoped_lock lk(m_mtx);
		return m_size;
	}

	handle module::get_export(std::string_view symbol_name) const
	{
		std::scoped_lock lk(m_mtx);
		if (!m_loaded)
			return handle(nullptr);

#if defined(RML_WINDOWS)
		const HMODULE mod = m_attached_handle ? static_cast<HMODULE>(m_attached_handle) : GetModuleHandleA(m_name.c_str());

		if (!mod)
			return handle(nullptr);

		return handle(reinterpret_cast<void*>(GetProcAddress(mod, symbol_name.data())));

#else
		void* sym = nullptr;

		if (m_attached_handle)
		{
			sym = dlsym(m_attached_handle, symbol_name.data());
		}
		else
		{
			void* h = dlopen(m_name.c_str(), RTLD_NOW | RTLD_NOLOAD);
			if (h)
			{
				sym = dlsym(h, symbol_name.data());
				dlclose(h);
			}
			else
			{
				sym = dlsym(RTLD_DEFAULT, symbol_name.data());
			}
		}

		return handle(sym);
#endif
	}

	handle module::find_export(std::string_view signature) const
	{
		std::scoped_lock lk(m_mtx);

		if (!m_loaded)
			return handle(nullptr);

		build_export_index_locked();

		const auto it = m_export_index.find(normalize_signature(signature));
		return it != m_export_index.end() ? handle(it->second) : handle(nullptr);
	}

	void module::build_export_index_locked() const
	{
		if (m_export_index_built)
			return;

		m_export_index_built = true;

		std::unordered_map<std::string, int> ranks;

		const auto insert = [&](const char* mangled, void* address) {
			const std::string signature = demangle_signature(mangled);
			if (signature.empty())
				return;

			std::string key = normalize_signature(signature);
			if (key.empty())
				return;

			const int rank = itanium_variant_rank(mangled, signature);

			if (const auto it = ranks.find(key); it != ranks.end() && it->second <= rank)
				return;

			ranks.insert_or_assign(key, rank);
			m_export_index.insert_or_assign(std::move(key), address);
		};

#if defined(RML_WINDOWS)
		const auto* dos = m_base.as<const IMAGE_DOS_HEADER*>();
		const auto* nt = m_base.add(dos->e_lfanew).as<const IMAGE_NT_HEADERS*>();

		const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if (!directory.VirtualAddress || !directory.Size)
			return;

		const auto* exports = m_base.add(directory.VirtualAddress).as<const IMAGE_EXPORT_DIRECTORY*>();
		const auto* names = m_base.add(exports->AddressOfNames).as<const DWORD*>();
		const auto* ordinals = m_base.add(exports->AddressOfNameOrdinals).as<const WORD*>();
		const auto* functions = m_base.add(exports->AddressOfFunctions).as<const DWORD*>();

		for (DWORD i = 0; i < exports->NumberOfNames; ++i)
		{
			const DWORD rva = functions[ordinals[i]];

			// An RVA inside the export directory is a forwarder string, not code.
			if (rva >= directory.VirtualAddress && rva < directory.VirtualAddress + directory.Size)
				continue;

			insert(m_base.add(names[i]).as<const char*>(), m_base.add(rva).as<void*>());
		}

#elif defined(RML_MACOS)
		const auto* mh = m_base.as<const mach_header_64*>();
		if (!mh || mh->magic != MH_MAGIC_64)
			return;

		const symtab_command* symtab = nullptr;
		const segment_command_64* linkedit = nullptr;

		std::uintptr_t text_vmaddr = 0;
		bool found_text = false;

		const auto* cmd = reinterpret_cast<const load_command*>(mh + 1);
		for (uint32_t i = 0; i < mh->ncmds; ++i)
		{
			if (cmd->cmd == LC_SYMTAB)
			{
				symtab = reinterpret_cast<const symtab_command*>(cmd);
			}
			else if (cmd->cmd == LC_SEGMENT_64)
			{
				const auto* seg = reinterpret_cast<const segment_command_64*>(cmd);
				if (std::strncmp(seg->segname, SEG_LINKEDIT, sizeof(seg->segname)) == 0)
					linkedit = seg;
				else if (std::strncmp(seg->segname, SEG_TEXT, sizeof(seg->segname)) == 0)
				{
					text_vmaddr = static_cast<std::uintptr_t>(seg->vmaddr);
					found_text = true;
				}
			}

			cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(cmd) + cmd->cmdsize);
		}

		if (!symtab || !linkedit || !found_text)
			return;

		const auto slide = m_base.as<std::uintptr_t>() - text_vmaddr;
		const auto linkedit_base = slide + static_cast<std::uintptr_t>(linkedit->vmaddr) - static_cast<std::uintptr_t>(linkedit->fileoff);

		const auto* symbols = reinterpret_cast<const nlist_64*>(linkedit_base + symtab->symoff);
		const auto* strings = reinterpret_cast<const char*>(linkedit_base + symtab->stroff);

		for (uint32_t i = 0; i < symtab->nsyms; ++i)
		{
			const nlist_64& symbol = symbols[i];

			if (symbol.n_type & N_STAB)
				continue;
			if ((symbol.n_type & N_TYPE) != N_SECT || !(symbol.n_type & N_EXT))
				continue;
			if (!symbol.n_value || !symbol.n_un.n_strx)
				continue;

			const char* name = strings + symbol.n_un.n_strx;

			if (*name == '_')
				++name;

			insert(name, reinterpret_cast<void*>(static_cast<std::uintptr_t>(symbol.n_value) + slide));
		}
#endif
	}

	bool module::wait_for_module(std::optional<std::chrono::steady_clock::duration> timeout)
	{
		using clock = std::chrono::steady_clock;

		const auto deadline = timeout ? std::make_optional(clock::now() + *timeout) : std::nullopt;

		LOG_DEBUG("Waiting for {}...", m_name);

		while (true)
		{
			{
				std::scoped_lock lk(m_mtx);
				if (try_get_module_locked())
					return true;
			}

			if (deadline && clock::now() >= *deadline)
				break;

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		LOG_DEBUG("Timed out waiting for {}", m_name);
		return false;
	}

	std::expected<void, std::string> module::attach()
	{
		std::scoped_lock lk(m_mtx);

		if (m_attached_handle)
			return std::unexpected(std::format("Module '{}' already attached", m_name));

#if defined(RML_WINDOWS)
		HMODULE loaded = nullptr;

		if (m_path)
		{
			const std::string abs_path_str = std::filesystem::absolute(*m_path).string();
			const std::wstring abs_path_w = std::filesystem::absolute(*m_path).wstring();
			const std::wstring dll_dir_w = m_path->parent_path().wstring();

			const auto h_module = LoadLibraryExW(abs_path_w.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

			const DWORD err = GetLastError();
			if (!h_module)
				return std::unexpected(std::format("Failed to attach module '{}' : Failed to load module '{}': {}", m_name, abs_path_str, std::system_category().message(static_cast<int>(err))));

			loaded = h_module;
		}
		else
		{
			loaded = LoadLibraryA(m_name.c_str());
			if (!loaded)
			{
				const DWORD err = GetLastError();
				return std::unexpected(std::format("Failed to attach module '{}': {}", m_name, std::system_category().message(static_cast<int>(err))));
			}
		}

		m_attached_handle = static_cast<void*>(loaded);
		m_attached_owner = true;
		m_loaded = true;
		m_base = handle(loaded);

		const auto* dos = m_base.as<const IMAGE_DOS_HEADER*>();
		const auto* nt = m_base.add(dos->e_lfanew).as<const IMAGE_NT_HEADERS*>();
		m_size = nt->OptionalHeader.SizeOfImage;

		return {};

#else
		const std::string load_target = m_path ? m_path->string() : m_name;

		const bool already_loaded = (dlopen(load_target.c_str(), RTLD_NOW | RTLD_NOLOAD) != nullptr);

		void* h = dlopen(load_target.c_str(), RTLD_NOW | RTLD_LOCAL);
		if (!h)
		{
			const char* err = dlerror();
			return std::unexpected(std::format("Failed to attach module '{}': {}", m_name, err ? err : "(unknown dlerror)"));
		}

		m_attached_handle = h;
		m_attached_owner = !already_loaded;

		try_get_module_locked();
		return {};
#endif
	}

	std::expected<void, std::string> module::detach()
	{
		std::scoped_lock lk(m_mtx);

		if (!m_attached_handle)
			return {};

#if defined(RML_WINDOWS)
		if (m_attached_owner && !FreeLibrary(static_cast<HMODULE>(m_attached_handle)))
		{
			const DWORD err = GetLastError();
			return std::unexpected(std::format("Failed to detach module '{}': {}", m_name, std::system_category().message(static_cast<int>(err))));
		}
#else
		if (m_attached_owner && dlclose(m_attached_handle) != 0)
		{
			const char* err = dlerror();
			return std::unexpected(std::format("Failed to detach module '{}': {}", m_name, err ? err : "(unknown dlerror)"));
		}
#endif

		m_attached_handle = nullptr;
		m_attached_owner = false;
		reset_state_locked();
		return {};
	}

	void module::reset_state_locked() noexcept
	{
		m_loaded = false;
		m_base = handle(nullptr);
		m_size = 0;
		m_export_index.clear();
		m_export_index_built = false;
	}

#if defined(RML_LINUX)
	int module::phdr_callback(struct ::dl_phdr_info* info, size_t /*sz*/, void* data) noexcept
	{
		auto* ctx = static_cast<PhdrSearchCtx*>(data);

		if (!info->dlpi_name || !ctx->search_name)
			return 0;
		if (!strstr(info->dlpi_name, ctx->search_name))
			return 0;

		std::uintptr_t min_vaddr = UINTPTR_MAX;
		std::uintptr_t max_vaddr = 0;

		for (int i = 0; i < info->dlpi_phnum; ++i)
		{
			const ElfW(Phdr) & ph = info->dlpi_phdr[i];
			if (ph.p_type != PT_LOAD)
				continue;

			const auto start = static_cast<std::uintptr_t>(ph.p_vaddr);
			const auto end = start + static_cast<std::uintptr_t>(ph.p_memsz);
			min_vaddr = std::min(min_vaddr, start);
			max_vaddr = std::max(max_vaddr, end);
		}

		if (min_vaddr == UINTPTR_MAX)
			min_vaddr = 0;

		ctx->found_base = static_cast<std::uintptr_t>(info->dlpi_addr) + min_vaddr;
		ctx->found_size = max_vaddr > min_vaddr ? (max_vaddr - min_vaddr) : 0;
		return 1;
	}
#endif

	bool module::try_get_module_locked()
	{
		if (m_loaded)
			return true;

#if defined(RML_WINDOWS)
		const HMODULE mod = GetModuleHandleA(m_name.c_str());
		if (!mod)
			return false;

		m_base = handle(mod);
		m_loaded = true;

		const auto* dos = m_base.as<const IMAGE_DOS_HEADER*>();
		const auto* nt = m_base.add(dos->e_lfanew).as<const IMAGE_NT_HEADERS*>();
		m_size = nt->OptionalHeader.SizeOfImage;

		return true;

#elif defined(RML_LINUX)
		PhdrSearchCtx ctx{m_name.c_str(), 0, 0};
		dl_iterate_phdr(phdr_callback, &ctx);

		if (ctx.found_base != 0)
		{
			m_loaded = true;
			m_base = handle(static_cast<std::uintptr_t>(ctx.found_base));
			m_size = ctx.found_size;
			return true;
		}

		void* h = dlopen(m_name.c_str(), RTLD_NOW | RTLD_NOLOAD);
		if (!h)
			return false;

		dlclose(h);
		m_loaded = true;
		m_base = handle(nullptr);
		m_size = 0;
		return true;

#elif defined(RML_MACOS)
		const uint32_t image_count = _dyld_image_count();

		for (uint32_t i = 0; i < image_count; ++i)
		{
			const char* image_name = _dyld_get_image_name(i);
			if (!image_name || !strstr(image_name, m_name.c_str()))
				continue;

			const auto* mh = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(i));
			const intptr_t slide = _dyld_get_image_vmaddr_slide(i);

			if (mh->magic != MH_MAGIC_64)
				continue;

			const auto* cmd = reinterpret_cast<const load_command*>(mh + 1);

			std::uintptr_t min_addr = UINTPTR_MAX;
			std::uintptr_t max_addr = 0;

			for (uint32_t ci = 0; ci < mh->ncmds; ++ci)
			{
				if (cmd->cmd == LC_SEGMENT_64)
				{
					const auto* seg = reinterpret_cast<const segment_command_64*>(cmd);
					const bool is_page_zero = std::strncmp(seg->segname, SEG_PAGEZERO, sizeof(seg->segname)) == 0;

					if (!is_page_zero && seg->vmsize > 0)
					{
						const auto start = static_cast<std::uintptr_t>(seg->vmaddr) + static_cast<std::uintptr_t>(slide);
						const auto end = start + static_cast<std::uintptr_t>(seg->vmsize);
						min_addr = std::min(min_addr, start);
						max_addr = std::max(max_addr, end);
					}
				}
				cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(cmd) + cmd->cmdsize);
			}

			if (min_addr == UINTPTR_MAX)
				min_addr = reinterpret_cast<std::uintptr_t>(mh) + static_cast<std::uintptr_t>(slide);

			m_loaded = true;
			m_base = handle(reinterpret_cast<void*>(min_addr));
			m_size = max_addr > min_addr ? (max_addr - min_addr) : 0;
			return true;
		}

		return false;

#else
		return false;
#endif
	}

} // namespace memory