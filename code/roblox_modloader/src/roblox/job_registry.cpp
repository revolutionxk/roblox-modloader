#include "job_registry.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"

#include <algorithm>
#include <array>

RML_LOG_SCOPE("JobRegistry");

namespace rml
{
	JobRegistry::JobRegistry() noexcept
	{
		initialize_vtable_mappings();
	}

	std::expected<JobRegistry::JobId, std::string> JobRegistry::register_job(JobPtr job) noexcept
	{
		if (!job)
		{
			return std::unexpected("Cannot register null job");
		}

		if (m_shutdown_requested.load(std::memory_order_acquire))
		{
			return std::unexpected("TaskScheduler is shutting down");
		}

		const auto job_name = job->get_name();
		const auto job_id   = generate_job_id();

		std::unique_lock lock(m_jobs_mutex);

		if (m_name_to_id.contains(std::string(job_name)))
		{
			return std::unexpected(std::format("Job with name '{}' already exists", job_name));
		}

		try
		{
			m_jobs.emplace(job_id, JobEntry(std::move(job)));
			m_name_to_id.emplace(job_name, job_id);

			RML_INFO("Registered job '{}' with ID {}", job_name, job_id);
			return job_id;
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("Failed to register job '{}': {}", job_name, e.what()));
		}
	}

	bool JobRegistry::unregister_job(const JobId job_id) noexcept
	{
		std::unique_lock lock(m_jobs_mutex);

		const auto it = m_jobs.find(job_id);
		if (it == m_jobs.end())
		{
			return false;
		}

		const auto job_name = it->second.job->get_name();

		it->second.job->destroy();

		m_name_to_id.erase(std::string(job_name));
		m_jobs.erase(it);

		RML_DEBUG("Unregistered job '{}' (ID: {})", job_name, job_id);
		return true;
	}

	bool JobRegistry::unregister_job(const std::string_view job_name) noexcept
	{
		std::shared_lock shared_lock(m_jobs_mutex);

		const auto name_it = m_name_to_id.find(std::string(job_name));
		if (name_it == m_name_to_id.end())
		{
			return false;
		}

		const auto job_id = name_it->second;
		shared_lock.unlock();

		return unregister_job(job_id);
	}

	void JobRegistry::execute_jobs_for_kind(const JobExecutionContext& context) noexcept
	{
		if (m_shutdown_requested.load(std::memory_order_acquire))
		{
			return;
		}

		std::vector<std::pair<JobId, std::reference_wrapper<JobEntry> > > jobs_to_execute;
		{
			std::shared_lock lock(m_jobs_mutex);
			jobs_to_execute.reserve(m_jobs.size());

			for (auto& [job_id, entry] : m_jobs)
			{
				if (entry.job->should_execute(context))
				{
					jobs_to_execute.emplace_back(job_id, std::ref(entry));
				}
			}
		}

		std::ranges::sort(jobs_to_execute, [](const auto& a, const auto& b) {
			const auto priority_a = a.second.get().job->get_priority();
			const auto priority_b = b.second.get().job->get_priority();
			return static_cast<std::int32_t>(priority_a) < static_cast<std::int32_t>(priority_b);
		});

		for (auto& entry_ref : jobs_to_execute | std::views::values)
		{
			execute_job_with_stats(entry_ref.get(), context);
		}

		const auto current_count = m_execution_counter.fetch_add(1, std::memory_order_relaxed);

		if (current_count % 100 == 0)
		{
			cleanup_finished_jobs();
		}
	}

	std::optional<std::reference_wrapper<IJob> > JobRegistry::get_job(const JobId job_id) const noexcept
	{
		std::shared_lock lock(m_jobs_mutex);

		if (const auto it = m_jobs.find(job_id); it != m_jobs.end())
		{
			return std::ref(*it->second.job);
		}

		return std::nullopt;
	}

	std::optional<std::reference_wrapper<IJob> > JobRegistry::get_job(const std::string_view job_name) const noexcept
	{
		std::shared_lock lock(m_jobs_mutex);

		const auto name_it = m_name_to_id.find(std::string(job_name));
		if (name_it == m_name_to_id.end())
		{
			return std::nullopt;
		}

		if (const auto job_it = m_jobs.find(name_it->second); job_it != m_jobs.end())
		{
			return std::ref(*job_it->second.job);
		}

		return std::nullopt;
	}

	std::vector<JobRegistry::JobId> JobRegistry::get_jobs_by_kind(const JobKind kind) const noexcept
	{
		std::vector<JobId> result;
		std::shared_lock lock(m_jobs_mutex);

		for (const auto& [job_id, entry] : m_jobs)
		{
			if (has_job_kind(entry.job->get_target_kind(), kind) || kind == JobKind::Custom)
			{
				result.push_back(job_id);
			}
		}

		return result;
	}

	std::size_t JobRegistry::get_job_count() const noexcept
	{
		std::shared_lock lock(m_jobs_mutex);
		return m_jobs.size();
	}

	std::optional<JobRegistry::JobStats> JobRegistry::get_job_stats(const JobId job_id) const noexcept
	{
		std::shared_lock lock(m_jobs_mutex);

		if (const auto it = m_jobs.find(job_id); it != m_jobs.end())
		{
			return it->second.stats;
		}

		return std::nullopt;
	}

	void JobRegistry::reset_stats() noexcept
	{
		std::unique_lock lock(m_jobs_mutex);

		for (auto& entry : m_jobs | std::views::values)
		{
			entry.stats = JobStats{};
		}
	}

	void JobRegistry::shutdown() noexcept
	{
		m_shutdown_requested.store(true, std::memory_order_release);

		std::unique_lock lock(m_jobs_mutex);

		for (const auto& entry : m_jobs | std::views::values)
		{
			entry.job->destroy();
		}

		m_jobs.clear();
		m_name_to_id.clear();
	}

	bool JobRegistry::is_shutdown() const noexcept
	{
		return m_shutdown_requested.load(std::memory_order_acquire);
	}

	std::optional<JobKind> JobRegistry::get_job_kind_from_vtable(void** vtable) const noexcept
	{
		if (const auto it = m_vtable_to_kind.find(vtable); it != m_vtable_to_kind.end())
		{
			return it->second;
		}

		return std::nullopt;
	}

	std::optional<void**> JobRegistry::get_vtable_for_job_kind(const JobKind kind) const noexcept
	{
		if (const auto it = m_kind_to_vtable.find(kind); it != m_kind_to_vtable.end())
		{
			return it->second;
		}
		return std::nullopt;
	}

	void JobRegistry::initialize_vtable_mappings() noexcept
	{
		static constexpr std::array<std::pair<std::string_view, JobKind>, 4> known_job_classes{{{"RBX::HeartbeatTask", JobKind::Heartbeat}, {"RBX::PhysicsJob", JobKind::Physics}, {"RBX::ScriptContextFacets::WaitingHybridScriptsJob", JobKind::WaitingHybridScripts}, {"RBX::Studio::RenderJob", JobKind::Render}}};

		m_vtable_to_kind.reserve(known_job_classes.size());
		m_kind_to_vtable.reserve(known_job_classes.size());

		auto* const index = memory::rtti();
		if (!index)
		{
			RML_WARN("Job vtable mapping skipped: no RTTI index available");
			return;
		}

		std::array<std::string_view, known_job_classes.size()> names{};
		std::ranges::transform(known_job_classes, names.begin(), [](const auto& entry) { return entry.first; });
		index->prefetch(names);

		for (const auto& [class_name, job_kind] : known_job_classes)
		{
			const auto vtable = index->find(class_name);
			if (!vtable)
			{
				RML_WARN("RTTI for '{}' not found, skipping vtable mapping", class_name);
				continue;
			}

			m_vtable_to_kind.emplace(*vtable, job_kind);
			m_kind_to_vtable.emplace(job_kind, *vtable);

			RML_INFO("Mapped vtable for '{}' (kind: {}) -> 0x{:X}", class_name, std::to_underlying(job_kind), reinterpret_cast<std::uintptr_t>(*vtable));
		}

		RML_INFO("Initialized vtable mappings: {}/{} job types mapped",
		    m_vtable_to_kind.size(),
		    known_job_classes.size());
	}

	void JobRegistry::cleanup_finished_jobs() noexcept
	{
		std::unique_lock lock(m_jobs_mutex);

		auto it = m_jobs.begin();
		while (it != m_jobs.end())
		{
			if (it->second.job->get_state() == JobState::Destroyed)
			{
				const auto job_name = it->second.job->get_name();
				m_name_to_id.erase(std::string(job_name));
				it = m_jobs.erase(it);
				RML_DEBUG("Cleaned up destroyed job '{}'", job_name);
			}
			else
			{
				++it;
			}
		}
	}

	JobRegistry::JobId JobRegistry::generate_job_id() noexcept
	{
		return m_next_job_id.fetch_add(1, std::memory_order_relaxed);
	}

	void JobRegistry::execute_job_with_stats(JobEntry& entry, const JobExecutionContext& context) noexcept
	{
		const auto start_time = std::chrono::steady_clock::now();

		try
		{
			entry.job->execute(context);
			entry.stats.executions++;
		}
		catch (const std::exception& e)
		{
			entry.stats.failures++;
			RML_ERROR("Job '{}' execution failed: {}", entry.job->get_name(), e.what());
		}
		catch (...)
		{
			entry.stats.failures++;
			RML_ERROR("Job '{}' execution failed with unknown exception", entry.job->get_name());
		}

		const auto end_time       = std::chrono::steady_clock::now();
		const auto execution_time = end_time - start_time;

		entry.stats.total_execution_time += execution_time;
		entry.stats.average_execution_time = entry.stats.total_execution_time / std::max(entry.stats.executions, 1ULL);
		entry.last_execution               = end_time;
	}
}
