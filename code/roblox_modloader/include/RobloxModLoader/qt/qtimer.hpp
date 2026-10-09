#pragma once

#include "RobloxModLoader/qt/qobject.hpp"
#include "RobloxModLoader/qt/qt_owned.hpp"

#include <cstdint>
#include <functional>

namespace rml::qt
{
	class RML_EXPORT QTimer : public QObject
	{
	public:
		[[nodiscard]] static QTimer* create(QObject* parent);
		[[nodiscard]] static QtOwned<QTimer> create_owned();

		static void destroy(QTimer* timer);

		void setInterval(int milliseconds);
		void setSingleShot(bool single_shot);

		void start();
		void stop();

		void on_timeout(std::function<void()> handler) const;

		int id;
		int inter;
		int del;
		std::uint32_t single : 1;
		std::uint32_t nulltimer : 1;
		std::uint32_t type : 2;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QTimer, id, 2 * sizeof(void*));
	RML_ASSERT_SIZE(QTimer, 2 * sizeof(void*) + 16);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
