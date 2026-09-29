#include "RobloxModLoader/qt/qapplication.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/qt/qarray_data.hpp"
#include "RobloxModLoader/qt/qlist.hpp"
#include "RobloxModLoader/qt/qbytearray.hpp"
#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"
#include "RobloxModLoader/qt/qwidget.hpp"

namespace rml::qt
{
	QApplication* QApplication::instance()
	{
		static void* const fn = detail::core_export_optional("QCoreApplication::instance()");
		if (fn)
			return static_cast<QApplication*>(reinterpret_cast<void* (*)()>(fn)());

		static auto* const self = static_cast<void* const*>(detail::core_export("QCoreApplication::self"));
		return self ? static_cast<QApplication*>(*self) : nullptr;
	}

	void QApplication::set_style_sheet(const std::string_view css)
	{
		static const auto fn = detail::widgets<void (*)(void*, const void*)>("QApplication::setStyleSheet(QString const&)");
		if (!fn)
			return;
		const QString style(css);
		fn(this, &style);
	}

	std::string QApplication::style_sheet() const
	{
		static void* const get = detail::widgets_export("QApplication::styleSheet() const");
		static void* const to_utf8 = detail::core_export("QString::toUtf8() const");
		
		static const auto const_data = detail::core_optional<const char* (*)(const QByteArray* self)>("QByteArray::constData() const");

		if (!get || !to_utf8)
			return {};

		QString text;
		QByteArray bytes;
		memory::call_returning_member(get, text, this);
		memory::call_returning_member(to_utf8, bytes, static_cast<const QString*>(&text));

		const char* const utf8 = const_data ? const_data(&bytes) : bytes.d ? bytes.d->data() : nullptr;
		return utf8 ? utf8 : "";
	}

	std::vector<QWidget*> QApplication::all_widgets()
	{
		static void* const fn = detail::widgets_export("QApplication::allWidgets()");

		std::vector<QWidget*> widgets;
		if (!fn)
			return widgets;

		QList<QWidget*> list;
		memory::call_returning(fn, list);

		widgets.reserve(static_cast<std::size_t>(list.size()));
		for (QWidget* widget : list)
			widgets.push_back(widget);

		return widgets;
	}
}
