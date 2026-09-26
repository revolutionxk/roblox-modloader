#pragma once

#include "RobloxModLoader/platform/abi.hpp"

#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

namespace rml::memory
{
	template<typename T>
	concept IndirectlyReturnable = std::is_trivially_copyable_v<T>;

	template<typename... T>
	concept ForeignArguments = (std::is_trivially_copyable_v<T> && ...);

	namespace detail
	{
		template<std::size_t Size>
		struct IndirectResult
		{
			alignas(std::max_align_t) unsigned char m_storage[Size];

			IndirectResult() = default;
			IndirectResult(const IndirectResult&) = default;

			~IndirectResult()
			{
			}
		};

		static_assert(!std::is_trivially_copyable_v<IndirectResult<sizeof(void*)>>,
		    "IndirectResult must stay non-trivially-copyable or the ABI stops returning it indirectly");
	}

	template<IndirectlyReturnable Result, typename... Args>
	    requires ForeignArguments<Args...>
	void call_returning(void* fn, Result& result, Args... args)
	{
		if (!fn)
			return;

		if constexpr (platform::abi::returns_via_hidden_pointer)
		{
			reinterpret_cast<void (*)(void*, Args...)>(fn)(std::addressof(result), args...);
		}
		else
		{
			using Slot = detail::IndirectResult<sizeof(Result)>;

			const Slot value = reinterpret_cast<Slot (*)(Args...)>(fn)(args...);
			std::memcpy(std::addressof(result), value.m_storage, sizeof(Result));
		}
	}

	template<IndirectlyReturnable Result, typename Self, typename... Args>
	    requires ForeignArguments<Self, Args...>
	void call_returning_member(void* fn, Result& result, Self self, Args... args)
	{
		if (!fn)
			return;

		if constexpr (platform::abi::returns_via_hidden_pointer)
		{
			reinterpret_cast<void (*)(Self, void*, Args...)>(fn)(self, std::addressof(result), args...);
		}
		else
		{
			using Slot = detail::IndirectResult<sizeof(Result)>;

			const Slot value = reinterpret_cast<Slot (*)(Self, Args...)>(fn)(self, args...);
			std::memcpy(std::addressof(result), value.m_storage, sizeof(Result));
		}
	}
}
