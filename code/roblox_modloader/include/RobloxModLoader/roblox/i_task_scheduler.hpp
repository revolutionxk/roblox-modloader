#pragma once
#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "job.hpp"

namespace RBX {
    class DataModel;
    class ScriptContext;
}

namespace rml {
    class ITaskScheduler {
    public:
        using JobId = std::uint64_t;
        using JobPtr = std::unique_ptr<IJob>;

        struct JobStats {
            std::uint64_t executions = 0;
            std::uint64_t failures = 0;
            std::chrono::nanoseconds total_execution_time{0};
            std::chrono::nanoseconds average_execution_time{0};
        };

        virtual ~ITaskScheduler() = default;

        ITaskScheduler(const ITaskScheduler &) = delete;

        ITaskScheduler &operator=(const ITaskScheduler &) = delete;

        ITaskScheduler(ITaskScheduler &&) noexcept = delete;

        ITaskScheduler &operator=(ITaskScheduler &&) noexcept = delete;

        virtual std::expected<JobId, std::string> register_job(JobPtr job) noexcept = 0;

        virtual bool unregister_job(JobId job_id) noexcept = 0;

        virtual bool unregister_job(std::string_view job_name) noexcept = 0;

        virtual void execute_jobs_for_kind(const JobExecutionContext &context) noexcept = 0;

        virtual std::optional<std::reference_wrapper<IJob> > get_job(JobId job_id) const noexcept = 0;

        virtual std::optional<std::reference_wrapper<IJob> > get_job(std::string_view job_name) const noexcept = 0;

        virtual std::vector<JobId> get_jobs_by_kind(JobKind kind) const noexcept = 0;

        virtual std::size_t get_job_count() const noexcept = 0;

        virtual std::optional<JobStats> get_job_stats(JobId job_id) const noexcept = 0;

        virtual void reset_stats() noexcept = 0;

        virtual void shutdown() noexcept = 0;

        virtual bool is_shutdown() const noexcept = 0;

        virtual std::optional<JobKind> get_job_kind_from_vtable(void **vtable) const noexcept = 0;

        virtual std::optional<void **> get_vtable_for_job_kind(JobKind kind) const noexcept = 0;

        virtual void set_data_model(RBX::DataModelType type, RBX::DataModel *data_model, RBX::ScriptContext *script_context) = 0;

        virtual const RBX::DataModel *get_data_model_by_type(RBX::DataModelType type) noexcept = 0;

        virtual void cleanup_data_model(RBX::DataModelType data_model_type) = 0;

    protected:
        ITaskScheduler() = default;
    };
}
