#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <vector>

namespace RBX
{
	template<typename T>
	class ArrayView
	{
	public:
		const T* data{};
		std::size_t size{};

		constexpr ArrayView() noexcept = default;

		constexpr ArrayView(const T* items, const std::size_t count) noexcept :
		    data(items),
		    size(count)
		{
		}

		ArrayView(const std::vector<T>& items) noexcept :
		    data(items.data()),
		    size(items.size())
		{
		}

		[[nodiscard]] constexpr const T* begin() const noexcept
		{
			return data;
		}

		[[nodiscard]] constexpr const T* end() const noexcept
		{
			return data + size;
		}
	};

	RML_ASSERT_SIZE(ArrayView<int>, 2 * sizeof(void*));
}
