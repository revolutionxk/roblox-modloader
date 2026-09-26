#include "RobloxModLoader/qt/qobject.hpp"

#include "RobloxModLoader/platform/abi.hpp"
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

	static constexpr std::size_t QT_METACAST_SLOT = 1;

	using qt_metacast_fn = void* (*)(const void*, const char*);

	static bool metacast_slot_is_valid()
	{
		static const bool valid = [] {
			const void* const vtable = detail::core_export_optional("vtable for QObject");
			const void* const metacast = detail::core_export_optional("QObject::qt_metacast(char const*)");

			if (!vtable || !metacast)
				return false;

			const auto* const entries = static_cast<void* const*>(vtable) + platform::abi::vtable_prefix_slots;
			return entries[QT_METACAST_SLOT] == metacast;
		}();

		return valid;
	}

	bool QObject::inherits(const char* class_name) const
	{
		if (!class_name)
			return false;

		static const auto fn = detail::core_optional<bool (*)(const void*, const char*)>("QObject::inherits(char const*) const");
		if (fn)
			return fn(this, class_name);
		
		if (!metacast_slot_is_valid())
			return false;

		const auto* const vtable = *reinterpret_cast<void* const* const*>(this);
		if (!vtable)
			return false;

		const auto metacast = reinterpret_cast<qt_metacast_fn>(vtable[QT_METACAST_SLOT]);
		return metacast && metacast(this, class_name) != nullptr;
	}
}
