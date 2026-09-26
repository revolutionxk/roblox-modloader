#pragma once

#include "RobloxModLoader/internal/platform.hpp"

#include <cstdint>
#include <cstring>

namespace rml::memory
{
	struct MemberFunctionPointer
	{
		std::uintptr_t function;
		std::intptr_t adjustment;
	};

	template<class T = void*>
	[[nodiscard]] inline T* vtable_of(const void* object) noexcept
	{
		return *static_cast<T* const*>(object);
	}

	template<typename Pmf>
	[[nodiscard]] std::size_t virtual_index(Pmf pmf)
	{
#if defined(RML_WINDOWS)
		const auto* thunk = *reinterpret_cast<const unsigned char* const*>(&pmf);
		if (!thunk || thunk[0] != 0x48 || thunk[1] != 0x8B || thunk[2] != 0x01)
			return static_cast<std::size_t>(-1);

		const unsigned char* jmp = thunk + 3;
		if (jmp[0] != 0xFF)
			return static_cast<std::size_t>(-1);

		switch (jmp[1])
		{
		case 0x20: return 0;
		case 0x60: return jmp[2] / sizeof(void*);
		case 0xA0: return *reinterpret_cast<const std::uint32_t*>(jmp + 2) / sizeof(void*);
		default: return static_cast<std::size_t>(-1);
		}
#else
		static_assert(sizeof(Pmf) >= sizeof(MemberFunctionPointer),
		    "member pointer is smaller than the Itanium representation it is decoded as");

		MemberFunctionPointer member{};
		std::memcpy(&member, &pmf, sizeof(member));

		constexpr std::intptr_t virtual_flag = 1;
		if ((member.adjustment & virtual_flag) == 0)
			return static_cast<std::size_t>(-1);

		return member.function / sizeof(void*);
#endif
	}
}
