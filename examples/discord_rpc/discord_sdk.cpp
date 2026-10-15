#include "discord_sdk.hpp"
#include <chrono>

namespace discord_integration {
    DiscordGameSDK::DiscordGameSDK(const std::shared_ptr<spdlog::logger> &logger)
        : core(nullptr), initialized(false), running(false), logger(logger) {
    }

    DiscordGameSDK::~DiscordGameSDK() {
        shutdown();
    }

    bool DiscordGameSDK::initialize(const discord::ClientId client_id) {
        if (initialized.load()) {
            return true;
        }

        auto result = discord::Core::Create(client_id, DiscordCreateFlags_Default, &core);
        if (result != discord::Result::Ok) {
            logger->error("Failed to create Discord core: {}", static_cast<int>(result));
            return false;
        }

        core->SetLogHook(discord::LogLevel::Debug, [this](discord::LogLevel level, const char *message) {
            log_problems(logger, level, message);
        });

        initialized.store(true);
        running.store(true);

        callback_thread = std::thread(&DiscordGameSDK::callback_loop, this);

        logger->info("Discord Game SDK initialized successfully");
        return true;
    }

    void DiscordGameSDK::shutdown() {
        if (!initialized.load()) {
            return;
        }

        running.store(false);

        if (callback_thread.joinable()) {
            callback_thread.join();
        }

        std::lock_guard lock(core_mutex);
        if (core) {
            delete core;
            core = nullptr;
        }

        initialized.store(false);
    }

    void DiscordGameSDK::update_activity(const ActivityDetails &activity) const {
        if (!initialized.load() || !core) {
            return;
        }

        discord::Activity discord_activity{};

        if (!activity.state.empty()) {
            discord_activity.SetState(activity.state.c_str());
        }

        if (!activity.details.empty()) {
            discord_activity.SetDetails(activity.details.c_str());
        }

        if (activity.start_timestamp > 0) {
            discord_activity.GetTimestamps().SetStart(activity.start_timestamp);
        }

        if (activity.end_timestamp > 0) {
            discord_activity.GetTimestamps().SetEnd(activity.end_timestamp);
        }

        if (!activity.large_image_key.empty()) {
            discord_activity.GetAssets().SetLargeImage(activity.large_image_key.c_str());
        }

        if (!activity.large_image_text.empty()) {
            discord_activity.GetAssets().SetLargeText(activity.large_image_text.c_str());
        }

        if (!activity.small_image_key.empty()) {
            discord_activity.GetAssets().SetSmallImage(activity.small_image_key.c_str());
        }

        if (!activity.small_image_text.empty()) {
            discord_activity.GetAssets().SetSmallText(activity.small_image_text.c_str());
        }

        if (activity.party_size > 0 && activity.party_max > 0) {
            discord_activity.GetParty().GetSize().SetCurrentSize(activity.party_size);
            discord_activity.GetParty().GetSize().SetMaxSize(activity.party_max);
        }

        discord_activity.SetType(static_cast<discord::ActivityType>(activity.type));

        std::lock_guard lock(core_mutex);
        core->ActivityManager().UpdateActivity(discord_activity, [this](discord::Result result) {
            if (result != discord::Result::Ok) {
                logger->warn("Failed to update Discord activity: {}", static_cast<int>(result));
            }
        });
    }

    void DiscordGameSDK::clear_activity() const {
        if (!initialized.load() || !core) {
            return;
        }

        std::lock_guard lock(core_mutex);
        core->ActivityManager().ClearActivity([this](discord::Result result) {
            if (result != discord::Result::Ok) {
                logger->warn("Failed to clear Discord activity: {}", static_cast<int>(result));
            }
        });
    }

    void DiscordGameSDK::run_callbacks() const {
        if (!initialized.load() || !core) {
            return;
        }

        std::lock_guard lock(core_mutex);
        core->RunCallbacks();
    }

    void DiscordGameSDK::callback_loop() const {
        while (running.load()) {
            run_callbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void DiscordGameSDK::log_problems(const std::shared_ptr<spdlog::logger> &logger, const discord::LogLevel level,
                                      const char *message) {
        switch (level) {
            case discord::LogLevel::Error:
                logger->error("[Discord SDK] {}", message);
                break;
            case discord::LogLevel::Warn:
                logger->warn("[Discord SDK] {}", message);
                break;
            case discord::LogLevel::Info:
                logger->info("[Discord SDK] {}", message);
                break;
            case discord::LogLevel::Debug:
                logger->debug("[Discord SDK] {}", message);
                break;
        }
    }
}
