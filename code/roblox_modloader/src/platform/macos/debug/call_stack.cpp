#include "RobloxModLoader/platform/debug/call_stack.hpp"

#include "RobloxModLoader/memory/string_anchor.hpp"

#include <algorithm>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <ptrauth.h>
#include <utility>

namespace rml::platform
{
	static constexpr std::uintptr_t frame_record_size = 2 * sizeof(std::uintptr_t);
	static constexpr std::uint32_t frame_setup_mask = 0xFF8003FF;
	static constexpr std::uint32_t frame_setup = 0x910003FD;
	static constexpr std::size_t prologue_window = 8;

	static std::pair<std::uintptr_t, std::uintptr_t> stack_region(const std::uintptr_t sp) noexcept
	{
		thread_local std::pair<std::uintptr_t, std::uintptr_t> cached{};
		if (sp >= cached.first && sp < cached.second)
			return cached;

		mach_vm_address_t address = sp;
		mach_vm_size_t size = 0;
		vm_region_basic_info_data_64_t info{};
		mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
		mach_port_t object = MACH_PORT_NULL;
		if (mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS || address > sp)
			return {sp, sp};

		cached = {static_cast<std::uintptr_t>(address), static_cast<std::uintptr_t>(address + size)};
		return cached;
	}

	[[gnu::noinline]] std::size_t capture_return_addresses(const std::span<std::uintptr_t> frames) noexcept
	{
		auto fp = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
		const auto [lower, upper] = stack_region(fp);
		auto floor = lower;
		std::size_t count = 0;

		while (count < frames.size() && fp >= floor && fp + frame_record_size <= upper && (fp & (sizeof(std::uintptr_t) - 1)) == 0)
		{
			const auto* record = reinterpret_cast<const std::uintptr_t*>(fp);
			const auto next = record[0];
			const auto ret = reinterpret_cast<std::uintptr_t>(ptrauth_strip(reinterpret_cast<void*>(record[1]), ptrauth_key_return_address));
			if (ret == 0)
				break;

			frames[count++] = ret;
			if (next <= fp)
				break;
			floor = fp + frame_record_size;
			fp = next;
		}
		return count;
	}

	bool frame_established(const void* pc) noexcept
	{
		const auto function = memory::function_containing(pc);
		if (!function)
			return true;

		const auto start = reinterpret_cast<std::uintptr_t>(function->start);
		const auto* code = static_cast<const std::uint32_t*>(function->start);
		const auto window = std::min<std::size_t>(prologue_window, function->size / sizeof(std::uint32_t));
		for (std::size_t i = 0; i < window; ++i)
		{
			if ((code[i] & frame_setup_mask) == frame_setup)
				return reinterpret_cast<std::uintptr_t>(pc) > start + i * sizeof(std::uint32_t);
		}
		return false;
	}

	std::uint64_t current_thread_id() noexcept
	{
		std::uint64_t thread = 0;
		pthread_threadid_np(nullptr, &thread);
		return thread;
	}
}
