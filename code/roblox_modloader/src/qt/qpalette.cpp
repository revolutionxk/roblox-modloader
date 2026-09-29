#include "RobloxModLoader/qt/qpalette.hpp"

#include "RobloxModLoader/qt/qbrush.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

#include <utility>

namespace rml::qt
{
	QPalette::QPalette(const QPalette* source)
	{
		static const auto copy_ctor = detail::gui<void (*)(QPalette*, const QPalette*)>("QPalette::QPalette(QPalette const&)");
		if (copy_ctor && source)
			copy_ctor(this, source);
	}

	QPalette::QPalette(QPalette&& other) noexcept :
	    d(std::exchange(other.d, nullptr)),
	    data(std::exchange(other.data, 0))
	{
	}

	QPalette& QPalette::operator=(QPalette&& other) noexcept
	{
		if (this != &other)
		{
			destroy();
			d = std::exchange(other.d, nullptr);
			data = std::exchange(other.data, 0);
		}
		return *this;
	}

	QPalette::~QPalette()
	{
		destroy();
	}

	void QPalette::destroy() noexcept
	{
		static const auto dtor = detail::gui<void (*)(QPalette*)>("QPalette::~QPalette()");
		if (d && dtor)
			dtor(this);
		d = nullptr;
	}

	void QPalette::set_brush(const ColorRole role, const QBrush& brush)
	{
		constexpr int all_color_groups = 5;

		static const auto fn = detail::gui<void (*)(QPalette*, int, int, const QBrush*)>("QPalette::setBrush(QPalette::ColorGroup, QPalette::ColorRole, QBrush const&)");
		if (fn && d)
			fn(this, all_color_groups, static_cast<int>(role), &brush);
	}
}
