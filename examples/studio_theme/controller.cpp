#include "controller.hpp"

#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/luau/script_runtime.hpp>
#include <RobloxModLoader/qt/menu_node.hpp>
#include <RobloxModLoader/qt/qt_integration.hpp>

#include <exception>
#include <format>
#include <type_traits>
#include <variant>
#include <vector>

RML_LOG_SCOPE("StudioTheme")

namespace studio_theme
{
	using rml::luau::BridgeArgs;
	using rml::luau::BridgeTable;
	using rml::luau::BridgeValue;

	static constexpr std::string_view k_bridge_name = "studio_theme";

	static nlohmann::json to_json(const BridgeValue& value)
	{
		return std::visit(
		    [](const auto& held) -> nlohmann::json {
			    using Held = std::decay_t<decltype(held)>;
			    if constexpr (std::is_same_v<Held, std::monostate>)
			    {
				    return nullptr;
			    }
			    else if constexpr (std::is_same_v<Held, std::shared_ptr<const BridgeTable>>)
			    {
				    auto object = nlohmann::json::object();
				    if (held)
					    for (const auto& [key, field] : held->fields)
						    object[key] = to_json(field);
				    return object;
			    }
			    else
			    {
				    return held;
			    }
		    },
		    value);
	}

	static BridgeValue to_bridge(const nlohmann::json& json)
	{
		if (json.is_object())
		{
			auto table = std::make_shared<BridgeTable>();
			for (const auto& [key, value] : json.items())
				table->fields.emplace(key, to_bridge(value));
			return std::shared_ptr<const BridgeTable>{std::move(table)};
		}
		if (json.is_string())
			return json.get<std::string>();
		if (json.is_boolean())
			return json.get<bool>();
		if (json.is_number())
			return json.get<double>();
		return std::monostate{};
	}

	static std::shared_ptr<const BridgeTable> table_arg(const BridgeArgs& args, const std::size_t index)
	{
		if (index >= args.size())
			return nullptr;
		const auto* held = std::get_if<std::shared_ptr<const BridgeTable>>(&args[index]);
		return held ? *held : nullptr;
	}

	static std::optional<std::string> string_arg(const BridgeArgs& args, const std::size_t index)
	{
		if (index >= args.size())
			return std::nullopt;
		const auto* held = std::get_if<std::string>(&args[index]);
		return held ? std::optional<std::string>{*held} : std::nullopt;
	}

	static bool bool_arg(const BridgeArgs& args, const std::size_t index)
	{
		const auto* held = index < args.size() ? std::get_if<bool>(&args[index]) : nullptr;
		return held && *held;
	}

	static BridgeArgs failure(std::string message)
	{
		return {false, std::move(message)};
	}

	static std::expected<ThemeDefinition, std::string> theme_arg(const BridgeArgs& args, const std::size_t index)
	{
		const auto table = table_arg(args, index);
		if (!table)
			return std::unexpected("expected a theme table");
		return parse_theme(to_json(BridgeValue{table}), false);
	}

	static void emit(const std::string_view event, const BridgeArgs& args = {})
	{
		if (auto* const runtime = rml::luau::script_runtime())
			if (const auto emitted = runtime->bridge().emit(event, args); !emitted)
				RML_WARN("{} was not delivered: {}", event, emitted.error());
	}

	Controller::Controller(std::filesystem::path root, Engine engine) :
	    m_store(std::move(root)),
	    m_qt(engine, [](const Base base) {
		    emit("studio_theme.changed", {std::string{base_name(base)}});
	    })
	{
	}

	Controller::~Controller()
	{
		stop();
	}

	void Controller::start()
	{
		{
			std::scoped_lock lock{m_mutex};
			m_store.load();
			if (const auto& active = m_store.active())
			{
				if (const auto* theme = m_store.find(*active))
					m_palette = std::make_shared<const Palette>(*theme);
				else
					RML_WARN("The active theme '{}' is gone, Studio stays stock", *active);
			}
			m_qt.set_palette(m_palette);
			RML_INFO("{} themes loaded, active: {}", m_store.themes().size(), m_palette ? m_palette->theme().name : std::string{"stock"});
		}

		m_qt.install();
		register_bridge();
		build_menu();
	}

	void Controller::stop()
	{
		unregister_bridge();
		m_qt.uninstall();
	}

	void Controller::on_data_model()
	{
		std::call_once(m_first_data_model, [this] {
			std::scoped_lock lock{m_mutex};
			if (m_palette)
				m_qt.apply(false);
		});
	}

	std::array<std::pair<std::string_view, Controller::Handler>, 7> Controller::functions()
	{
		return {{
		    {"status", &Controller::status},
		    {"list", &Controller::list},
		    {"get", &Controller::get},
		    {"save", &Controller::save},
		    {"preview", &Controller::preview},
		    {"activate", &Controller::activate},
		    {"map", &Controller::map},
		}};
	}

	rml::luau::NativeFunction Controller::guarded(const std::string_view name, const Handler handler)
	{
		return [this, name, handler](const BridgeArgs& args) -> BridgeArgs {
			try
			{
				return (this->*handler)(args);
			}
			catch (const std::exception& error)
			{
				RML_ERROR("studio_theme.{} failed: {}", name, error.what());
				return failure(error.what());
			}
			catch (...)
			{
				RML_ERROR("studio_theme.{} failed", name);
				return failure("unexpected error");
			}
		};
	}

