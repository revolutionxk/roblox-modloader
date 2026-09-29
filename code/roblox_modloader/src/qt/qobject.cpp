#include "RobloxModLoader/qt/qobject.hpp"

#include "RobloxModLoader/qt/qmetaobject.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	const char* QObject::class_name() const
	{
		const QMetaObject* meta = metaObject();
		const char* name = meta ? meta->className() : nullptr;
		return name ? name : "";
	}

	bool QObject::inherits(const char* class_name) const
	{
		if (!class_name)
			return false;

		static const auto fn = detail::core_optional<bool (*)(const QObject*, const char*)>("QObject::inherits(char const*) const");
		if (fn)
			return fn(this, class_name);

		return const_cast<QObject*>(this)->qt_metacast(class_name) != nullptr;
	}
}
