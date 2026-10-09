#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QIconPrivate;
	class QString;

	class RML_EXPORT QIcon
	{
	public:
		QIconPrivate* d{};

		explicit QIcon(const QString& path);
		~QIcon();

		QIcon(const QIcon&) = delete;
		QIcon& operator=(const QIcon&) = delete;
	};

	RML_ASSERT_SIZE(QIcon, sizeof(void*));
}
