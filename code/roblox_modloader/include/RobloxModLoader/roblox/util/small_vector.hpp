#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <type_traits>

namespace RBX
{
	template<typename T>
	class SmallVectorImpl
	{
	public:
		T* begin_x;
		std::uint32_t size;
		std::uint32_t capacity;

		[[nodiscard]] T* begin() const noexcept
		{
			return begin_x;
		}

		[[nodiscard]] T* end() const noexcept
		{
			return begin_x + size;
		}

		[[nodiscard]] std::span<T> items() const noexcept
		{
			return {begin_x, size};
		}
	};

	template<typename T, unsigned N>
	class SmallVector : public SmallVectorImpl<T>
	{
		static_assert(std::is_trivially_destructible_v<T>);

	public:
		alignas(T) std::byte inline_storage[sizeof(T) * N];

		SmallVector() noexcept :
		    SmallVectorImpl<T>{inline_items(), 0, N}
		{
		}

		explicit SmallVector(const std::span<const T> source) :
		    SmallVector()
		{
			if (source.size() > N)
			{
				this->begin_x = static_cast<T*>(::operator new(source.size() * sizeof(T)));
				this->capacity = static_cast<std::uint32_t>(source.size());
			}
			std::ranges::copy(source, this->begin_x);
			this->size = static_cast<std::uint32_t>(source.size());
		}

		SmallVector(const SmallVector&) = delete;
		SmallVector& operator=(const SmallVector&) = delete;

		~SmallVector()
		{
			if (this->begin_x != inline_items())
				::operator delete(this->begin_x);
		}

	private:
		T* inline_items() noexcept
		{
			return reinterpret_cast<T*>(inline_storage);
		}
	};

	namespace layout
	{
		using SmallVectorOfFour = SmallVector<float, 4>;
	}

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(SmallVectorImpl<float>, size, 0x8);
	RML_ASSERT_OFFSET(SmallVectorImpl<float>, capacity, 0xC);
	RML_ASSERT_OFFSET(layout::SmallVectorOfFour, inline_storage, 0x10);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
