#include "RobloxModLoader/qt/qmenubar.hpp"

#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QMenu* QMenuBar::addMenu(const QString& title)
	{
		static const auto fn = detail::widgets<void* (*)(void*, const void*)>("QMenuBar::addMenu(QString const&)");
		return fn ? static_cast<QMenu*>(fn(this, &title)) : nullptr;
	}
}
