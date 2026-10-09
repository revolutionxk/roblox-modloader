#include "RobloxModLoader/qt/qfiledialog.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	std::string QFileDialog::get_open_file_name(QWidget* parent, const std::string_view caption, const std::string_view dir, const std::string_view filter)
	{
		static void* const fn = detail::widgets_export("QFileDialog::getOpenFileName(QWidget*, QString const&, QString const&, QString const&, QString*, QFlags<QFileDialog::Option>)");
		if (!fn)
			return {};

		const QString caption_str(caption);
		const QString dir_str(dir);
		const QString filter_str(filter);

		QString result;
		memory::call_returning(fn, result, static_cast<void*>(parent), &caption_str, &dir_str, &filter_str, static_cast<QString*>(nullptr), 0);
		return result.to_utf8();
	}
}
