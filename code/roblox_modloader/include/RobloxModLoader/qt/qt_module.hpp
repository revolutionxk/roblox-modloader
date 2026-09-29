#pragma once

#include <cstddef>
#include <initializer_list>
#include <new>
#include <utility>

namespace rml::qt::detail
{
	[[nodiscard]] void* core_export(const char* signature);
	[[nodiscard]] void* widgets_export(const char* signature);
	[[nodiscard]] void* gui_export(const char* signature);
	[[nodiscard]] void* core_export_optional(const char* signature);
	[[nodiscard]] void* widgets_export_optional(const char* signature);
	[[nodiscard]] void* gui_export_optional(const char* signature);
	[[nodiscard]] void* core_export(std::initializer_list<const char*> signatures);
	[[nodiscard]] void* widgets_export(std::initializer_list<const char*> signatures);
	[[nodiscard]] void* gui_export(std::initializer_list<const char*> signatures);

	template<typename Fn>
	[[nodiscard]] Fn core(const char* signature)
	{
		return reinterpret_cast<Fn>(core_export(signature));
	}

	template<typename Fn>
	[[nodiscard]] Fn widgets(const char* signature)
	{
		return reinterpret_cast<Fn>(widgets_export(signature));
	}

	template<typename Fn>
	[[nodiscard]] Fn gui(const char* signature)
	{
		return reinterpret_cast<Fn>(gui_export(signature));
	}

	template<typename Fn>
	[[nodiscard]] Fn core_optional(const char* signature)
	{
		return reinterpret_cast<Fn>(core_export_optional(signature));
	}

	template<typename Fn>
	[[nodiscard]] Fn widgets_optional(const char* signature)
	{
		return reinterpret_cast<Fn>(widgets_export_optional(signature));
	}

	template<typename Fn>
	[[nodiscard]] Fn gui_optional(const char* signature)
	{
		return reinterpret_cast<Fn>(gui_export_optional(signature));
	}

	template<typename Fn>
	[[nodiscard]] Fn core(std::initializer_list<const char*> signatures)
	{
		return reinterpret_cast<Fn>(core_export(signatures));
	}

	template<typename Fn>
	[[nodiscard]] Fn widgets(std::initializer_list<const char*> signatures)
	{
		return reinterpret_cast<Fn>(widgets_export(signatures));
	}

	template<typename Fn>
	[[nodiscard]] Fn gui(std::initializer_list<const char*> signatures)
	{
		return reinterpret_cast<Fn>(gui_export(signatures));
	}

	template<typename T, typename Ctor, typename... Args>
	[[nodiscard]] T* heap_construct(const std::size_t size, Ctor ctor, Args&&... args)
	{
		if (!ctor)
			return nullptr;

		void* memory = ::operator new(size);
		ctor(memory, std::forward<Args>(args)...);
		return static_cast<T*>(memory);
	}

	template<typename T, typename Dtor>
	void heap_destroy(Dtor dtor, T* instance)
	{
		if (!instance)
			return;

		if (dtor)
			dtor(instance);
		::operator delete(instance);
	}
}
