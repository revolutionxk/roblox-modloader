#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QPoint
	{
	public:
		int xp{};
		int yp{};

		constexpr QPoint() noexcept = default;

		constexpr QPoint(const int x, const int y) noexcept :
		    xp(x),
		    yp(y)
		{
		}
	};

	class QPointF
	{
	public:
		double xp{};
		double yp{};

		constexpr QPointF() noexcept = default;

		constexpr QPointF(const double x, const double y) noexcept :
		    xp(x),
		    yp(y)
		{
		}
	};

	RML_ASSERT_SIZE(QPoint, 8);
	RML_ASSERT_SIZE(QPointF, 16);
}
