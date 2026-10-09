#include "RobloxModLoader/qt/qlabel.hpp"

#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QLabel* QLabel::create(const std::string_view text, QWidget* parent)
	{
		static const auto construct = detail::widgets<void* (*)(void*, const void*, void*, int)>("QLabel::QLabel(QString const&, QWidget*, QFlags<Qt::WindowType>)");
		if (!construct)
			return nullptr;

		const QString label(text);
		return detail::heap_construct<QLabel>(sizeof(QLabel), construct, &label, parent, 0);
	}

	QtOwned<QLabel> QLabel::create_owned(const std::string_view text)
	{
		return QtOwned<QLabel>(create(text, nullptr));
	}

	void QLabel::destroy(QLabel* label)
	{
		static const auto dtor = detail::widgets<void (*)(void*)>("QLabel::~QLabel()");
		detail::heap_destroy(dtor, label);
	}

	void QLabel::setText(const std::string_view text)
	{
		static const auto fn = detail::widgets<void (*)(void*, const void*)>("QLabel::setText(QString const&)");
		if (fn)
		{
			const QString value(text);
			fn(this, &value);
		}
	}

	void QLabel::setAlignment(const int alignment)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QLabel::setAlignment(QFlags<Qt::AlignmentFlag>)");
		if (fn)
			fn(this, alignment);
	}
}