	void Controller::register_bridge()
	{
		auto* const runtime = rml::luau::script_runtime();
		if (!runtime)
		{
			RML_WARN("The script runtime is not available, the editor and the plugin recolor are off");
			return;
		}

		for (const auto& [name, handler] : functions())
			if (const auto registered = runtime->bridge().register_function(k_bridge_name, name, guarded(name, handler)); !registered)
				RML_ERROR("studio_theme.{} was not registered: {}", name, registered.error());
		m_registered = true;
	}

	void Controller::unregister_bridge()
	{
		if (!m_registered)
			return;
		m_registered = false;

		auto* const runtime = rml::luau::script_runtime();
		if (!runtime)
			return;
		for (const auto& [name, handler] : functions())
			if (const auto removed = runtime->bridge().unregister_function(k_bridge_name, name); !removed)
				RML_WARN("studio_theme.{} was not unregistered: {}", name, removed.error());
	}

	void Controller::build_menu()
	{
		auto* const qt = rml::qt::QtIntegration::instance();
		if (!qt)
			return;

		std::vector<std::string> names;
		{
			std::scoped_lock lock{m_mutex};
			for (const auto& theme : m_store.themes())
				names.push_back(theme.name);
		}

		auto root = qt->menu().add_submenu("Studio Theme");
		root.add_action("Theme Editor", [] {
			emit("studio_theme.toggle_editor");
		});
		root.add_separator();
		root.add_action("Stock", [this] {
			if (const auto activated = activate_by_name(std::nullopt); !activated)
				RML_WARN("{}", activated.error());
		});
		for (const auto& name : names)
			root.add_action(name, [this, name] {
				if (const auto activated = activate_by_name(name); !activated)
					RML_WARN("{}", activated.error());
			});
	}

	void Controller::select(const ThemeDefinition* const theme, const bool commit)
	{
		std::shared_ptr<const Palette> palette = theme ? std::make_shared<const Palette>(*theme) : nullptr;
		{
			std::scoped_lock lock{m_mutex};
			m_palette = palette;
		}
		m_qt.set_palette(palette);
		emit("studio_theme.palette");
		m_qt.apply(commit);
	}

	std::expected<void, std::string> Controller::activate_by_name(const std::optional<std::string>& name)
	{
		std::optional<ThemeDefinition> theme;
		{
			std::scoped_lock lock{m_mutex};
			if (name)
			{
				const auto* found = m_store.find(*name);
				if (!found)
					return std::unexpected(std::format("no theme named '{}'", *name));
				theme = *found;
			}
			m_store.set_active(name);
		}
		select(theme ? &*theme : nullptr, theme.has_value());
		return {};
	}

	BridgeArgs Controller::status(const BridgeArgs&)
	{
		auto table = std::make_shared<BridgeTable>();
		table->fields.emplace("qt", m_qt.available());
		{
			std::scoped_lock lock{m_mutex};
			if (const auto& active = m_store.active())
				table->fields.emplace("active", *active);
		}
		return {std::shared_ptr<const BridgeTable>{std::move(table)}};
	}

	BridgeArgs Controller::list(const BridgeArgs&)
	{
		auto table = std::make_shared<BridgeTable>();
		std::scoped_lock lock{m_mutex};
		for (const auto& theme : m_store.themes())
		{
			auto entry = std::make_shared<BridgeTable>();
			entry->fields.emplace("base", std::string{base_name(theme.base)});
			entry->fields.emplace("builtin", theme.builtin);
			table->fields.emplace(theme.name, std::shared_ptr<const BridgeTable>{std::move(entry)});
		}
		return {std::shared_ptr<const BridgeTable>{std::move(table)}};
	}

	BridgeArgs Controller::get(const BridgeArgs& args)
	{
		const auto name = string_arg(args, 0);
		if (!name)
			return failure("expected a theme name");

		std::scoped_lock lock{m_mutex};
		const auto* theme = m_store.find(*name);
		if (!theme)
			return failure(std::format("no theme named '{}'", *name));
		return {to_bridge(serialize_theme(*theme))};
	}

	BridgeArgs Controller::save(const BridgeArgs& args)
	{
		auto theme = theme_arg(args, 0);
		if (!theme)
			return failure(theme.error());

		std::scoped_lock lock{m_mutex};
		if (const auto saved = m_store.save(std::move(*theme)); !saved)
			return failure(saved.error());
		return {true};
	}

	BridgeArgs Controller::preview(const BridgeArgs& args)
	{
		auto theme = theme_arg(args, 0);
		if (!theme)
			return failure(theme.error());

		auto palette = std::make_shared<const Palette>(std::move(*theme));
		{
			std::scoped_lock lock{m_mutex};
			m_palette = palette;
		}
		m_qt.set_palette(palette);
		emit("studio_theme.palette");
		if (bool_arg(args, 1))
			m_qt.apply(false);
		return {true};
	}

	BridgeArgs Controller::activate(const BridgeArgs& args)
	{
		if (const auto activated = activate_by_name(string_arg(args, 0)); !activated)
			return failure(activated.error());
		return {true};
	}

	BridgeArgs Controller::map(const BridgeArgs& args)
	{
		const auto colors = table_arg(args, 0);
		std::shared_ptr<const Palette> palette;
		{
			std::scoped_lock lock{m_mutex};
			palette = m_palette;
		}

		auto table = std::make_shared<BridgeTable>();
		if (colors)
			for (const auto& entry : colors->fields)
				table->fields.emplace(entry.first, palette ? palette->map_hex(entry.first) : entry.first);
		return {std::shared_ptr<const BridgeTable>{std::move(table)}};
	}
}
