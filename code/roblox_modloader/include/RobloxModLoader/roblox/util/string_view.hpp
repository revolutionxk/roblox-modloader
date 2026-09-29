#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace RBX
{
	class StringView
	{
	public:
		const char* data{};
		std::size_t size{};

		constexpr StringView() noexcept = default;

		constexpr StringView(const char* text, const std::size_t length) noexcept :
		    data(text),
		    size(length)
		{
		}

		constexpr StringView(const std::string_view text) noexcept :
		    data(text.data()),
		    size(text.size())
		{
		}

		constexpr StringView(const char* text) noexcept :
		    StringView(std::string_view(text))
		{
		}

		[[nodiscard]] constexpr std::string_view view() const noexcept
		{
			return {data, size};
		}
	};

	static_assert(std::is_trivially_copyable_v<StringView>);
	RML_ASSERT_SIZE(StringView, 2 * sizeof(void*));
}
