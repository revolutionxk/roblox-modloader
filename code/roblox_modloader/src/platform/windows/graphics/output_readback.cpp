#include "RobloxModLoader/platform/graphics/output_readback.hpp"

namespace rml::platform
{
	bool set_output_readable(bool)
	{
		return true;
	}

	bool output_readable(std::uint32_t, std::uint32_t)
	{
		return true;
	}
}
