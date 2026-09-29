#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QChildEvent;
	class QEvent;
	class QMetaMethod;
	class QMetaObject;
	class QTimerEvent;

	class RML_EXPORT QObject
	{
	public:
		virtual const QMetaObject* metaObject() const = 0;
		virtual void* qt_metacast(const char* class_name) = 0;
		virtual int qt_metacall(int call, int id, void** arguments) = 0;
		virtual ~QObject() = default;
		virtual bool event(QEvent* event) = 0;
		virtual bool eventFilter(QObject* watched, QEvent* event) = 0;
		virtual void timerEvent(QTimerEvent* event) = 0;
		virtual void childEvent(QChildEvent* event) = 0;
		virtual void customEvent(QEvent* event) = 0;
		virtual void connectNotify(const QMetaMethod& signal) = 0;
		virtual void disconnectNotify(const QMetaMethod& signal) = 0;

		[[nodiscard]] const char* class_name() const;
		[[nodiscard]] bool inherits(const char* class_name) const;

		[[nodiscard]] void* handle()
		{
			return this;
		}
		[[nodiscard]] const void* handle() const
		{
			return this;
		}

		void* d_ptr;

	private:
		RML_LAYOUT_GUARD_BEGIN()
			RML_ASSERT_LAYOUT_OFFSET(QObject, d_ptr, sizeof(void*));
			RML_ASSERT_LAYOUT_SIZE(QObject, sizeof(void*) * 2);
		RML_LAYOUT_GUARD_END()
	};
}
