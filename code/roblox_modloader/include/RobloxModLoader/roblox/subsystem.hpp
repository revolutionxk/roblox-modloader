#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <atomic>
#include <cstdint>

namespace RBX
{
	namespace subsysDetail
	{
		class SubsystemHandleImpl
		{
		public:
			struct RecursiveSpinMutex
			{
				std::uint64_t owner;
				std::uint64_t depth;
			};

			virtual SubsystemHandleImpl* acquire() = 0;
			virtual SubsystemHandleImpl* acquire_weak() = 0;
			virtual void release() = 0;
			virtual void on_scope_exit() = 0;
			virtual const char* get_name() const = 0;

			RecursiveSpinMutex mutex;

		protected:
			SubsystemHandleImpl() = delete;
			~SubsystemHandleImpl() = default;
		};
	}

	template<typename T>
	class SubsystemHandle : public subsysDetail::SubsystemHandleImpl
	{
	public:
		T* instance;
		std::atomic<std::uint32_t> state;

		T* get() const
		{
			return instance;
		}

		T* operator->() const
		{
			return instance;
		}

	private:
		SubsystemHandle() = delete;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(subsysDetail::SubsystemHandleImpl, mutex, 8);
	RML_ASSERT_OFFSET(SubsystemHandle<int>, instance, 24);
	RML_ASSERT_OFFSET(SubsystemHandle<int>, state, 32);
	RML_ASSERT_SIZE(SubsystemHandle<int>, 40);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
