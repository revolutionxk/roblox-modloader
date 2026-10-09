#pragma once

#include "function_descriptor.hpp"
#include "member.hpp"
#include "type.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace RBX::Reflection
{
	class YieldFunction;
	class YieldFunctionState;

	class YieldFunctionDescriptor : public MemberDescriptor
	{
	public:
		typedef YieldFunction ConstMember;
		typedef YieldFunction Member;

		struct Attributes : Descriptor::Attributes
		{
			bool is_no_mutation{false};
		};

		using ResumeCallback = void (*)(YieldFunctionState* state, const Variant& result);
		using ErrorCallback = void (*)(const std::shared_ptr<YieldFunctionState>& state, std::string message);

		virtual void execute(DescribedBase* instance, FunctionDescriptor::Arguments& arguments, const std::shared_ptr<YieldFunctionState>& state, ResumeCallback resume, ErrorCallback error) const = 0;

		[[nodiscard]] const SignatureDescriptor& get_signature() const
		{
			return signature;
		}

		bool is_no_mutation;
		SignatureDescriptor signature;

	private:
		RML_LAYOUT_GUARD_BEGIN()
#if defined(RML_WINDOWS)
		RML_ASSERT_SIZE(YieldFunctionDescriptor, 0x88);
		RML_ASSERT_OFFSET(Attributes, is_no_mutation, 0x10);
		RML_ASSERT_OFFSET(YieldFunctionDescriptor, is_no_mutation, 0x50);
		RML_ASSERT_OFFSET(YieldFunctionDescriptor, signature, 0x58);
#else
		RML_ASSERT_SIZE(YieldFunctionDescriptor, 0x80);
		RML_ASSERT_OFFSET(Attributes, is_no_mutation, 0xC);
		RML_ASSERT_OFFSET(YieldFunctionDescriptor, is_no_mutation, 0x4C);
		RML_ASSERT_OFFSET(YieldFunctionDescriptor, signature, 0x50);
#endif
		RML_LAYOUT_GUARD_END()
	};
}
