#include "RobloxModLoader/qt/qicon.hpp"

#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QIcon::QIcon(const QString& path)
	{
		static const auto ctor = detail::gui<void (*)(QIcon*, const QString*)>("QIcon::QIcon(QString const&)");
		if (ctor)
			ctor(this, &path);
	}

	QIcon::~QIcon()
	{
		static const auto dtor = detail::gui<void (*)(QIcon*)>("QIcon::~QIcon()");
		if (d && dtor)
			dtor(this);
	}
}
