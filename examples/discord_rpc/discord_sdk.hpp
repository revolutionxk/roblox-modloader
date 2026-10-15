#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#include "discord.h"
#include "spdlog/spdlog.h"


namespace discord_integration {
    enum class ActivityType {
        Playing,
        Streaming,
        Listening,
        Watching,
        Custom,
        Competing
    };

    inline ActivityType activity_from_string(const std::string &type_str) {
        if (type_str == "Playing") return ActivityType::Playing;
        if (type_str == "Streaming") return ActivityType::Streaming;
        if (type_str == "Listening") return ActivityType::Listening;
        if (type_str == "Watching") return ActivityType::Watching;
        if (type_str == "Custom") return ActivityType::Custom;
        if (type_str == "Competing") return ActivityType::Competing;
        return ActivityType::Playing;
    }

    struct ActivityDetails {
        std::string state;
        std::string details;
        std::string large_image_key;
        std::string large_image_text;
        std::string small_image_key;
        std::string small_image_text;
        std::int64_t start_timestamp = 0;
        std::int64_t end_timestamp = 0;
        std::int32_t party_size = 0;
        std::int32_t party_max = 0;
        ActivityType type = ActivityType::Playing;
    };

    class DiscordGameSDK {
    public:
        explicit DiscordGameSDK(const std::shared_ptr<spdlog::logger> &logger);

        ~DiscordGameSDK();

        bool initialize(discord::ClientId client_id);

        void shutdown();

        void update_activity(const ActivityDetails &activity) const;

        void clear_activity() const;

        void run_callbacks() const;

        bool is_initialized() const { return initialized; }

    private:
        discord::Core *core;
        mutable std::mutex core_mutex;
        std::atomic<bool> initialized;
        std::atomic<bool> running;
        std::thread callback_thread;
        std::shared_ptr<spdlog::logger> logger;

        void callback_loop() const;

        static void log_problems(const std::shared_ptr<spdlog::logger> &logger, discord::LogLevel level,
                                 const char *message);
    };
}
