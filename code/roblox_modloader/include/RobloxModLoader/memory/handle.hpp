#pragma once
#include "RobloxModLoader/memory/instruction.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace rml::memory
{
	class RML_EXPORT handle
	{
	public:
		handle(void* ptr = nullptr);

		explicit handle(std::uintptr_t ptr);

		template<typename T>
		std::enable_if_t<std::is_pointer_v<T>, T> as() const;

		template<typename T>
		std::enable_if_t<std::is_lvalue_reference_v<T>, T> as() const;

		template<typename T>
		std::enable_if_t<std::is_same_v<T, std::uintptr_t>, T> as() const;

		template<typename T>
		handle add(T offset) const;

		template<typename T>
		handle sub(T offset) const;

		handle rip() const;

		handle adrp() const;

		handle bl() const;

		explicit operator bool();

		friend bool operator==(handle a, handle b);

		friend bool operator!=(handle a, handle b);

	private:
		void* ptr;
	};

	inline handle::handle(void* ptr) :
	    ptr(ptr)
	{
	}

	inline handle::handle(std::uintptr_t ptr) :
	    ptr(reinterpret_cast<void*>(ptr))
	{
	}

	template<typename T>
	inline std::enable_if_t<std::is_pointer_v<T>, T> handle::as() const
	{
		if constexpr (std::is_function_v<std::remove_pointer_t<T>>)
			return reinterpret_cast<T>(ptr);
		else
			return static_cast<T>(ptr);
	}

	template<typename T>
	inline std::enable_if_t<std::is_lvalue_reference_v<T>, T> handle::as() const
	{
		return *static_cast<std::add_pointer_t<std::remove_reference_t<T>>>(ptr);
	}

	template<typename T>
	inline std::enable_if_t<std::is_same_v<T, std::uintptr_t>, T> handle::as() const
	{
		return reinterpret_cast<std::uintptr_t>(ptr);
	}

	template<typename T>
	inline handle handle::add(T offset) const
	{
		return handle(as<std::uintptr_t>() + offset);
	}

	template<typename T>
	inline handle handle::sub(T offset) const
	{
		return handle(as<std::uintptr_t>() - offset);
	}

	inline handle handle::rip() const
	{
		return handle(instruction::x64_relative_target(as<std::uintptr_t>() + 4, as<std::int32_t&>()));
	}

	inline handle handle::adrp() const
	{
		const auto* const instructions = as<const std::uint32_t*>();
		const auto page = instruction::arm64_adrp_page(as<std::uintptr_t>(), instructions[0]);
		return handle(page + instruction::arm64_page_offset(instructions[1]).value_or(0));
	}

	inline handle handle::bl() const
	{
		return handle(instruction::arm64_branch_target(as<std::uintptr_t>(), as<const std::uint32_t&>()));
	}

	inline bool operator==(handle a, handle b)
	{
		return a.ptr == b.ptr;
	}

	inline bool operator!=(handle a, handle b)
	{
		return a.ptr != b.ptr;
	}

	inline handle::operator bool()
	{
		return ptr != nullptr;
	}
}
