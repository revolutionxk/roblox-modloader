#include "RobloxModLoader/qt/qmetaobject.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	const char* QMetaObject::className() const
	{
		static const auto fn = detail::core<const char* (*)(const QMetaObject*)>("QMetaObject::className() const");
		return fn ? fn(this) : "";
	}
}

namespace rml::qt
{
	QMetaObject::Connection::~Connection()
	{
		static const auto dtor = detail::core<void (*)(Connection*)>("QMetaObject::Connection::~Connection()");
		if (d_ptr && dtor)
			dtor(this);
	}
}
