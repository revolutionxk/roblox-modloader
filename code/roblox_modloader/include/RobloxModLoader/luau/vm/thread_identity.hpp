#pragma once

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/roblox/luau/roblox_extra_space.hpp"
#include "RobloxModLoader/roblox/security/script_permissions.hpp"

#include <utility>

struct Closure;

namespace rml::luau::vm
{
	bool set_identity(lua_State* L, RBX::Security::Permissions identity, std::uint64_t capabilities,
	                  bool reflect_identity_number = true) noexcept;

	/// What the engine had in a thread's identity context and extra space before RML
	/// touched it, plus the exact context object the elevation was written to: contexts
	/// are pooled, so the restore has to go back to that object and only while it is
	/// still bound to the same thread.
	struct IdentitySnapshot
	{
		RBX::Luau::ThreadIdentityContext* context{};
		RBX::Luau::ExtendedIdentity identity{};
		std::uint64_t capabilities{};
		bool captured{};

		RBX::Security::Permissions extra_identity{};
		std::uint64_t extra_capabilities{};
		bool extra_captured{};
	};

	namespace detail
	{
		/// Elevates a thread the way Studio reads it (the identity context, not only the
		/// extra space) and hands back what was there. Not part of the public surface:
		/// an elevation that is never restored leaks the privilege into whatever thread
		/// receives that pooled context next, so callers take ScopedIdentity instead.
		[[nodiscard]] IdentitySnapshot elevate_scoped(lua_State* L, RBX::Security::Permissions identity,
		                                              std::uint64_t capabilities) noexcept;

		void restore_identity(lua_State* L, const IdentitySnapshot& snapshot) noexcept;
	}

	/// Owns one elevation for as long as the thread runs. Movable, so a chunk that
	/// yields can hand the elevation to whoever owns the parked thread and keep running
	/// elevated when the engine resumes it; the restore happens when this object dies,
	/// on every path including an exception.
	class ScopedIdentity final
	{
	public:
		ScopedIdentity() noexcept = default;

		ScopedIdentity(lua_State* L, const RBX::Security::Permissions identity,
		               const std::uint64_t capabilities) noexcept :
		    m_state(L),
		    m_snapshot(detail::elevate_scoped(L, identity, capabilities))
		{
		}

		ScopedIdentity(const ScopedIdentity&) = delete;
		ScopedIdentity& operator=(const ScopedIdentity&) = delete;

		ScopedIdentity(ScopedIdentity&& other) noexcept :
		    m_state(std::exchange(other.m_state, nullptr)),
		    m_snapshot(std::exchange(other.m_snapshot, IdentitySnapshot{}))
		{
		}

		ScopedIdentity& operator=(ScopedIdentity&& other) noexcept
		{
			if (this != &other)
			{
				restore();
				m_state = std::exchange(other.m_state, nullptr);
				m_snapshot = std::exchange(other.m_snapshot, IdentitySnapshot{});
			}

			return *this;
		}

		~ScopedIdentity() { restore(); }

		/// Hands the identity back now instead of at destruction, for the callers that
		/// have to un-elevate before the thread's anchor goes away.
		void restore() noexcept
		{
			if (!m_state)
			{
				return;
			}

			auto* state = std::exchange(m_state, nullptr);
			const auto snapshot = std::exchange(m_snapshot, IdentitySnapshot{});
			detail::restore_identity(state, snapshot);
		}

	private:
		lua_State* m_state{nullptr};
		IdentitySnapshot m_snapshot{};
	};

	[[nodiscard]] RBX::Luau::TaskState task_state(lua_State* L) noexcept;

	void elevate_closure(const Closure* closure, std::uint64_t capabilities) noexcept;

	void elevate_stack_closure(lua_State* L, int index, std::uint64_t capabilities) noexcept;
}
