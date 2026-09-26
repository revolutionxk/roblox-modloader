#include "RobloxModLoader/memory/signature_cache.hpp"

#include "RobloxModLoader/internal/platform.hpp"
#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/memory/batch.hpp"
#include "RobloxModLoader/memory/handle.hpp"
#include "RobloxModLoader/memory/string_anchor.hpp"
#include "filesystem/directory.hpp"

#if defined(RML_WINDOWS)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
#elif defined(RML_MACOS)
	#include <mach-o/loader.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

RML_LOG_SCOPE("SigCache");

namespace rml::memory
{
	class SignatureCache
	{
	public:
		static bool run(std::span<const signature> entries, range region, std::uint32_t sigset_hash)
		{
			const std::uintptr_t base = region.begin().as<std::uintptr_t>();
			const ExeIdentity id = read_exe_identity(region);

			if (const auto cached = load(); cached && id.image_size != 0 && cached->sigset_hash == sigset_hash && cached->id == id)
			{
				if (apply(entries, base, id, *cached))
				{
					LOG_INFO("Applied {} signatures from cache", entries.size());
					return true;
				}
				LOG_WARN("Signature cache present but incomplete/out-of-range - rescanning");
			}
			else
			{
				LOG_INFO("No valid signature cache for this Studio build - scanning");
			}

			std::unordered_map<std::uint32_t, std::uint32_t> rvas;
			rvas.reserve(entries.size());
			const bool found_all = scan(entries, region, base, rvas);

			if (found_all && id.image_size != 0)
			{
				save(CacheData{id, sigset_hash, std::move(rvas)});
				LOG_INFO("Wrote signature cache ({} signatures)", entries.size());
			}
			else if (!found_all)
			{
				LOG_WARN("Some signatures missing - cache not written (signatures need updating for this build)");
			}

			return found_all;
		}

		static std::optional<handle> find(const signature& entry, range region, std::uint32_t sigset_hash)
		{
			const ExeIdentity id = read_exe_identity(region);
			const auto cached = load();
			if (!cached || id.image_size == 0 || cached->sigset_hash != sigset_hash || cached->id != id)
				return std::nullopt;

			const auto it = cached->rvas.find(name_hash(entry));
			if (it == cached->rvas.end() || it->second >= id.image_size)
				return std::nullopt;

			return handle(region.begin().as<std::uintptr_t>() + it->second);
		}

		static std::optional<handle> scan_one(const signature& entry, range region)
		{
			if (!entry.anchored())
				return region.scan(entry.m_ida.c_str());

			const auto target = locate(entry.m_anchor);
			if (!target)
			{
				LOG_INFO("Failed to find '{}': {}", entry.m_name.c_str(), target.error());
				return std::nullopt;
			}
			return handle(*target);
		}

	private:
		static constexpr std::uint32_t MAGIC = 0x434C4D52; // "RMLC"
		static constexpr std::uint32_t FORMAT = 2;
		static constexpr std::uint32_t MAX_ENTRIES = 100000;

		struct ExeIdentity
		{
			std::array<std::uint8_t, 16> stamp{};
			std::uint32_t image_size{};

			bool operator==(const ExeIdentity&) const = default;
		};

		struct CacheData
		{
			ExeIdentity id{};
			std::uint32_t sigset_hash{};
			std::unordered_map<std::uint32_t, std::uint32_t> rvas;
		};

		static ExeIdentity read_exe_identity(const range region) noexcept
		{
			ExeIdentity id{};
			const std::uintptr_t base = region.begin().as<std::uintptr_t>();
			if (!base) return id;

#if defined(RML_WINDOWS)
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE) return id;

			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE) return id;

			std::memcpy(id.stamp.data(), &nt->FileHeader.TimeDateStamp, sizeof(nt->FileHeader.TimeDateStamp));
			id.image_size = nt->OptionalHeader.SizeOfImage;
#elif defined(RML_MACOS)
			const auto header = reinterpret_cast<const mach_header_64*>(base);
			if (header->magic != MH_MAGIC_64)
				return id;

