#include "RobloxModLoader/qt/qstring.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/qt/qarray_data.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"
#include "RobloxModLoader/util/string.hpp"


namespace rml::qt
{
	QString::QString(const std::string_view utf8)
	{
		static void* const from_utf8 = detail::core_export({
		    "QString::fromUtf8(char const*, int)",
		    "QString::fromUtf8_helper(char const*, int)",
		});

		memory::call_returning(from_utf8, m_storage, utf8.data(), static_cast<int>(utf8.size()));
	}

	QString::QString(const char* utf8) :
	    QString(std::string_view(utf8 ? utf8 : ""))
	{
	}

	QString::~QString()
	{
		detail::destroy_qstring(m_storage);
	}

	std::string QString::to_utf8() const
	{
		if (!m_storage)
			return {};
		
		static const auto utf16 = detail::core<const unsigned short* (*)(const void*)>("QString::utf16() const");
		if (!utf16)
			return {};

		const auto* const utf16_data = reinterpret_cast<const char16_t*>(utf16(&m_storage));
		return utf16_data ? utils::from_utf16(utf16_data) : std::string{};
	}
}
