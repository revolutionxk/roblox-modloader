#pragma once

#include "RobloxModLoader/internal/engine_abi.hpp"
#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/roblox/util/intrusive_ptr.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace rbx::signals
{
	struct slots_holder;

	struct slot_base : intrusive_ptr_target<slot_base>
	{
		void* do_call;
		slot_base* next;
		std::uintptr_t prev_and_flags;
		slots_holder* holder;
		void(RML_ENGINE_CALL* do_dtor)(slot_base*);
	};

	template<typename Functor>
	struct slot_with_functor : slot_base
	{
		Functor functor;
	};

	struct slots_holder : intrusive_ptr_target<slots_holder>
	{
		slot_base* head;
		std::uint32_t reserved_10;
		std::uint32_t reserved_14;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(slot_base, strong, 0x0);
	RML_ASSERT_OFFSET(slot_base, weak, 0x4);
	RML_ASSERT_OFFSET(slot_base, do_call, 0x8);
	RML_ASSERT_OFFSET(slot_base, next, 0x10);
	RML_ASSERT_OFFSET(slot_base, prev_and_flags, 0x18);
	RML_ASSERT_OFFSET(slot_base, holder, 0x20);
	RML_ASSERT_OFFSET(slot_base, do_dtor, 0x28);
	RML_ASSERT_SIZE(slot_base, 0x30);
	RML_ASSERT_OFFSET(slots_holder, head, 0x8);
	RML_ASSERT_OFFSET(slots_holder, reserved_10, 0x10);
	RML_LAYOUT_DIAGNOSTIC_POP()

	RML_EXPORT void intrusive_ptr_release(slots_holder* holder) noexcept;
	RML_EXPORT void intrusive_weak_ptr_free(slot_base* slot) noexcept;

	class connection
	{
	public:
		connection() noexcept = default;

		explicit connection(intrusive_weak_ptr<slot_base> slot) noexcept :
		    m_slot(std::move(slot))
		{
		}

		[[nodiscard]] static connection observe(slot_base* slot) noexcept
		{
			return connection(intrusive_weak_ptr<slot_base>::observe(slot));
		}

		void disconnect() const;

		[[nodiscard]] slot_base* raw_slot() const noexcept
		{
			return m_slot.get();
		}

		[[nodiscard]] bool connected() const noexcept
		{
			return m_slot.alive();
		}

		bool operator==(const connection& other) const noexcept = default;

	private:
		intrusive_weak_ptr<slot_base> m_slot;
	};

	static_assert(sizeof(connection) == sizeof(void*), "connection must stay a single-pointer handle");
}

namespace rbx
{
	template<typename Signature>
	class signal
	{
	public:
		boost::intrusive_ptr<signals::slots_holder> holder;

		[[nodiscard]] bool empty() const noexcept
		{
			return !holder || !holder->head;
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(signal<void()>, 8);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
