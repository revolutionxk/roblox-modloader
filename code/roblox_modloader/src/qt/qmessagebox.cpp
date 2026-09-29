#include "RobloxModLoader/qt/qmessagebox.hpp"

#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QMessageBox* QMessageBox::create(QWidget* parent)
	{
		static const auto construct = detail::widgets<void* (*)(void*, void*)>("QMessageBox::QMessageBox(QWidget*)");
		return detail::heap_construct<QMessageBox>(sizeof(QMessageBox), construct, parent);
	}

	QtOwned<QMessageBox> QMessageBox::create_owned()
	{
		return QtOwned<QMessageBox>(create(nullptr));
	}

	void QMessageBox::destroy(QMessageBox* box)
	{
		static const auto dtor = detail::widgets<void (*)(void*)>("QMessageBox::~QMessageBox()");
		detail::heap_destroy(dtor, box);
	}

	void QMessageBox::setText(const QString& text)
	{
		static const auto fn = detail::widgets<void (*)(void*, const void*)>("QMessageBox::setText(QString const&)");
		if (fn)
			fn(this, &text);
	}

	void QMessageBox::setIcon(const Icon icon)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QMessageBox::setIcon(QMessageBox::Icon)");
		if (fn)
			fn(this, icon);
	}

	void QMessageBox::setTextFormat(const TextFormat format)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QMessageBox::setTextFormat(Qt::TextFormat)");
		if (fn)
			fn(this, static_cast<int>(format));
	}

	QPushButton* QMessageBox::addButton(const StandardButton button)
	{
		static const auto fn = detail::widgets<void* (*)(void*, int)>("QMessageBox::addButton(QMessageBox::StandardButton)");
		return fn ? static_cast<QPushButton*>(fn(this, button)) : nullptr;
	}
}
