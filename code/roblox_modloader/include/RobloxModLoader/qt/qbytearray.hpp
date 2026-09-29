#pragma once

#include "RobloxModLoader/qt/qarray_data.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <utility>

namespace rml::qt
{
	class RML_EXPORT QByteArray
	{
	public:
		QArrayData* d{};

		QByteArray() noexcept = default;
		~QByteArray();

		QByteArray(const QByteArray&) = delete;
		QByteArray& operator=(const QByteArray&) = delete;
	};

	RML_ASSERT_SIZE(QByteArray, sizeof(void*));
}
