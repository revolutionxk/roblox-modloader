#include "RobloxModLoader/roblox/reflection/event_descriptor.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/platform/threading/engine_mutex.hpp"
#include "RobloxModLoader/roblox/reflection/event.hpp"
#include "pointers.hpp"

namespace RBX::Reflection
{
	template<typename Fn>
	void walk_slots_locked(rbx::signals::slots_holder* signal, Fn&& fn)
	{
		if (!signal)
			return;

		void* mtx = g_pointers && g_pointers->m_roblox_pointers.signal_mutex_get ? g_pointers->m_roblox_pointers.signal_mutex_get() : nullptr;
		const bool locked = rml::platform::lock_engine_mutex(mtx);

		for (auto* slot = signal->head; slot; slot = slot->next)
		{
			if (slot->holder)
				fn(slot);
		}

		if (locked)
			rml::platform::unlock_engine_mutex(mtx);
	}

	rbx::signals::slots_holder* EventDescriptor::get_signal(EventSource* source) const
	{
		if (!source)
			return nullptr;

		const auto offset = static_cast<const EventDesc*>(this)->signal;
		return reinterpret_cast<const rbx::signal<void()>*>(reinterpret_cast<std::uint8_t*>(source) + offset)->holder.get();
	}

	std::vector<rbx::signals::connection> EventDescriptor::snapshot_connections(EventSource* source) const
	{
		std::vector<rbx::signals::connection> out;
		walk_slots_locked(get_signal(source), [&](rbx::signals::slot_base* slot) {
			out.push_back(rbx::signals::connection::observe(slot));
		});
		return out;
	}
}
