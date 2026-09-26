#pragma once
#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "RobloxModLoader/roblox/job_base.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"

namespace RBX {
    class DataModel;
    class ScriptContext;
}

namespace rml::jobs {
    class DataModelWatcherJob final : public JobBase {
    public:
        DataModelWatcherJob() noexcept;

        ~DataModelWatcherJob() override = default;

        DataModelWatcherJob(const DataModelWatcherJob &) = delete;

        DataModelWatcherJob &operator=(const DataModelWatcherJob &) = delete;

        DataModelWatcherJob(DataModelWatcherJob &&) noexcept = delete;

        DataModelWatcherJob &operator=(DataModelWatcherJob &&) noexcept = delete;

    private:
        bool should_execute_impl(const JobExecutionContext &context) noexcept override;

        void execute_impl(const JobExecutionContext &context) override;

        void destroy_impl() noexcept override;

        static constexpr std::string_view JOB_NAME = "DataModelWatcher";

        static void on_data_model_changed(const RBX::DataModel *old_data_model,
                                          RBX::DataModel *new_data_model,
                                          RBX::ScriptContext *script_context);

        void check_and_cleanup_stale_data_models();

        std::unordered_map<RBX::DataModelType, RBX::DataModel *> m_data_models;
        std::unordered_map<RBX::DataModelType, std::chrono::steady_clock::time_point> m_data_model_last_time_stepped;
        std::chrono::steady_clock::time_point m_last_check = std::chrono::high_resolution_clock::now();
    };
}
