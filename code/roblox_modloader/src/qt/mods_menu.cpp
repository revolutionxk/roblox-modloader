#include "RobloxModLoader/qt/mods_menu.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/qt/action_dispatcher.hpp"
#include "RobloxModLoader/qt/menu_node.hpp"
#include "RobloxModLoader/qt/qicon.hpp"
#include "RobloxModLoader/qt/qmenu.hpp"
#include "RobloxModLoader/qt/qmenubar.hpp"
#include "RobloxModLoader/qt/qmessagebox.hpp"
#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/util/filesystem.hpp"
#include "RobloxModLoader/version.hpp"
#include "logo.hpp"
#include "config/config_manager.hpp"
#include "filesystem/directory.hpp"
#include "utils/shell.hpp"

#if RML_ENABLE_LUAU
	#include "RobloxModLoader/luau/script_runtime.hpp"
#endif

namespace rml::qt
{
	void ModsMenu::show_about()
	{
		QtOwned<QMessageBox> box = QMessageBox::create_owned();

		if (!box)
		{
			utils::shell::message_box("About RobloxModLoader",
			    "RobloxModLoader\n\n"
			    "A modding framework for Roblox Studio — native, C#, and Luau mods.\n\n"
			    "github.com/revolutionxk/roblox-modloader");
			return;
		}

		const auto logo_path = filesystem::directory::get_mod_loader_directory() / "assets" / "logo.png";
		(void)utils::write_file(logo_path, std::string_view(reinterpret_cast<const char*>(LOGO_PNG), LOGO_PNG_SIZE));

		const std::string logo_url = "file:///" + logo_path.generic_string();

		const std::string version = std::string(version::commit()) + (version::dirty() ? "-dirty" : "") + " &middot; " + version::branch() + " &middot; " + version::date();

		box->setWindowTitle("About RobloxModLoader");
		box->setTextFormat(TextFormat::RichText);
		box->addButton(QMessageBox::Ok);
		box->setStyleSheet("QMessageBox { background-color:#1e1f22; }"
		                   "QMessageBox QLabel { color:#e6e6e6; min-width:320px; }"
		                   "QPushButton { color:white; padding:6px 22px; font-weight:600; }");

		const std::string html = "<div align='center' style='font-family:Segoe UI, Arial'>"
		                         "<img src='"
		    + logo_url
		    + "' width='96' height='96'><br>"
		      "<span style='font-size:20px; font-weight:600;'>Roblox Mod Loader</span><br>"
		      "<span style='color:#9aa0a6'>A modding framework for Roblox Studio</span>"
		      "<p style='color:#dddddd; margin-top:12px'>Native, C# and Luau mods loaded directly into Studio.</p>"
		      "<a href='https://github.com/revolutionxk/roblox-modloader' style='color:#5b9bff'>"
		      "github.com/revolutionxk/roblox-modloader</a>"
		      "<p style='color:#5c6066; font-size:11px; margin-top:14px'>"
		    + version
		    + "</p>"
		      "</div>";
		box->setText(QString{std::string_view{html}});
		box->exec();
	}

	ModsMenu::ModsMenu(ActionDispatcher& dispatcher) :
	    m_dispatcher(dispatcher)
	{
	}

	void ModsMenu::persist_hot_reload(const bool enabled)
	{
		auto& manager = config::get_config_manager();

		auto core = manager.get_core_config();
		if (core.developer.enable_hot_reload == enabled)
			return;

		core.developer.enable_hot_reload = enabled;

		if (const auto updated = manager.update_core_config(std::move(core)); !updated)
		{
			LOG_WARN("[qt] Could not update the hot reload setting");
			return;
		}

		if (const auto saved = manager.save_config(filesystem::directory::get_mod_loader_directory() / "config.toml");
		    !saved)
			LOG_WARN("[qt] Could not persist the hot reload setting");
	}

	void ModsMenu::add_action(std::string text, std::function<void()> on_click)
	{
		add_action(0, std::move(text), std::move(on_click));
	}

	uint64_t ModsMenu::register_action(std::string text, std::function<void()> on_click)
	{
		return add_action(0, std::move(text), std::move(on_click));
	}

	void ModsMenu::remove_action(uint64_t id)
	{
		remove(id);
	}

	uint64_t ModsMenu::add_node(uint64_t parent_id, MenuItem node)
	{
		std::scoped_lock lock(m_mutex);
		if (parent_id != 0 && !m_nodes.contains(parent_id))
		{
			LOG_ERROR("[qt] ModsMenu: unknown parent_id {}", parent_id);
			return 0;
		}

		const uint64_t id = m_next_id++;
		node.id = id;
		node.parent_id = parent_id;

		if (parent_id == 0)
			m_root_children.push_back(id);
		else
			m_nodes[parent_id].children.push_back(id);

		m_nodes.emplace(id, std::move(node));
		return id;
	}

