#include "RobloxModLoader/qt/qcheckbox.hpp"

#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QCheckBox* QCheckBox::create(const std::string_view text, QWidget* parent)
	{
		static const auto construct = detail::widgets<void* (*)(void*, const void*, void*)>("QCheckBox::QCheckBox(QString const&, QWidget*)");
		if (!construct)
			return nullptr;

		const QString label(text);
		return detail::heap_construct<QCheckBox>(sizeof(QCheckBox), construct, &label, parent);
	}

	QtOwned<QCheckBox> QCheckBox::create_owned(const std::string_view text)
	{
		return QtOwned<QCheckBox>(create(text, nullptr));
	}

	void QCheckBox::destroy(QCheckBox* box)
	{
		static const auto dtor = detail::widgets<void (*)(void*)>("QCheckBox::~QCheckBox()");
		detail::heap_destroy(dtor, box);
	}
}
