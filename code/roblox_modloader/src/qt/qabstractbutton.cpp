#include "RobloxModLoader/qt/qabstractbutton.hpp"

#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"
#include "qt_connect.hpp"

#include <utility>

namespace rml::qt
{
	void QAbstractButton::setText(const std::string_view text)
	{
		static const auto fn = detail::widgets<void (*)(void*, const void*)>("QAbstractButton::setText(QString const&)");
		if (fn)
		{
			const QString value(text);
			fn(this, &value);
		}
	}

	void QAbstractButton::setChecked(const bool checked)
	{
		static const auto fn = detail::widgets<void (*)(void*, bool)>("QAbstractButton::setChecked(bool)");
		if (fn)
			fn(this, checked);
	}

	bool QAbstractButton::isChecked() const
	{
		static const auto fn = detail::widgets<bool (*)(const void*)>("QAbstractButton::isChecked() const");
		return fn && fn(this);
	}

	void QAbstractButton::on_clicked(std::function<void()> handler) const
	{
		static void* const signal = detail::widgets_export("QAbstractButton::clicked(bool)");
		static const void* const meta = detail::widgets_export("QAbstractButton::staticMetaObject");
		detail::connect_function(this, signal, meta, [handler = std::move(handler)](void**) {
			if (handler)
				handler();
		});
	}

	void QAbstractButton::on_toggled(std::function<void(bool)> handler) const
	{
		static void* const signal = detail::widgets_export("QAbstractButton::toggled(bool)");
		static const void* const meta = detail::widgets_export("QAbstractButton::staticMetaObject");
		detail::connect_function(this, signal, meta, [handler = std::move(handler)](void** args) {
			if (handler && args && args[1])
				handler(*static_cast<bool*>(args[1]));
		});
	}
}
