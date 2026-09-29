#include "RobloxModLoader/roblox/signals.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "pointers.hpp"

namespace rbx::signals
{
	void intrusive_weak_ptr_free(slot_base* slot) noexcept
	{
		if (g_pointers && g_pointers->m_roblox_pointers.signal_slot_free)
			g_pointers->m_roblox_pointers.signal_slot_free(slot);
	}

	void intrusive_ptr_release(slots_holder* holder) noexcept
	{
		if (holder && g_pointers && g_pointers->m_roblox_pointers.slots_holder_release)
			g_pointers->m_roblox_pointers.slots_holder_release(holder);
	}

	void connection::disconnect() const
	{
		if (m_slot && g_pointers && g_pointers->m_roblox_pointers.connection_disconnect)
			g_pointers->m_roblox_pointers.connection_disconnect(this);
	}
}
