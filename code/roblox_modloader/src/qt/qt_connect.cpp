#include "qt_connect.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/memory/vtable.hpp"
#include "RobloxModLoader/qt/qmetaobject.hpp"
#include "RobloxModLoader/qt/qobject.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

#include <atomic>
#include <cstdint>
#include <utility>

namespace rml::qt::QtPrivate
{
	class QSlotObjectBase
	{
	public:
		enum Operation
		{
			Destroy,
			Call,
			Compare,
			NumOperations,
		};

		using ImplFn = void (*)(int which, QSlotObjectBase* self, QObject* receiver, void** args, bool* ret);

		std::atomic<int> m_ref;
		ImplFn m_impl;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QSlotObjectBase, m_impl, sizeof(void*));
	RML_LAYOUT_DIAGNOSTIC_POP()
}

namespace rml::qt::detail
{
	struct FunctionSlot : QtPrivate::QSlotObjectBase
	{
		std::function<void(void**)> callable;
	};

	static void slot_impl(const int which, QtPrivate::QSlotObjectBase* self, QObject*, void** args, bool* ret)
	{
		auto* const slot = static_cast<FunctionSlot*>(self);
		switch (which)
		{
		case QtPrivate::QSlotObjectBase::Destroy: delete slot; break;
		case QtPrivate::QSlotObjectBase::Call:
			if (slot->callable)
				slot->callable(args);
			break;
		case QtPrivate::QSlotObjectBase::Compare:
			if (ret)
				*ret = false;
			break;
		default: break;
		}
	}

	bool connect_function(const void* sender, void* signal_addr, const void* sender_meta, std::function<void(void**)> slot)
	{
		if (!sender || !signal_addr || !sender_meta)
			return false;

		static void* const connect = core_export("QObject::connectImpl(QObject const*, void**, QObject const*, void**, QtPrivate::QSlotObjectBase*, Qt::ConnectionType, int const*, QMetaObject const*)");
		if (!connect)
			return false;

		auto* const function = new FunctionSlot{{1, &slot_impl}, std::move(slot)};

		memory::MemberFunctionPointer signal_pmf{reinterpret_cast<std::uintptr_t>(signal_addr), 0};
		const auto signal = reinterpret_cast<void**>(&signal_pmf);

		QMetaObject::Connection connection;
		memory::call_returning(connect,
		    connection,
		    sender,
		    signal,
		    sender,
		    static_cast<void**>(nullptr),
		    static_cast<QtPrivate::QSlotObjectBase*>(function),
		    0,
		    static_cast<const int*>(nullptr),
		    sender_meta);
		return connection.d_ptr != nullptr;
	}
}
