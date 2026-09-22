#include "RobloxModLoader/luau/vm/thread_identity.hpp"

#include "RobloxModLoader/luau/generated/layout_access.hpp"
#include "RobloxModLoader/roblox/luau/roblox_extra_space.hpp"
#include "lobject.h"
#include "lstate.h"
#include "pointers.hpp"
#include "utils/seh_guard.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

RML_LOG_SCOPE("ThreadIdentity");

namespace rml::luau::vm
{
	struct IdentityWrite
	{
		lua_State* raw_state;
		const mirror::LuaState* L;
		RBX::Security::Permissions identity;
		std::uint64_t capabilities;
		bool reflect_identity_number;
	};

	// One warning per call site, then at most one more every 30 seconds: a mismatch on a
	// restore is a different problem from a mismatch on an elevation, and a single
	// process-wide flag would hide every later one.
	static void report_bad_identity_context(const std::string_view site, const void* context, const lua_State* L)
	{
		using clock = std::chrono::steady_clock;

		static std::mutex guard;
		static std::unordered_map<std::string_view, clock::time_point> last_report;

		const auto now = clock::now();
		{
			const std::scoped_lock lock(guard);
			const auto it = last_report.find(site);
			if (it != last_report.end() && now - it->second < std::chrono::seconds(30))
				return;

			last_report[site] = now;
		}

		RML_WARN("[{}] rbx_thread_identity_context returned 0x{:X} for state 0x{:X}, which is not a "
		         "ThreadIdentityContext bound to that state; the identity write was skipped",
		    site, reinterpret_cast<std::uintptr_t>(context), reinterpret_cast<std::uintptr_t>(L));
	}

	static void write_identity(void* ctx)
	{
		const auto* call = static_cast<IdentityWrite*>(ctx);

		if (auto* extra_space = static_cast<RBX::Luau::RobloxExtraSpace*>(call->L->userdata))
		{
			extra_space->context.identity = call->identity;
			extra_space->capabilities = call->capabilities;
		}

		if (!call->reflect_identity_number)
			return;

		const auto get_context = g_pointers ? g_pointers->m_roblox_pointers.rbx_thread_identity_context : nullptr;
		if (!get_context || !call->raw_state)
			return;

		auto* identity_context = static_cast<RBX::Luau::ThreadIdentityContext*>(get_context(call->raw_state));
		if (!identity_context)
			return;

		if (static_cast<void*>(identity_context) == call->L->userdata
		    || identity_context->bound_state != call->raw_state)
		{
			report_bad_identity_context("reflect", identity_context, call->raw_state);
			return;
		}

		identity_context->identity.identity = call->identity;
		identity_context->identity.asset_id = 0;
		identity_context->capabilities = call->capabilities;
	}

	static std::uint64_t* mask_for(const std::uint64_t capabilities)
	{
		static std::mutex guard;
		static std::vector<std::unique_ptr<std::uint64_t>> masks;

		const std::scoped_lock lock(guard);
		for (const auto& mask : masks)
			if (*mask == capabilities)
				return mask.get();

		return masks.emplace_back(std::make_unique<std::uint64_t>(capabilities)).get();
	}

	struct ElevateCall
	{
		const void* closure;
		std::uint64_t* mask;
	};

	static void set_proto(mirror::Proto* proto, std::uint64_t* mask)
	{
		if (!proto)
			return;

		proto->userdata = mask;

		if (proto->sizep <= 0 || !proto->p)
			return;

		for (int i = 0; i < proto->sizep; ++i)
			if (proto->p[i])
				set_proto(access::proto(proto->p[i]), mask);
	}

	static void elevate_proto_tree(void* ctx)
	{
		const auto* call = static_cast<ElevateCall*>(ctx);
		const auto* closure = access::closure(call->closure);
		if (closure->isC)
			return;

		set_proto(access::proto(closure->p), call->mask);
	}

	bool set_identity(lua_State* L, const RBX::Security::Permissions identity, const std::uint64_t capabilities,
	                  const bool reflect_identity_number) noexcept
	{
		if (!L)
		{
			RML_ERROR("Cannot set thread identity: Lua state is null");
			return false;
		}

		IdentityWrite call{L, access::state(L), identity, capabilities, reflect_identity_number};
		if (!utils::guarded_invoke(&write_identity, &call))
		{
			RML_ERROR("set_thread_identity faulted; skipping identity set");
			return false;
		}

		RML_INFO("Set thread identity to {} with capabilities 0x{:X}", static_cast<int>(identity), capabilities);
		return true;
	}

	struct ExtraSpaceRead
	{
		const mirror::LuaState* L;
		RBX::Security::Permissions identity;
		std::uint64_t capabilities;
		bool captured;
	};

	static void read_extra_space(void* ctx)
	{
		auto* call = static_cast<ExtraSpaceRead*>(ctx);
		if (!call->L)
			return;

		const auto* extra_space = static_cast<const RBX::Luau::RobloxExtraSpace*>(call->L->userdata);
		if (!extra_space)
			return;

		call->identity = extra_space->context.identity;
		call->capabilities = extra_space->capabilities;
		call->captured = true;
	}

	struct ContextAccess
	{
		lua_State* state;
		std::string_view site;
		RBX::Luau::ThreadIdentityContext* context;
		RBX::Luau::ExtendedIdentity identity;
		std::uint64_t capabilities;
	};

