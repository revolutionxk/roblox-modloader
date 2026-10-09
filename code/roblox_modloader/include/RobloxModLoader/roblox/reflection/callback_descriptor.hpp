#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"
#include "member.hpp"
#include "type.hpp"

namespace RBX::Reflection
{
	class Callback;

	class CallbackDescriptor : public MemberDescriptor
	{
	public:
		typedef Callback ConstMember;
		typedef Callback Member;

	public:
		SignatureDescriptor signature;
		bool async_flag;

	public:
		const SignatureDescriptor& get_signature() const
		{
			return signature;
		}

		bool is_async() const
		{
			return async_flag;
		}

	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(CallbackDescriptor, 0x88);
	RML_ASSERT_OFFSET(CallbackDescriptor, signature, 0x50);
	RML_ASSERT_OFFSET(CallbackDescriptor, async_flag, 0x80);
	RML_LAYOUT_DIAGNOSTIC_POP()
}