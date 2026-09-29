#pragma once

#include "RobloxModLoader/qt/qarray_data.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace rml::qt
{
	class RML_EXPORT QString
	{
	public:
		QString() = default;

		QString(std::string_view utf8);

		QString(const char* utf8);

		~QString();

		QString(const QString&) = delete;
		QString(QString&& other) noexcept :
		    d(std::exchange(other.d, nullptr))
		{
		}

		QString& operator=(const QString&) = delete;

		[[nodiscard]] std::string to_utf8() const;

		QArrayData* d{};
	};
}
