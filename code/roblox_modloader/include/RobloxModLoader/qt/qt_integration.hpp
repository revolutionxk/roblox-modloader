#pragma once

#include "RobloxModLoader/qt/action_dispatcher.hpp"
#include "RobloxModLoader/qt/mods_menu.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace rml::qt
{
	class RML_EXPORT QtIntegration
	{
	public:
		QtIntegration();

		~QtIntegration();

		QtIntegration(const QtIntegration&) = delete;

		QtIntegration& operator=(const QtIntegration&) = delete;

		bool ensure_action_hook();

		[[nodiscard]] bool is_action_hook_ready() const;

		[[nodiscard]] ModsMenu& menu();

		void on_menu_bar_built(QMenuBar* menu_bar);

		void on_action_triggered(QAction* action) const;

		void run_on_gui_thread(std::function<void()> task);

		[[nodiscard]] static QtIntegration* instance();

	private:
		void request_drain();
		void drain_tasks();

		ActionDispatcher m_dispatcher;
		ModsMenu m_menu;

		std::mutex m_tasks_mutex;
		std::vector<std::function<void()>> m_tasks;
		std::atomic_bool m_gui_ready{};
		std::atomic_bool m_drain_requested{};

		static QtIntegration* s_instance;
	};
}
