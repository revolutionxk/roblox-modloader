#include "RobloxModLoader/qt/qmenu.hpp"

#include "RobloxModLoader/qt/qicon.hpp"
#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QAction* QMenu::addAction(const QString& text)
	{
		static const auto fn = detail::widgets<void* (*)(void*, const void*)>("QMenu::addAction(QString const&)");
		return fn ? static_cast<QAction*>(fn(this, &text)) : nullptr;
	}

	QMenu* QMenu::addMenu(const QString& title)
	{
		static const auto fn = detail::widgets<void* (*)(void*, const void*)>("QMenu::addMenu(QString const&)");
		return fn ? static_cast<QMenu*>(fn(this, &title)) : nullptr;
	}

	QAction* QMenu::addSeparator()
	{
		static const auto fn = detail::widgets<void* (*)(void*)>("QMenu::addSeparator()");
		return fn ? static_cast<QAction*>(fn(this)) : nullptr;
	}

	void QMenu::clear()
	{
		static const auto fn = detail::widgets<void (*)(void*)>("QMenu::clear()");
		if (fn)
			fn(this);
	}

	void QMenu::setIcon(const QIcon& icon)
	{
		static const auto fn = detail::widgets<void (*)(void*, const void*)>("QMenu::setIcon(QIcon const&)");
		if (fn)
			fn(this, &icon);
	}

	QAction* QMenu::menuAction() const
	{
		static const auto fn = detail::widgets<void* (*)(const void*)>("QMenu::menuAction() const");
		return fn ? static_cast<QAction*>(fn(this)) : nullptr;
	}
}
