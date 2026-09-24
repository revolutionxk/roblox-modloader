#pragma once

#include "RobloxModLoader/hooking/vtable_index.hpp"
#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/property_accessor.hpp"
#include "RobloxModLoader/roblox/signals.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace rml::reflection
{
	struct ClassLayout
	{
		std::size_t size;
		std::size_t align;
		std::size_t virtual_slots;
		void (*construct)(void* memory);
		void (*destroy)(void* object);
		const void* const* base_vtable;
	};

	template<typename Base>
	struct engine_virtual_slots;

	template<>
	struct engine_virtual_slots<RBX::Instance>
	{
		static std::size_t value()
		{
			return vtable_index_of(&RBX::Instance::post_equality_check_name_callback_for_subclass) + 1;
		}
	};

	template<typename Derived>
	class TypedClassBuilder;

	RML_EXPORT RBX::Reflection::Variant make_variant(PropertyType type, const void* value);
	RML_EXPORT void destroy_variant(RBX::Reflection::Variant& variant);
	RML_EXPORT void fire_event(RBX::Instance* instance, std::ptrdiff_t member_offset, const RBX::Reflection::EventArguments& arguments);

	template<typename Class, typename Member>
	std::ptrdiff_t member_offset(Member Class::* member)
	{
		return reinterpret_cast<std::ptrdiff_t>(&(static_cast<Class*>(nullptr)->*member));
	}

	template<typename Derived, typename Base = RBX::Instance>
	class DescribedCreatable : public Base
	{
	public:
		using BaseClass = Base;

		static const RBX::Reflection::ClassDescriptor* class_descriptor()
		{
			return s_descriptor;
		}

		const RBX::Name& get_class_name() const override
		{
			return this->get_descriptor().name;
		}

		static ClassLayout layout()
		{
			return {sizeof(Derived), alignof(Derived), engine_virtual_slots<Base>::value(), &construct, &destroy, base_vtable()};
		}

		template<typename... Args, typename... Values>
		void fire(rbx::signal<void(Args...)> Derived::* member, Values&&... values)
		{
			RBX::Reflection::EventArguments arguments;
			arguments.reserve(sizeof...(Args));
			(push_argument<Args>(arguments, std::forward<Values>(values)), ...);
			fire_event(this, member_offset(member), arguments);
			for (auto& argument : arguments)
				destroy_variant(argument);
		}

	private:
		struct BaseProbe final : Base
		{
		};

		static const void* const* base_vtable()
		{
			alignas(BaseProbe) static std::byte storage[sizeof(BaseProbe)];
			static const void* const* const vtable = *reinterpret_cast<const void* const**>(::new (storage) BaseProbe);
			return vtable;
		}

		template<typename T, typename Value>
		static void push_argument(RBX::Reflection::EventArguments& arguments, Value&& value)
		{
			const T copy(std::forward<Value>(value));
			arguments.push_back(make_variant(property_type_of<T>::value, &copy));
		}

		static void construct(void* memory)
		{
			::new (memory) Derived;
		}

		// Qualified: a virtual call lands in the registry's deleting destructor, which calls back here.
		static void destroy(void* object)
		{
			static_cast<Derived*>(object)->Derived::~Derived();
		}

		static inline const RBX::Reflection::ClassDescriptor* s_descriptor{};

		friend class TypedClassBuilder<Derived>;
	};
}
