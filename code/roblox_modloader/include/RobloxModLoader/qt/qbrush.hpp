#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QBrushData;
	class QPixmap;

	class RML_EXPORT QBrush
	{
	public:
		QBrushData* d{};

		explicit QBrush(const QPixmap& texture);
		~QBrush();

		QBrush(const QBrush&) = delete;
		QBrush& operator=(const QBrush&) = delete;
	};

	RML_ASSERT_SIZE(QBrush, sizeof(void*));
}
