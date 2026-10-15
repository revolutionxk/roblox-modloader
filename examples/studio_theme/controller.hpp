#pragma once

#include "engine.hpp"
#include "palette.hpp"
#include "qt_theme.hpp"
#include "theme_store.hpp"

#include <RobloxModLoader/luau/luau_bridge.hpp>

#include <array>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace studio_theme
{
	class Controller
	{
	public:
		Controller(std::filesystem::path root, Engine engine);
		~Controller();

		Controller(const Controller&) = delete;
		Controller& operator=(const Controller&) = delete;

		void start();
		void stop();
		void on_data_model();

	private:
		using Handler = rml::luau::BridgeArgs (Controller::*)(const rml::luau::BridgeArgs&);

		[[nodiscard]] static std::array<std::pair<std::string_view, Handler>, 7> functions();
		[[nodiscard]] rml::luau::NativeFunction guarded(std::string_view name, Handler handler);
		void register_bridge();
		void unregister_bridge();
		void build_menu();
		void select(const ThemeDefinition* theme, bool commit);
		std::expected<void, std::string> activate_by_name(const std::optional<std::string>& name);

		rml::luau::BridgeArgs status(const rml::luau::BridgeArgs& args);
		rml::luau::BridgeArgs list(const rml::luau::BridgeArgs& args);
		rml::luau::BridgeArgs get(const rml::luau::BridgeArgs& args);
		rml::luau::BridgeArgs save(const rml::luau::BridgeArgs& args);
		rml::luau::BridgeArgs preview(const rml::luau::BridgeArgs& args);
		rml::luau::BridgeArgs activate(const rml::luau::BridgeArgs& args);
		rml::luau::BridgeArgs map(const rml::luau::BridgeArgs& args);

		std::mutex m_mutex;
		ThemeStore m_store;
		QtTheme m_qt;
		std::shared_ptr<const Palette> m_palette;
		bool m_registered{};
		std::once_flag m_first_data_model;
	};
}
