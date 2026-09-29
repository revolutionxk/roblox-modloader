#pragma once

#include "RobloxModLoader/roblox/signals.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "member.hpp"
#include "type.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace RBX::Reflection
{
	typedef std::vector<Variant> EventArguments;

	class GenericSlotWrapper
	{
	public:
		virtual ~GenericSlotWrapper() = default;

		virtual bool use_submit_task()
		{
			return false;
		}

		virtual void deliver(const EventArguments& args) = 0;

		virtual void execute_once()
		{
		}

		virtual EventArguments build_args()
		{
			return EventArguments();
		}
	};

	struct GenericSlotFunctor
	{
		std::shared_ptr<GenericSlotWrapper> wrapper;
	};

	class Event;
	class EventSource;

	class EventDescriptor : public MemberDescriptor
	{
	public:
		typedef Event ConstMember;
		typedef Event Member;

	public:
		SignatureDescriptor signature;

		bool operator==(const EventDescriptor& other) const
		{
			return this == &other;
		}
		bool operator!=(const EventDescriptor& other) const
		{
			return this != &other;
		}

		const SignatureDescriptor& get_signature() const
		{
			return signature;
		}

		[[nodiscard]] bool is_public() const
		{
			return is_scriptable();
		}

		virtual rbx::signals::connection connect_generic(EventSource* source, std::shared_ptr<GenericSlotWrapper> wrapper) const = 0;
		virtual bool is_scriptable() const = 0;
		virtual bool is_broadcast() const = 0;
		virtual int get_send_mode() const = 0;
		virtual void* get_latched_signal(EventSource* source) const = 0;
		virtual void fire_event_generic(EventSource* source, const EventArguments& args) const = 0;
		virtual bool has_event_connections(EventSource* source) const = 0;
		virtual void disconnect_all(EventSource* source) const = 0;

		[[nodiscard]] rbx::signals::slots_holder* get_signal(EventSource* source) const;
		[[nodiscard]] std::vector<rbx::signals::connection> snapshot_connections(EventSource* source) const;

	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(EventDescriptor, 0x78);
	RML_ASSERT_OFFSET(EventDescriptor, signature, 0x48);
	RML_LAYOUT_DIAGNOSTIC_POP()
	
	class EventDesc : public EventDescriptor
	{
	public:
#if defined(RML_WINDOWS)
		std::int32_t signal;
#else
		std::ptrdiff_t signal;
#endif
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(EventDesc, signal, 0x78);
	RML_LAYOUT_DIAGNOSTIC_POP()
}