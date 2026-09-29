#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QRect
	{
	public:
		int x1{0};
		int y1{0};
		int x2{-1};
		int y2{-1};

		constexpr QRect() noexcept = default;

		constexpr QRect(const int x, const int y, const int width, const int height) noexcept :
		    x1(x),
		    y1(y),
		    x2(x + width - 1),
		    y2(y + height - 1)
		{
		}

		[[nodiscard]] constexpr int width() const noexcept
		{
			return x2 - x1 + 1;
		}

		[[nodiscard]] constexpr int height() const noexcept
		{
			return y2 - y1 + 1;
		}
	};

	class QRectF
	{
	public:
		double xp{};
		double yp{};
		double w{};
		double h{};

		constexpr QRectF() noexcept = default;

		constexpr QRectF(const double x, const double y, const double width, const double height) noexcept :
		    xp(x),
		    yp(y),
		    w(width),
		    h(height)
		{
		}

		constexpr explicit QRectF(const QRect& rect) noexcept :
		    xp(rect.x1),
		    yp(rect.y1),
		    w(rect.width()),
		    h(rect.height())
		{
		}
	};

	RML_ASSERT_SIZE(QRect, 16);
	RML_ASSERT_SIZE(QRectF, 32);
}
