#pragma once
#include "RobloxModLoader/roblox/reflection/function_descriptor.hpp"
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "RobloxModLoader/roblox/reflection/yield_function_descriptor.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "dotnet_variant.hpp"
#include "interop_registry.hpp"
#include "type_marshaler.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace rml::dotnet
{
	class YieldResult
	{
	public:
		YieldResult() = default;
		YieldResult(const YieldResult&) = delete;
		YieldResult& operator=(const YieldResult&) = delete;

		~YieldResult()
		{
			for (char* owned : m_strings)
				std::free(owned);
		}

		void take_values(const RBX::Reflection::Variant* result)
		{
			if (!result || result->is_void())
				return;

			if (result->type().name == "Tuple")
			{
				const auto* shared = result->try_cast<std::shared_ptr<const RBX::Reflection::Tuple>>();
				if (const RBX::Reflection::Tuple* tuple = shared ? shared->get() : nullptr)
				{
					for (const auto& value : tuple->values)
						m_values.push_back(TypeMarshaler::encode_variant(value, &m_strings, InstanceOwnership::Transferred));
				}
				return;
			}

			m_values.push_back(TypeMarshaler::encode_variant(*result, &m_strings, InstanceOwnership::Transferred));
		}

		void take_error(const RBX::Reflection::Variant* message)
		{
			m_errored = true;
			if (message && !message->is_void() && message->type().name == "string")
				m_error_message = *message->try_cast<std::string>();
		}

		[[nodiscard]] bool errored() const noexcept
		{
			return m_errored;
		}

		[[nodiscard]] const char* error_message() const noexcept
		{
			return m_error_message.c_str();
		}

		[[nodiscard]] InteropVariant to_interop()
		{
			const InteropVariant value = m_values.empty() ? null_value() : (m_values.size() == 1 ? m_values.front() : tuple_value(m_values));
			m_strings.clear();
			return value;
		}

	private:
		std::vector<InteropVariant> m_values;
		InteropStringPool m_strings;
		bool m_errored = false;
		std::string m_error_message;
	};

	class YieldInvocation
	{
	public:
		static void dispatch(const RBX::Reflection::YieldFunctionDescriptor& descriptor, RBX::Reflection::DescribedBase& instance, RBX::Reflection::FunctionDescriptor::Arguments& arguments, const ManagedYieldCallback on_complete, void* state)
		{
			std::unique_ptr<YieldInvocation> invocation(new YieldInvocation(on_complete, state));

			const RBX::Reflection::YieldFunctionDescriptor::Context context{&invocation->m_engine_context, nullptr};
			descriptor.execute(&instance, arguments, context, &on_resume, &on_error);

			invocation.release();
		}

	private:
		static constexpr std::size_t kYieldEngineScratchBytes = 56;

		struct alignas(16) EngineContext
		{
			YieldInvocation* owner{};
			std::array<std::byte, kYieldEngineScratchBytes> reserved{};

		private:
			RML_LAYOUT_GUARD_BEGIN()
				RML_ASSERT_LAYOUT_OFFSET(EngineContext, owner, 0);
				RML_ASSERT_LAYOUT_OFFSET(EngineContext, reserved, sizeof(YieldInvocation*));
			RML_LAYOUT_GUARD_END()
		};

		EngineContext m_engine_context{};
		YieldResult m_result;
		ManagedYieldCallback m_on_complete;
		void* m_state;

		YieldInvocation(ManagedYieldCallback on_complete, void* state) noexcept :
		    m_on_complete(on_complete),
		    m_state(state)
		{
			m_engine_context.owner = this;
		}

		[[nodiscard]] static YieldInvocation* recover_from_engine_context(void* engine_context) noexcept
		{
			return static_cast<EngineContext*>(engine_context)->owner;
		}

		[[nodiscard]] static YieldInvocation* recover_from_error_continuation(void* error_continuation) noexcept
		{
			return recover_from_engine_context(*static_cast<void**>(error_continuation));
		}

		void report_and_destroy() noexcept
		{
			if (m_on_complete)
			{
				try
				{
					if (m_result.errored())
					{
						m_on_complete(m_state, nullptr, m_result.error_message());
					}
					else
					{
						const InteropVariant value = m_result.to_interop();
						m_on_complete(m_state, &value, nullptr);
					}
				}
				catch (...)
				{
				}
			}

			delete this;
		}

		static void on_resume(void* continuation, RBX::Reflection::Variant* result) noexcept
		{
			auto* const self = recover_from_engine_context(continuation);
			try
			{
				self->m_result.take_values(result);
			}
			catch (...)
			{
			}
			self->report_and_destroy();
		}

		static void on_error(void* continuation, RBX::Reflection::Variant* message) noexcept
		{
			auto* const self = recover_from_error_continuation(continuation);
			try
			{
				self->m_result.take_error(message);
			}
			catch (...)
			{
			}
			self->report_and_destroy();
		}
	};
} // namespace rml::dotnet
