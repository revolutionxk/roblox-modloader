#pragma once

namespace RBX::Security {
    enum Capabilities : std::uint64_t {
        Plugin = 1ULL << 0, // 0x1
        LocalUser = 1ULL << 1, // 0x2
        WritePlayer = 1ULL << 2, // 0x4
        RobloxScript = 1ULL << 3, // 0x8
        RobloxEngine = 1ULL << 4, // 0x10
        NotAccessible = 1ULL << 5, // 0x20

        RunClientScript = 1ULL << 8, // 0x100
        RunServerScript = 1ULL << 9, // 0x200
        AccessOutsideWrite = 1ULL << 11, // 0x800

        Unassigned = 1ULL << 15, // 0x8000
        AssetRequire = 1ULL << 16, // 0x10000
        LoadStringCapability = 1ULL << 17, // 0x20000
        ScriptGlobals = 1ULL << 18, // 0x40000
        CreateInstances = 1ULL << 19, // 0x80000
        Basic = 1ULL << 20, // 0x100000
        Audio = 1ULL << 21, // 0x200000
        DataStore = 1ULL << 22, // 0x400000
        Network = 1ULL << 23, // 0x800000
        Physics = 1ULL << 24, // 0x1000000
        UI = 1ULL << 25, // 0x2000000
        CSG = 1ULL << 26, // 0x4000000
        Chat = 1ULL << 27, // 0x8000000
        Animation = 1ULL << 28, // 0x10000000
        Avatar = 1ULL << 29, // 0x20000000
        Input = 1ULL << 30, // 0x40000000
        Environment = 1ULL << 31, // 0x80000000
        RemoteEvent = 1ULL << 32, // 0x100000000
        LegacySound = 1ULL << 33, // 0x200000000
        Players = 1ULL << 34, // 0x400000000
        CapabilityControl = 1ULL << 35, // 0x800000000

        InternalTest = 1ULL << 60, // 0x1000000000000000
        PluginOrOpenCloud = 1ULL << 61, // 0x2000000000000000
        Assistant = 1ULL << 62, // 0x4000000000000000

        Restricted = 0xFFFFFFFFFFFFFFFFULL
    };

    struct Protection {
        Capabilities capabilities{};
    };

    constexpr std::uint64_t FULL_CAPABILITIES = Restricted;

    constexpr std::uint64_t BASIC_SCRIPT_CAPABILITIES =
            Plugin | LocalUser | RobloxScript | RunClientScript | RunServerScript | Basic;

    constexpr std::uint64_t PLUGIN_CAPABILITIES =
            Plugin | LocalUser | WritePlayer | RobloxEngine | CreateInstances | Basic |
            Audio | Network | Physics | UI | CSG | Chat | Animation | Avatar;

    enum class Permissions : std::uint32_t {
        None = 0,
        Plugin = 1,
        RobloxPlace = 2,
        LocalUser = 3,
        WritePlayer = 4,
        RobloxScript = 5,
        Roblox = 6,
        RobloxExecutor = 7,
        RobloxEngine = 8,
    };

    enum class Identity : std::uint64_t {
        Default = 0,
        StudioPlugin = 1,
        RobloxScript = 2,
        RobloxEngine = 3, // This is also used by the ElevatedStudioPlugin
    };
}