	void ModsMenu::trigger_rebuild()
	{
		QMenuBar* menu_bar = nullptr;
		{
			std::scoped_lock lock(m_mutex);
			menu_bar = m_menu_bar_handle;
		}

		if (menu_bar)
			rebuild(menu_bar);
	}

	uint64_t ModsMenu::add_action(uint64_t parent_id, std::string text, std::function<void()> on_click)
	{
		if (text.empty() || !on_click)
			return 0;

		MenuItem node;
		node.kind = MenuItemKind::Action;
		node.text = std::move(text);
		node.on_click = std::move(on_click);

		const uint64_t id = add_node(parent_id, std::move(node));
		if (id != 0)
			trigger_rebuild();

		return id;
	}

	uint64_t ModsMenu::add_submenu(uint64_t parent_id, std::string text)
	{
		if (text.empty())
			return 0;

		MenuItem node;
		node.kind = MenuItemKind::Submenu;
		node.text = std::move(text);

		const uint64_t id = add_node(parent_id, std::move(node));
		if (id != 0)
			trigger_rebuild();

		return id;
	}

	uint64_t ModsMenu::add_separator(uint64_t parent_id)
	{
		MenuItem node;
		node.kind = MenuItemKind::Separator;

		const uint64_t id = add_node(parent_id, std::move(node));
		if (id != 0)
			trigger_rebuild();

		return id;
	}

	uint64_t ModsMenu::add_checkable(uint64_t parent_id, std::string text, bool initial, std::function<void(bool)> on_toggle)
	{
		if (text.empty() || !on_toggle)
			return 0;

		MenuItem node;
		node.kind = MenuItemKind::CheckableAction;
		node.text = std::move(text);
		node.checked = initial;
		node.on_toggle = std::move(on_toggle);

		const uint64_t id = add_node(parent_id, std::move(node));
		if (id != 0)
			trigger_rebuild();

		return id;
	}

	void ModsMenu::set_item_icon(uint64_t id, std::string path)
	{
		if (id == 0)
			return;

		{
			std::scoped_lock lock(m_mutex);
			const auto it = m_nodes.find(id);
			if (it == m_nodes.end())
			{
				LOG_ERROR("[qt] ModsMenu: set_item_icon unknown id {}", id);
				return;
			}
			it->second.icon_path = std::move(path);
		}

		trigger_rebuild();
	}

	void ModsMenu::erase_subtree(uint64_t id)
	{
		const auto it = m_nodes.find(id);
		if (it == m_nodes.end())
			return;

		const std::vector<uint64_t> children = it->second.children;
		for (const uint64_t child_id : children)
			erase_subtree(child_id);

		m_nodes.erase(id);
	}

	void ModsMenu::remove(uint64_t id)
	{
		if (id == 0)
			return;

		{
			std::scoped_lock lock(m_mutex);
			const auto it = m_nodes.find(id);
			if (it == m_nodes.end())
				return;

			const uint64_t parent_id = it->second.parent_id;
			erase_subtree(id);

			if (parent_id == 0)
				std::erase(m_root_children, id);
			else if (const auto parent_it = m_nodes.find(parent_id); parent_it != m_nodes.end())
				std::erase(parent_it->second.children, id);
		}

		trigger_rebuild();
	}

	rml::qt::MenuNode ModsMenu::add_submenu(std::string text)
	{
		return rml::qt::MenuNode{*this, add_submenu(uint64_t{0}, std::move(text))};
	}

	rml::qt::MenuNode ModsMenu::root_node()
	{
		return rml::qt::MenuNode{*this, 0};
	}

	void ModsMenu::set_checked_state(uint64_t id, bool checked)
	{
		std::scoped_lock lock(m_mutex);
		if (const auto it = m_nodes.find(id); it != m_nodes.end())
			it->second.checked = checked;
	}