			const auto* command = reinterpret_cast<const load_command*>(header + 1);
			for (std::uint32_t i = 0; i < header->ncmds; ++i)
			{
				if (command->cmd == LC_UUID)
				{
					std::memcpy(id.stamp.data(), reinterpret_cast<const uuid_command*>(command)->uuid, id.stamp.size());
					id.image_size = static_cast<std::uint32_t>(region.size());
					break;
				}
				command = reinterpret_cast<const load_command*>(reinterpret_cast<const std::byte*>(command) + command->cmdsize);
			}
#endif
			return id;
		}

		static std::filesystem::path cache_path()
		{
			return filesystem::directory::get_mod_loader_directory() / "cache" / "signatures.bin";
		}

		static std::uint32_t name_hash(const signature& entry) noexcept
		{
			return utils::fnv1a_32(entry.m_name.view());
		}

		static std::optional<CacheData> load() noexcept
		{
			try
			{
				std::ifstream file(cache_path(), std::ios::binary);
				if (!file) return std::nullopt;

				const auto read = [&](auto& value) {
					file.read(reinterpret_cast<char*>(&value), sizeof(value));
				};

				std::uint32_t magic{}, format{}, count{};
				read(magic);
				read(format);
				if (!file || magic != MAGIC || format != FORMAT) return std::nullopt;

				CacheData data{};
				read(data.sigset_hash);
				read(data.id.stamp);
				read(data.id.image_size);
				read(count);
				if (!file || count > MAX_ENTRIES) return std::nullopt;

				data.rvas.reserve(count);
				for (std::uint32_t i = 0; i < count; ++i)
				{
					std::uint32_t nh{}, rva{};
					read(nh);
					read(rva);
					if (!file) return std::nullopt;
					data.rvas[nh] = rva;
				}
				return data;
			}
			catch (const std::exception& e)
			{
				LOG_WARN("Failed to read signature cache: {}", e.what());
				return std::nullopt;
			}
		}

		static void save(const CacheData& data) noexcept
		{
			try
			{
				const auto path = cache_path();
				std::error_code ec;
				std::filesystem::create_directories(path.parent_path(), ec);

				std::ofstream file(path, std::ios::binary | std::ios::trunc);
				if (!file)
				{
					LOG_WARN("Could not open signature cache for writing: {}", path.string());
					return;
				}

				const auto write = [&](auto value) {
					file.write(reinterpret_cast<const char*>(&value), sizeof(value));
				};

				write(MAGIC);
				write(FORMAT);
				write(data.sigset_hash);
				write(data.id.stamp);
				write(data.id.image_size);
				write(static_cast<std::uint32_t>(data.rvas.size()));
				for (const auto& [nh, rva] : data.rvas)
				{
					write(nh);
					write(rva);
				}
			}
			catch (const std::exception& e)
			{
				LOG_WARN("Failed to write signature cache: {}", e.what());
			}
		}

		static bool apply(std::span<const signature> entries, const std::uintptr_t base, const ExeIdentity& id,
		                  const CacheData& cached)
		{
			for (const auto& entry : entries)
			{
				const auto it = cached.rvas.find(name_hash(entry));
				if (it == cached.rvas.end() || it->second >= id.image_size)
					return false;

				if (entry.m_on_signature_found)
					entry.m_on_signature_found(handle(base + it->second));
			}
			return true;
		}

		static bool scan(std::span<const signature> entries, range region, const std::uintptr_t base,
		                 std::unordered_map<std::uint32_t, std::uint32_t>& out_rvas)
		{
			std::mutex mutex;
			std::unordered_map<std::string_view, void*> resolved;

			std::vector<std::string_view> texts;
			for (const auto& entry : entries)
			{
				if (*entry.m_anchor.m_text.c_str())
					texts.emplace_back(entry.m_anchor.m_text.c_str());
			}
			auto references = std::async(std::launch::async, [&texts] {
				return functions_referencing_strings(texts);
			});

			const auto record = [&](const signature& entry, const handle result) {
				const auto rva = static_cast<std::uint32_t>(result.as<std::uintptr_t>() - base);
				if (entry.m_on_signature_found)
					entry.m_on_signature_found(result);
				out_rvas[name_hash(entry)] = rva;
				resolved.emplace(entry.m_name.c_str(), result.as<void*>());
				LOG_INFO("Found '{}' RobloxStudioBeta.exe+0x{:X}", entry.m_name.c_str(), rva);
			};

			std::vector<std::future<bool>> futures;
			futures.reserve(entries.size());

			for (const auto& entry : entries)
			{
				if (entry.anchored())
					continue;

				futures.emplace_back(std::async(std::launch::async, [&, entry]() -> bool {
					const auto result = region.scan(entry.m_ida.c_str());
					if (!result.has_value())
					{
						LOG_INFO("Failed to find '{}'.", entry.m_name.c_str());
						return false;
					}

					std::lock_guard lock(mutex);
					record(entry, result.value());
					return true;
				}));
			}

			bool found_all = true;
			for (auto& future : futures)
			{
				future.wait();
				if (!future.get()) found_all = false;
			}

			if (!resolve_anchored(entries, texts, references.get(), resolved, record))
				found_all = false;
			return found_all;
		}

		static bool resolve_anchored(std::span<const signature> entries, const std::vector<std::string_view>& texts, const std::vector<std::vector<AnchoredFunction>>& referencing, const std::unordered_map<std::string_view, void*>& resolved, const auto& record)
		{
			std::vector<const signature*> pending;
			for (const auto& entry : entries)
			{
				if (entry.anchored())
					pending.push_back(&entry);
			}

			bool found_all = true;
			for (bool progress = true; progress && !pending.empty();)
			{
				progress = false;
				for (auto it = pending.begin(); it != pending.end();)
				{
					const auto& entry = **it;
					const auto& path = entry.m_anchor;
					std::expected<void*, std::string> origin;

					if (*path.m_text.c_str())
					{
						const auto& functions =
						    referencing[std::ranges::find(texts, std::string_view(path.m_text.c_str())) - texts.begin()];
						if (functions.size() == 1)
							origin = functions.front().start;
						else
							origin = std::unexpected(
							    std::format("{} functions reference \"{}\"", functions.size(), path.m_text.c_str()));
					}
					else if (const auto found = resolved.find(path.m_origin.c_str()); found != resolved.end())
					{
						origin = found->second;
					}
					else
					{
						++it;
						continue;
					}

					const auto target = origin.and_then([&](void* start) {
						return follow(path, start);
					});
					if (target)
					{
						record(entry, handle(*target));
					}
					else
					{
						LOG_INFO("Failed to find '{}': {}", entry.m_name.c_str(), target.error());
						found_all = false;
					}

					it = pending.erase(it);
					progress = true;
				}
			}

			for (const auto* entry : pending)
			{
				LOG_INFO("Failed to find '{}': '{}' was not found", entry->m_name.c_str(), entry->m_anchor.m_origin.c_str());
				found_all = false;
			}
			return found_all;
		}
	};

	bool run_batch_cached(const std::span<const signature> entries, range region, const std::uint32_t sigset_hash)
	{
		return SignatureCache::run(entries, region, sigset_hash);
	}

	std::optional<handle> find_cached_signature(const signature& entry, range region, const std::uint32_t sigset_hash)
	{
		return SignatureCache::find(entry, region, sigset_hash);
	}

	std::optional<handle> scan_signature(const signature& entry, range region)
	{
		return SignatureCache::scan_one(entry, region);
	}
}
