#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <concepts>

namespace RBX {
    class TaskScheduler;
    class DataModelJob;
    struct Stats;
}

namespace rml {
    enum class JobKind : std::uint8_t {
        None = 0,
        Heartbeat = 1 << 0,
        Physics = 1 << 1,
        Render = 1 << 2,
        WaitingHybridScripts = 1 << 3,
        Custom = 1 << 7
    };

    constexpr JobKind operator|(JobKind lhs, JobKind rhs) noexcept {
        return static_cast<JobKind>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }

    constexpr JobKind operator&(JobKind lhs, JobKind rhs) noexcept {
        return static_cast<JobKind>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
    }

    constexpr JobKind operator^(JobKind lhs, JobKind rhs) noexcept {
        return static_cast<JobKind>(static_cast<std::uint8_t>(lhs) ^ static_cast<std::uint8_t>(rhs));
    }

    constexpr JobKind operator~(JobKind kind) noexcept {
        return static_cast<JobKind>(~static_cast<std::uint8_t>(kind));
    }

    constexpr JobKind &operator|=(JobKind &lhs, JobKind rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr JobKind &operator&=(JobKind &lhs, JobKind rhs) noexcept {
        lhs = lhs & rhs;
        return lhs;
    }

    constexpr JobKind &operator^=(JobKind &lhs, JobKind rhs) noexcept {
        lhs = lhs ^ rhs;
        return lhs;
    }

    constexpr bool has_job_kind(JobKind target_kinds, JobKind kind) noexcept {
        return (target_kinds & kind) != JobKind::None;
    }

    constexpr std::string_view job_kind_to_string(JobKind kind) noexcept {
        switch (kind) {
            case JobKind::Heartbeat: return "Heartbeat";
            case JobKind::Physics: return "Physics";
            case JobKind::Render: return "Render";
            case JobKind::WaitingHybridScripts: return "WaitingHybridScripts";
            case JobKind::Custom: return "Custom";
            case JobKind::None: return "None";
            default: return "Unknown";
        }
    }

    enum class JobPriority : std::int32_t {
        Critical = -100,
        High = -50,
        Normal = 0,
        Low = 50,
        Background = 100
    };

    enum class JobState : std::uint8_t {
        Idle,
        Running,
        Finished,
        Destroyed
    };

    struct JobExecutionContext {
        JobKind kind;
        RBX::DataModelJob *job;
        const RBX::Stats *stats;
        double delta_time;

        template<typename T>
        T *job_as() const {
            return static_cast<T *>(job);
        }
    };

    template<typename T>
    concept JobImplementation = requires(T job, const JobExecutionContext &context)
    {
        { job.should_execute(context) } -> std::convertible_to<bool>;
        { job.execute(context) } -> std::same_as<void>;
        { job.get_name() } -> std::convertible_to<std::string_view>;
        { job.get_priority() } -> std::convertible_to<JobPriority>;
    };
}
