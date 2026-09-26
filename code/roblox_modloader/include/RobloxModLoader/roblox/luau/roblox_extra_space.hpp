#pragma once
#include "RobloxModLoader/roblox/security/script_permissions.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

namespace RBX {
    class ScriptContext;
}

namespace RBX {
    class Actor;
    class Script;
}

namespace RBX::Luau {
    typedef int64_t (*CapabilityValidator)(int64_t capability, lua_State *L);

    struct ExtendedIdentity {
        Security::Permissions identity;
        uint64_t asset_id;
    };

    class ThreadIdentityContext {
    public:
        ExtendedIdentity identity;

    private:
        std::byte padding_0[0x10];

    public:
        lua_State *bound_state;

    private:
        std::byte padding_1[0x8];

    public:
        uint64_t capabilities;
        void *capability_deriver;

    private:
        RML_LAYOUT_GUARD_BEGIN()
            RML_ASSERT_LAYOUT_OFFSET(ThreadIdentityContext, identity, 0x00);
            RML_ASSERT_LAYOUT_OFFSET(ThreadIdentityContext, bound_state, 0x20);
            RML_ASSERT_LAYOUT_OFFSET(ThreadIdentityContext, capabilities, 0x30);
            RML_ASSERT_LAYOUT_OFFSET(ThreadIdentityContext, capability_deriver, 0x38);
        RML_LAYOUT_GUARD_END()
    };


    enum TaskState : std::int8_t {
        None = 0,
        Deferred = 1,
        Delayed = 2,
        Waiting = 3,
    };

    class RobloxExtraSpace {
        struct Shared {
            int32_t thread_count;
            ScriptContext *context;
            uintptr_t *weak_ref;
            uintptr_t *intrusive_hook_all_threads;
        };

        struct WeakRef {
            void *pointer;
            void *control_block;
        };

        std::byte padding_0[0x18];

    public:
        Shared *shared;

    private:
        std::byte padding_1[0x8];

    public:
        CapabilityValidator *capabilities_validator;
        ExtendedIdentity context;

    private:
        std::byte padding_2[0x18];

    public:
        uint64_t capabilities;

    private:
        std::byte padding_3[0x8];

    public:
        WeakRef weak_refs[4];

    private:
        std::byte padding_4[0x10];
    };

    RML_LAYOUT_DIAGNOSTIC_PUSH()
    RML_ASSERT_OFFSET(RobloxExtraSpace, shared, 0x18);
    RML_ASSERT_OFFSET(RobloxExtraSpace, capabilities_validator, 0x28);
    RML_ASSERT_OFFSET(RobloxExtraSpace, context, 0x30);
    RML_ASSERT_OFFSET(RobloxExtraSpace, capabilities, 0x58);
    RML_ASSERT_OFFSET(RobloxExtraSpace, weak_refs, 0x68);
    RML_ASSERT_SIZE(RobloxExtraSpace, 0xB8);
    RML_LAYOUT_DIAGNOSTIC_POP()
}
