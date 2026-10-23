#include "RobloxModLoader/roblox/rsl/mutex.hpp"
#include "pointers.hpp"

namespace RSL
{
	void Mutex::lock()
	{
		g_pointers->m_roblox_pointers.rsl_mutex_lock(this);
	}

	void Mutex::unlock()
	{
		g_pointers->m_roblox_pointers.rsl_mutex_unlock(this);
	}
}
