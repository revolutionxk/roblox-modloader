#pragma once

#include <mutex>

namespace rml::utils
{
	std::recursive_mutex& dbghelp_mutex();
}