	void ModsMenu::build_into(QMenu* parent_menu, uint64_t node_id, const std::unordered_map<uint64_t, MenuItem>& nodes, std::vector<QAction*>& live)
	{
		const auto it = nodes.find(node_id);
		if (it == nodes.end())
			return;

		const MenuItem& node = it->second;

		switch (node.kind)
		{
		case MenuItemKind::Action:
		{
			QAction* const action = parent_menu->addAction(std::string_view{node.text});
			if (!action)
				return;

			action->setMenuRole(QAction::NoRole); // Without this, macOS hoists "About" out of here and into the application menu idk why
			if (!node.icon_path.empty())
				action->setIcon(QIcon(QString{node.icon_path}));

			m_dispatcher.connect(action, node.on_click);
			live.push_back(action);
			break;
		}
		case MenuItemKind::Separator:
		{
			parent_menu->addSeparator();
			break;
		}
		case MenuItemKind::CheckableAction:
		{
			QAction* const action = parent_menu->addAction(std::string_view{node.text});
			if (!action)
				return;

			action->setMenuRole(QAction::NoRole);
			action->setCheckable(true);
			action->setChecked(node.checked);
			if (!node.icon_path.empty())
				action->setIcon(QIcon(QString{node.icon_path}));

			const auto on_toggle = node.on_toggle;
			m_dispatcher.connect_toggled(action, [this, id = node.id, on_toggle](bool value) {
				set_checked_state(id, value);
				if (on_toggle)
					on_toggle(value);
			});
			live.push_back(action);
			break;
		}
		case MenuItemKind::Submenu:
		{
			QMenu* const submenu = parent_menu->addMenu(std::string_view{node.text});
			if (!submenu)
				return;

			if (!node.icon_path.empty())
			{
				if (QAction* const submenu_action = submenu->menuAction())
					submenu_action->setIcon(QIcon(QString{node.icon_path}));
			}

			for (const uint64_t child_id : node.children)
				build_into(submenu, child_id, nodes, live);
			break;
		}
		}
	}

	void ModsMenu::rebuild(QMenuBar* menu_bar_handle)
	{
		if (!menu_bar_handle)
			return;

		std::unordered_map<uint64_t, MenuItem> nodes;
		std::vector<uint64_t> root_children;
		std::vector<QAction*> previous;
		QMenu* menu = nullptr;
		{
			std::scoped_lock lock(m_mutex);
			nodes = m_nodes;
			root_children = m_root_children;
			previous = std::move(m_live_actions);
			m_live_actions.clear();
			if (m_menu && m_menu_bar_handle == menu_bar_handle)
				menu = m_menu;
		}

		for (QAction* stale : previous)
			m_dispatcher.disconnect(stale);

		if (menu)
		{
			menu->clear();
		}
		else
		{
			menu = menu_bar_handle->addMenu("Mods");
			if (!menu)
			{
				LOG_ERROR("[qt] QMenuBar::addMenu returned null; Mods menu not added");
				return;
			}
		}

		std::vector<QAction*> live;

		const auto emit_builtin = [&](const std::string_view text, std::function<void()> on_click) {
			QAction* const action = menu->addAction(text);
			if (!action)
				return;

			// Without this, macOS hoists "About" out of here and into the application menu.
			action->setMenuRole(QAction::NoRole);

			m_dispatcher.connect(action, std::move(on_click));
			live.push_back(action);
		};

		emit_builtin("Open Mods Folder", [] {
			utils::shell::open_folder(filesystem::directory::get_mod_loader_directory() / "mods");
		});
		emit_builtin("Open Logs Folder", [] {
			utils::shell::open_folder(filesystem::directory::get_mod_loader_directory() / "logs");
		});
		emit_builtin("Open Config", [] {
			utils::shell::open(filesystem::directory::get_mod_loader_directory() / "config.toml");
		});

#if RML_ENABLE_LUAU
		if (auto* runtime = luau::script_runtime())
		{
			menu->addSeparator();

			emit_builtin("Reload Luau Scripts", [] {
				if (auto* target = luau::script_runtime())
					LOG_INFO("[qt] Queued a reload of {} script mod(s)", target->reload_all());
			});

			QAction* const auto_reload = menu->addAction("Auto-reload Luau Scripts");
			if (auto_reload)
			{
				auto_reload->setMenuRole(QAction::NoRole);
				auto_reload->setCheckable(true);
				auto_reload->setChecked(runtime->hot_reload_enabled());

				m_dispatcher.connect_toggled(auto_reload, [](const bool enabled) {
					if (auto* target = luau::script_runtime())
						target->set_hot_reload(enabled);

					persist_hot_reload(enabled);
				});

				live.push_back(auto_reload);
			}
		}
#endif

		if (!root_children.empty())
		{
			menu->addSeparator();
			for (const uint64_t id : root_children)
				build_into(menu, id, nodes, live);
		}

		menu->addSeparator();
		emit_builtin("About", [] {
			show_about();
		});

		{
			std::scoped_lock lock(m_mutex);
			m_menu_bar_handle = menu_bar_handle;
			m_menu = menu;
			m_live_actions = std::move(live);
		}

		LOG_INFO("[qt] Mods menu rebuilt ({} root mod node(s) + built-ins)", root_children.size());
	}
}
