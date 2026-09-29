#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

namespace rbx
{
	template<typename T>
	struct intrusive_ptr_target
	{
		std::int32_t strong;
		std::int32_t weak;
	};

	template<typename T>
	class intrusive_weak_ptr
	{
	public:
		constexpr intrusive_weak_ptr() noexcept = default;

		explicit intrusive_weak_ptr(T* target) noexcept :
		    m_target(target)
		{
		}

		intrusive_weak_ptr(const intrusive_weak_ptr& other) noexcept :
		    m_target(other.m_target)
		{
			acquire();
		}

		intrusive_weak_ptr(intrusive_weak_ptr&& other) noexcept :
		    m_target(std::exchange(other.m_target, nullptr))
		{
		}

		intrusive_weak_ptr& operator=(const intrusive_weak_ptr& other) noexcept
		{
			if (m_target != other.m_target)
			{
				T* previous = m_target;
				m_target = other.m_target;
				acquire();
				release(previous);
			}
			return *this;
		}

		intrusive_weak_ptr& operator=(intrusive_weak_ptr&& other) noexcept
		{
			if (this != &other)
			{
				release(m_target);
				m_target = std::exchange(other.m_target, nullptr);
			}
			return *this;
		}

		~intrusive_weak_ptr()
		{
			release(m_target);
		}

		[[nodiscard]] static intrusive_weak_ptr observe(T* target) noexcept
		{
			intrusive_weak_ptr result(target);
			result.acquire();
			return result;
		}

		[[nodiscard]] T* get() const noexcept
		{
			return m_target;
		}

		explicit operator bool() const noexcept
		{
			return m_target != nullptr;
		}

		[[nodiscard]] bool alive() const noexcept
		{
			return m_target && std::atomic_ref(m_target->strong).load(std::memory_order_acquire) > 0;
		}

		bool operator==(const intrusive_weak_ptr& other) const noexcept = default;

	private:
		void acquire() const noexcept
		{
			if (m_target)
				std::atomic_ref(m_target->weak).fetch_add(1, std::memory_order_seq_cst);
		}

		static void release(T* target) noexcept
		{
			if (target && std::atomic_ref(target->weak).fetch_sub(1, std::memory_order_seq_cst) == 1)
				intrusive_weak_ptr_free(target);
		}

		T* m_target = nullptr;
	};
}

namespace boost
{
	template<typename T>
	class intrusive_ptr
	{
	public:
		T* px{};

		constexpr intrusive_ptr() noexcept = default;
		intrusive_ptr(const intrusive_ptr&) = delete;
		intrusive_ptr& operator=(const intrusive_ptr&) = delete;

		~intrusive_ptr()
		{
			if (px)
				intrusive_ptr_release(px);
		}

		[[nodiscard]] T* get() const noexcept
		{
			return px;
		}

		T* operator->() const noexcept
		{
			return px;
		}

		explicit operator bool() const noexcept
		{
			return px != nullptr;
		}
	};
}
