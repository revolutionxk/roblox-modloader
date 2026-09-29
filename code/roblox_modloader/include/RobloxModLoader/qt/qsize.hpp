#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QSize
	{
	public:
		int wd{-1};
		int ht{-1};

		constexpr QSize() noexcept = default;

		constexpr QSize(const int width, const int height) noexcept :
		    wd(width),
		    ht(height)
		{
		}
	};

	RML_ASSERT_SIZE(QSize, 8);
}
