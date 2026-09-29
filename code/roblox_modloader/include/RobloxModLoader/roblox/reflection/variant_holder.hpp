#pragma once

#include <cstring>
#include <new>
#include <utility>

namespace RBX::Reflection::detail
{
	struct holder
	{
		void (*construct_func)(const char* source, char* storage);
		void (*move_construct_func)(char* source, char* storage);
		void (*destruct_func)(char* storage);
	};

	template<typename T>
	struct typed_holder
	{
		static void construct_func(const char* source, char* storage)
		{
			::new (storage) T(*reinterpret_cast<const T*>(source));
		}

		static void move_construct_func(char* source, char* storage)
		{
			::new (storage) T(std::move(*reinterpret_cast<T*>(source)));
		}

		static void destruct_func(char* storage)
		{
			reinterpret_cast<T*>(storage)->~T();
		}

		static const holder* singleton() noexcept
		{
			static constexpr holder s{&construct_func, &move_construct_func, &destruct_func};
			return &s;
		}
	};
}
