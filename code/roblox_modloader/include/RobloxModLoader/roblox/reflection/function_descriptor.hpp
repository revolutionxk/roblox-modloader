#pragma once

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "enum_descriptor.hpp"
#include "member.hpp"
#include "type.hpp"

#include <cstdint>
#include <cstring>
#include <utility>

class Function;

namespace G3D
{
	class Vector3;
	class Vector3int16;
	class Rect2D;
}

namespace RBX::Reflection
{
	class DescribedBase;

	class FunctionDescriptor : public MemberDescriptor
	{
	public:
		typedef Function ConstMember;
		typedef Function Member;

		struct Attributes : Descriptor::Attributes
		{
			bool is_no_mutation{false};
		};

		class Arguments
		{
		public:
			virtual size_t size() const = 0;

			virtual bool get_varint(int index, Variant& value) const = 0;
			virtual bool get_bool(int index, bool& value) const = 0;
			virtual bool get_long(int index, long& value) const = 0;
			virtual bool get_double(int index, double& value) const = 0;
			virtual bool get_string(int index, std::string& value) const = 0;
			virtual bool get_vector3_int16(int index, G3D::Vector3int16& value) const = 0;
			virtual bool get_region3_int16(int index, void* value) const = 0;
			virtual bool get_vector3(int index, G3D::Vector3& value) const = 0;
			virtual bool get_region3(int index, void* value) const = 0;
			virtual bool get_rect(int index, G3D::Rect2D& value) const = 0;
			virtual bool get_object(int index, std::shared_ptr<DescribedBase>& value) const = 0;
			virtual bool get_enum(int index, const EnumDescriptor& desc, int& value) const = 0;

			// Generic type
			virtual void* get(int index) const = 0;
		};

		enum Kind : std::uint32_t
		{
			Default = 0,
			Custom = 1,
		};

		virtual int execute_custom(DescribedBase* instance, lua_State* L) const = 0;
		virtual int execute_lua(DescribedBase* instance, lua_State* L, int argument_base) const = 0;

		[[nodiscard]] const SignatureDescriptor& get_signature() const noexcept
		{
			return signature;
		}

		[[nodiscard]] Kind get_kind() const noexcept
		{
			return kind;
		}

		bool is_no_mutation;
		SignatureDescriptor signature;
		Kind kind;

	private:
		RML_LAYOUT_GUARD_BEGIN()
#if defined(RML_WINDOWS)
		RML_ASSERT_SIZE(FunctionDescriptor, 0x90);
		RML_ASSERT_OFFSET(Attributes, is_no_mutation, 0x10);
		RML_ASSERT_OFFSET(FunctionDescriptor, is_no_mutation, 0x50);
		RML_ASSERT_OFFSET(FunctionDescriptor, signature, 0x58);
		RML_ASSERT_OFFSET(FunctionDescriptor, kind, 0x88);
#else
		RML_ASSERT_SIZE(FunctionDescriptor, 0x88);
		RML_ASSERT_OFFSET(Attributes, is_no_mutation, 0xC);
		RML_ASSERT_OFFSET(FunctionDescriptor, is_no_mutation, 0x4C);
		RML_ASSERT_OFFSET(FunctionDescriptor, signature, 0x50);
		RML_ASSERT_OFFSET(FunctionDescriptor, kind, 0x80);
#endif
		RML_LAYOUT_GUARD_END()
	};

	class BoundFunctionDescriptor : public FunctionDescriptor
	{
	public:
		template<typename T>
		[[nodiscard]] T* native_func_ptr() const noexcept
		{
			return reinterpret_cast<T*>(invoke_func_ptr);
		}

		void* invoke_func_ptr;
		std::intptr_t bound_this_delta;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_SIZE(BoundFunctionDescriptor, 0xA0);
	RML_ASSERT_OFFSET(BoundFunctionDescriptor, invoke_func_ptr, 0x90);
	RML_ASSERT_OFFSET(BoundFunctionDescriptor, bound_this_delta, 0x98);
#else
	RML_ASSERT_SIZE(BoundFunctionDescriptor, 0x98);
	RML_ASSERT_OFFSET(BoundFunctionDescriptor, invoke_func_ptr, 0x88);
	RML_ASSERT_OFFSET(BoundFunctionDescriptor, bound_this_delta, 0x90);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()

	class Function
	{
	protected:
		const FunctionDescriptor* m_descriptor;
		DescribedBase* m_instance;

		template<std::size_t>
		using arg_slot = void*;

		class EngineCallable
		{
		};

		template<typename MemberPointer>
		[[nodiscard]] MemberPointer load_member_pointer() const noexcept
		{
			MemberPointer member{};
			std::memcpy(&member, &static_cast<const BoundFunctionDescriptor*>(m_descriptor)->invoke_func_ptr, sizeof(member));
			return member;
		}

	public:
		static constexpr std::size_t engine_return_bytes = 64;

		struct ReturnStorage
		{
			alignas(16) std::byte bytes[engine_return_bytes];
		};

	protected:
		template<std::size_t... I>
		std::uint64_t invoke_fixed(FunctionDescriptor::Arguments& arguments, const bool indirect_result, ReturnStorage& result, std::index_sequence<I...>) const
		{
			auto* const self = reinterpret_cast<EngineCallable*>(m_instance);

			// cursed ABI voodoo, don't ask, just trust me :)

			if (!indirect_result)
			{
				using DirectMember = std::uint64_t (EngineCallable::*)(arg_slot<I>...);
				return (self->*load_member_pointer<DirectMember>())(arguments.get(static_cast<int>(I) + 1)...);
			}

			using Slot = rml::memory::detail::IndirectResult<engine_return_bytes>;
			using IndirectMember = Slot (EngineCallable::*)(arg_slot<I>...);

			const Slot value = (self->*load_member_pointer<IndirectMember>())(arguments.get(static_cast<int>(I) + 1)...);

			std::memcpy(result.bytes, value.m_storage, engine_return_bytes);
			std::uint64_t first{};
			std::memcpy(&first, result.bytes, sizeof(first));
			return first;
		}

	public:
		Function(const FunctionDescriptor& descriptor, DescribedBase* instance) :
		    m_descriptor(&descriptor),
		    m_instance(instance)
		{
		}

		Function(const Function& other) = default;

		Function& operator=(const Function& other) = default;

		[[nodiscard]] const Name& name() const
		{
			return m_descriptor->name;
		}

		[[nodiscard]] const FunctionDescriptor* descriptor() const
		{
			return m_descriptor;
		}

		uint64_t invoke(FunctionDescriptor::Arguments& arguments, const bool indirect_result, ReturnStorage& result) const
		{
			switch (arguments.size())
			{
			case 0: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<0>{});
			case 1: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<1>{});
			case 2: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<2>{});
			case 3: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<3>{});
			case 4: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<4>{});
			case 5: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<5>{});
			case 6: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<6>{});
			case 7: return invoke_fixed(arguments, indirect_result, result, std::make_index_sequence<7>{});
			default: throw std::runtime_error("Too many arguments");
			}
		}
	};
}