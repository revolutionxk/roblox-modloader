#include "RobloxModLoader/qt/qt_integration.hpp"

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/qt/qapplication.hpp"
#include "qt_connect.hpp"

RML_LOG_SCOPE("QtIntegration")

namespace rml::qt
{
	QtIntegration* QtIntegration::s_instance = nullptr;

	QtIntegration::QtIntegration() :
	    m_menu(m_dispatcher)
	{
		s_instance = this;
	}

	QtIntegration::~QtIntegration()
	{
		s_instance = nullptr;
	}

	ModsMenu& QtIntegration::menu()
	{
		return m_menu;
	}

	void QtIntegration::on_menu_bar_built(QMenuBar* menu_bar)
	{
		m_menu.rebuild(menu_bar);

		m_gui_ready.store(true, std::memory_order_release);
		request_drain();
	}

	void QtIntegration::run_on_gui_thread(std::function<void()> task)
	{
		if (!task)
			return;

		{
			const std::scoped_lock lock(m_tasks_mutex);
			m_tasks.push_back(std::move(task));
		}
		request_drain();
	}

	void QtIntegration::request_drain()
	{
		// Tasks queued before Studio builds its menu bar wait for it.
		if (!m_gui_ready.load(std::memory_order_acquire))
			return;

		// One queued call drains every task, so a burst of tasks wakes the GUI
		// thread once and an idle queue never wakes it at all.
		if (m_drain_requested.exchange(true, std::memory_order_acq_rel))
			return;

		const bool queued = detail::invoke_queued(QApplication::instance(), [] {
			if (QtIntegration* const self = QtIntegration::instance())
				self->drain_tasks();
		});
		if (!queued)
			m_drain_requested.store(false, std::memory_order_release);
	}

	void QtIntegration::drain_tasks()
	{
		// Cleared before the swap: a task queued from here on asks for a new drain.
		m_drain_requested.store(false, std::memory_order_release);

		std::vector<std::function<void()>> pending;
		{
			const std::scoped_lock lock(m_tasks_mutex);
			pending.swap(m_tasks);
		}

		for (auto& task : pending)
		{
			try
			{
				task();
			}
			catch (const std::exception& error)
			{
				RML_ERROR("GUI task threw: {}", error.what());
			}
			catch (...)
			{
				RML_ERROR("GUI task threw an unknown exception");
			}
		}
	}

	void QtIntegration::on_action_triggered(QAction* action) const
	{
		m_dispatcher.dispatch(action);
	}

	bool QtIntegration::ensure_action_hook()
	{
		return m_dispatcher.ensure_hook();
	}

	bool QtIntegration::is_action_hook_ready() const
	{
		return m_dispatcher.is_hook_ready();
	}

	QtIntegration* QtIntegration::instance()
	{
		return s_instance;
	}
}
