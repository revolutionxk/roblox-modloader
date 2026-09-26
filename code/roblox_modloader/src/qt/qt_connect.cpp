#include "qt_connect.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/memory/vtable.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

#include <cstdint>
#include <utility>

namespace rml::qt::detail
{
	namespace
	{
		using impl_fn = void (*)(int which, void* self, void* receiver, void** args, bool* ret);

		struct FunctionSlot
		{
			int ref;
			impl_fn impl;
			std::function<void(void**)> callable;
		};

		void slot_impl(const int which, void* self, void*, void** args, bool* ret)
		{
			const auto* const slot = static_cast<FunctionSlot*>(self);
			switch (which)
			{
			case 0: delete slot; break;
			case 1:
				if (slot->callable)
					slot->callable(args);
				break;
			case 2:
				if (ret)
					*ret = false;
				break;
			default: break;
			}
		}
	}

	bool connect_function(const void* sender, void* signal_addr, const void* sender_meta, std::function<void(void**)> slot)
	{
		if (!sender || !signal_addr || !sender_meta)
			return false;

		static void* const connect = core_export("QObject::connectImpl(QObject const*, void**, QObject const*, void**, QtPrivate::QSlotObjectBase*, Qt::ConnectionType, int const*, QMetaObject const*)");
		if (!connect)
			return false;

		auto* const function = new FunctionSlot{1, &slot_impl, std::move(slot)};

		memory::MemberFunctionPointer signal_pmf{reinterpret_cast<std::uintptr_t>(signal_addr), 0};
		const auto signal = reinterpret_cast<void**>(&signal_pmf);

		void* result = nullptr;
		memory::call_returning<void*>(connect,
		    result,
		    sender,
		    signal,
		    sender,
		    static_cast<void**>(nullptr),
		    static_cast<void*>(function),
		    0,
		    static_cast<const int*>(nullptr),
		    sender_meta);
		return true;
	}
}
