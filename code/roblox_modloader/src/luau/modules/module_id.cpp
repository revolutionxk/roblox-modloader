#include "RobloxModLoader/luau/modules/module_id.hpp"

#include "RobloxModLoader/util/string.hpp"

namespace rml::luau
{
	static std::string identity_key(const std::string& path)
	{
#if defined(RML_WINDOWS)
		return utils::to_lower(path);
#else
		return path;
#endif
	}

	ModuleId::ModuleId(std::string canonical_path)
		: m_path(std::move(canonical_path)), m_key(identity_key(m_path)), m_hash(std::hash<std::string>{}(m_key))
	{
	}

	std::string_view ModuleId::display() const noexcept
	{
		const std::string_view path{m_path};
		const auto separator = path.find_last_of("/\\");
		return separator == std::string_view::npos ? path : path.substr(separator + 1);
	}
}
