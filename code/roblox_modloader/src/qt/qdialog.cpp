#include "RobloxModLoader/qt/qdialog.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QDialog* QDialog::create(QWidget* parent)
	{
		static const auto construct = detail::widgets<void* (*)(void*, void*, int)>("QDialog::QDialog(QWidget*, QFlags<Qt::WindowType>)");
		return detail::heap_construct<QDialog>(sizeof(QDialog), construct, parent, 0);
	}

	QtOwned<QDialog> QDialog::create_owned()
	{
		return QtOwned<QDialog>(create(nullptr));
	}

	void QDialog::destroy(QDialog* dialog)
	{
		static const auto dtor = detail::widgets<void (*)(void*)>("QDialog::~QDialog()");
		detail::heap_destroy(dtor, dialog);
	}

	void QDialog::setModal(const bool modal)
	{
		static const auto fn = detail::widgets<void (*)(void*, bool)>("QDialog::setModal(bool)");
		if (fn)
			fn(this, modal);
	}

	int QDialog::exec()
	{
		static const auto fn = detail::widgets<int (*)(void*)>("QDialog::exec()");
		return fn ? fn(this) : -1;
	}
}
