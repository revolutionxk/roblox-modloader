#include "RobloxModLoader/qt/qbrush.hpp"

#include "RobloxModLoader/qt/qpixmap.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QBrush::QBrush(const QPixmap& texture)
	{
		static const auto ctor = detail::gui<void (*)(QBrush*, const QPixmap*)>("QBrush::QBrush(QPixmap const&)");
		if (ctor)
			ctor(this, &texture);
	}

	QBrush::~QBrush()
	{
		static const auto dtor = detail::gui<void (*)(QBrush*)>("QBrush::~QBrush()");
		if (d && dtor)
			dtor(this);
	}
}
