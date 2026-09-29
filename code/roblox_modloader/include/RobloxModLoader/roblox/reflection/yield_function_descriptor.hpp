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

		using ResumeCallback = void (*)(YieldFunctionState* state, const Variant& result);
		using ErrorCallback = void (*)(const std::shared_ptr<YieldFunctionState>& state, std::string message);

		virtual void execute(DescribedBase* instance, FunctionDescriptor::Arguments& arguments, const std::shared_ptr<YieldFunctionState>& state, ResumeCallback resume, ErrorCallback error) const = 0;

		[[nodiscard]] const SignatureDescriptor& get_signature() const
		{
			return signature;
		}

		SignatureDescriptor signature;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(YieldFunctionDescriptor, 0x78);
	RML_ASSERT_OFFSET(YieldFunctionDescriptor, signature, 0x48);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
