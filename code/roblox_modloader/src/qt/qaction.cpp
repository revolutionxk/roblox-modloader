#include "RobloxModLoader/qt/qaction.hpp"

#include "RobloxModLoader/qt/qicon.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	void QAction::setCheckable(const bool checkable)
	{
		static const auto fn = detail::widgets<void (*)(void*, bool)>("QAction::setCheckable(bool)");
		if (fn)
			fn(this, checkable);
	}

	void QAction::setChecked(const bool checked)
	{
		static const auto fn = detail::widgets<void (*)(void*, bool)>("QAction::setChecked(bool)");
		if (fn)
			fn(this, checked);
	}

	bool QAction::isChecked() const
	{
		static const auto fn = detail::widgets<bool (*)(const void*)>("QAction::isChecked() const");
		return fn && fn(this);
	}

	void QAction::setIcon(const QIcon& icon)
	{
		static const auto fn = detail::widgets<void (*)(void*, const void*)>("QAction::setIcon(QIcon const&)");
		if (fn)
			fn(this, &icon);
	}

	void QAction::setMenuRole(const MenuRole role)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QAction::setMenuRole(QAction::MenuRole)");
		if (fn)
			fn(this, static_cast<int>(role));
	}

	void* QAction::activate_address()
	{
		return detail::widgets_export("QAction::activate(QAction::ActionEvent)");
	}
}
