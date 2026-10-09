#pragma once
#include "RobloxModLoader/roblox/security/script_permissions.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <memory>

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
}

class RobloxExtraSpace {
public:
    struct Shared {
        int32_t thread_count;
        RBX::ScriptContext *context;
        uintptr_t *weak_ref;
        uintptr_t *intrusive_hook_all_threads;
    };

    struct WeakRef {
        void *pointer;
        void *control_block;
    };

private:
    [[maybe_unused]] void *reserved_0[3];

public:
    std::shared_ptr<Shared> shared;

private:
    [[maybe_unused]] void *reserved_28;
    [[maybe_unused]] WeakRef reserved_30;

public:
    uint64_t capabilities;

private:
    WeakRef reserved_weak_ref_0;

public:
    RBX::Luau::ExtendedIdentity context;

private:
    std::byte padding_2[0x10];

    WeakRef reserved_weak_ref_1;
    WeakRef reserved_weak_ref_2;
    WeakRef reserved_weak_ref_3;
    std::byte padding_3[0x10];

    RML_LAYOUT_GUARD_BEGIN()
    RML_ASSERT_OFFSET(RobloxExtraSpace, reserved_28, 0x28);
    RML_ASSERT_OFFSET(RobloxExtraSpace, reserved_weak_ref_0, 0x48);
    RML_ASSERT_OFFSET(RobloxExtraSpace, reserved_weak_ref_1, 0x78);
    RML_ASSERT_OFFSET(RobloxExtraSpace, reserved_weak_ref_3, 0x98);
    RML_LAYOUT_GUARD_END()
};

RML_LAYOUT_DIAGNOSTIC_PUSH()
RML_ASSERT_OFFSET(RobloxExtraSpace, shared, 0x18);
RML_ASSERT_OFFSET(RobloxExtraSpace, capabilities, 0x40);
RML_ASSERT_OFFSET(RobloxExtraSpace, context, 0x58);
RML_ASSERT_SIZE(RobloxExtraSpace, 0xB8);
RML_LAYOUT_DIAGNOSTIC_POP()
