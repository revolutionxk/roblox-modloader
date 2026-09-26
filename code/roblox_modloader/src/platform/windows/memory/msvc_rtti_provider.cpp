#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/memory/rtti_scanner.hpp"
#include "RobloxModLoader/memory/i_rtti_provider.hpp"

namespace rml::memory
{
	class MsvcRttiProvider final : public IRttiProvider
	{
	public:
		std::optional<void**> find_class_vtable(const std::string_view class_name) override
		{
			const auto* info = rtti::RTTIManager::get_class_rtti(class_name);
			if (!info)
				return std::nullopt;

			auto* vtable = info->get_virtual_function_table();
			if (!vtable)
				return std::nullopt;

			return vtable;
		}

		// Undecorated names, as the scanner keys them. The alphabetically first match wins, every launch.
		std::optional<void**> find_class_vtable_matching(const std::string_view mangled_prefix, const std::function<bool(std::string_view)>& accept) override
		{
			const std::string* chosen_name = nullptr;
			void** chosen = nullptr;
			for (const auto& [name, info] : rtti::Scanner::get_all_classes())
			{
				if (!info || !name.starts_with(mangled_prefix) || !accept(name))
					continue;
				if (chosen_name && *chosen_name <= name)
					continue;
				if (auto* vtable = info->get_virtual_function_table())
				{
					chosen_name = &name;
					chosen = vtable;
				}
			}

			if (!chosen)
				return std::nullopt;
			return chosen;
		}

	private:
		rtti::RTTIManager m_manager;
	};

	std::unique_ptr<IRttiProvider> create_rtti_provider()
	{
		return std::make_unique<MsvcRttiProvider>();
	}
}
