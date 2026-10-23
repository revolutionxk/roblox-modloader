#include "utils/dbghelp_lock.hpp"

#include "RobloxModLoader/rml_export.hpp"

namespace rml::utils
{
	std::recursive_mutex& dbghelp_mutex()
	{
		static std::recursive_mutex mutex;
		return mutex;
	}
}

extern "C" RML_EXPORT void RmlDbgHelpInit()
{
}

extern "C" RML_EXPORT void RmlDbgHelpLock()
{
	rml::utils::dbghelp_mutex().lock();
}

extern "C" RML_EXPORT void RmlDbgHelpUnlock()
{
	rml::utils::dbghelp_mutex().unlock();
}