	static void resolve_context(void* ctx)
	{
		auto* call = static_cast<ContextAccess*>(ctx);
		call->context = nullptr;

		const auto get_context = g_pointers ? g_pointers->m_roblox_pointers.rbx_thread_identity_context : nullptr;
		if (!get_context || !call->state)
			return;

		auto* candidate = static_cast<RBX::Luau::ThreadIdentityContext*>(get_context(call->state));
		if (!candidate || candidate->bound_state != call->state)
		{
			report_bad_identity_context(call->site, candidate, call->state);
			return;
		}

		call->identity = candidate->identity;
		call->capabilities = candidate->capabilities;
		call->context = candidate;
	}

	struct ContextWrite
	{
		RBX::Luau::ThreadIdentityContext* context;
		const lua_State* expected_state;
		RBX::Luau::ExtendedIdentity identity;
		std::uint64_t capabilities;
		bool written;
	};

	// Contexts are pooled: writing one that has been rebound to another thread would
	// hand that thread this thread's identity, so the binding is re-checked here.
	static void write_context(void* ctx)
	{
		auto* call = static_cast<ContextWrite*>(ctx);
		if (call->context->bound_state != call->expected_state)
			return;

		call->context->identity = call->identity;
		call->context->capabilities = call->capabilities;
		call->written = true;
	}

	namespace detail
	{
		IdentitySnapshot elevate_scoped(lua_State* L, const RBX::Security::Permissions identity,
		                                const std::uint64_t capabilities) noexcept
		{
			IdentitySnapshot snapshot{};
			if (!L)
				return snapshot;

			// The extra space still has to carry the elevation: that is what RML's own
			// bindings read. Snapshot it first, or the restore leaves FULL_CAPABILITIES
			// behind while Studio sees the defaults again.
			ExtraSpaceRead extra_call{access::state(L), {}, 0, false};
			if (!utils::guarded_invoke(&read_extra_space, &extra_call))
			{
				RML_ERROR("Reading the extra space of state 0x{:X} faulted; its identity will not be restored",
				    reinterpret_cast<std::uintptr_t>(L));
			}

			// Kept even when the write fails: a faulted write may still have landed, and
			// reverting values that were never changed costs nothing.
			snapshot.extra_identity = extra_call.identity;
			snapshot.extra_capabilities = extra_call.capabilities;
			snapshot.extra_captured = extra_call.captured;

			if (!set_identity(L, identity, capabilities, false))
			{
				RML_ERROR("Could not elevate the extra space of state 0x{:X}; RML bindings see the engine's identity",
				    reinterpret_cast<std::uintptr_t>(L));
			}

			ContextAccess access_call{L, "elevate", nullptr, {}, 0};
			if (!utils::guarded_invoke(&resolve_context, &access_call) || !access_call.context)
			{
				RML_ERROR("Could not resolve the identity context of state 0x{:X}; the chunk runs with the engine's "
				          "default identity and Studio-gated APIs such as settings() will fail",
				    reinterpret_cast<std::uintptr_t>(L));
				return snapshot;
			}

			snapshot.context = access_call.context;
			snapshot.identity = access_call.identity;
			snapshot.capabilities = access_call.capabilities;
			snapshot.captured = true;

			// Keep whatever asset the engine attributed the thread to; only the identity
			// number and the capability mask are ours to raise.
			ContextWrite write_call{access_call.context, L, {identity, access_call.identity.asset_id}, capabilities,
			    false};
			if (!utils::guarded_invoke(&write_context, &write_call) || !write_call.written)
			{
				RML_ERROR("Could not elevate identity context 0x{:X} of state 0x{:X}; the chunk runs unelevated",
				    reinterpret_cast<std::uintptr_t>(access_call.context), reinterpret_cast<std::uintptr_t>(L));
				snapshot.context = nullptr;
				snapshot.captured = false;
			}

			return snapshot;
		}

		void restore_identity(lua_State* L, const IdentitySnapshot& snapshot) noexcept
		{
			if (!L)
				return;

			if (snapshot.extra_captured
			    && !set_identity(L, snapshot.extra_identity, snapshot.extra_capabilities, false))
			{
				RML_ERROR("Could not restore the extra space of state 0x{:X}; it stays elevated",
				    reinterpret_cast<std::uintptr_t>(L));
			}

			if (!snapshot.captured || !snapshot.context)
				return;

			// Restore goes back to the context the elevation was written to, not to
			// whatever the engine resolves now: a rebound context belongs to another
			// thread and write_context refuses it.
			ContextWrite write_call{snapshot.context, L, snapshot.identity, snapshot.capabilities, false};
			if (!utils::guarded_invoke(&write_context, &write_call) || !write_call.written)
			{
				RML_ERROR("Could not restore identity context 0x{:X} of state 0x{:X}; it stays elevated for whatever "
				          "thread receives it next",
				    reinterpret_cast<std::uintptr_t>(snapshot.context), reinterpret_cast<std::uintptr_t>(L));
			}
		}
	}

	RBX::Luau::TaskState task_state(lua_State*) noexcept
	{
		return RBX::Luau::TaskState::None;
	}

	void elevate_closure(const Closure* closure, const std::uint64_t capabilities) noexcept
	{
		if (!closure)
			return;

		ElevateCall call{closure, mask_for(capabilities)};
		if (!utils::guarded_invoke(&elevate_proto_tree, &call))
			RML_ERROR("elevate_closure faulted while walking the proto tree; capabilities were not applied");
	}

	void elevate_stack_closure(lua_State* L, const int index, const std::uint64_t capabilities) noexcept
	{
		const auto to_pointer = g_pointers ? g_pointers->m_roblox_pointers.lua_topointer : nullptr;
		if (!L || !to_pointer || !lua_isfunction(L, index))
			return;

		elevate_closure(static_cast<const Closure*>(to_pointer(L, index)), capabilities);
	}
}
