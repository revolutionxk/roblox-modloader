#pragma once
#include "RobloxModLoader/roblox/reflection/function_descriptor.hpp"
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "RobloxModLoader/roblox/reflection/yield_function_descriptor.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "dotnet_variant.hpp"
#include "instance_handles.hpp"
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

		void take_error(std::string message)
		{
			m_errored = true;
			m_error_message = std::move(message);
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
			std::unique_ptr<YieldInvocation> invocation(new YieldInvocation(instance_handles().retain(&instance), on_complete, state));

			const std::shared_ptr<RBX::Reflection::YieldFunctionState> yield_state(reinterpret_cast<RBX::Reflection::YieldFunctionState*>(invocation.get()), [](RBX::Reflection::YieldFunctionState*) {});
			descriptor.execute(&instance, arguments, yield_state, &on_resume, &on_error);

			invocation.release();
		}

	private:
		YieldResult m_result;
		std::uintptr_t m_receiver;
		ManagedYieldCallback m_on_complete;
		void* m_state;

		YieldInvocation(const std::uintptr_t receiver, ManagedYieldCallback on_complete, void* state) noexcept :
		    m_receiver(receiver),
		    m_on_complete(on_complete),
		    m_state(state)
		{
		}

	public:
		~YieldInvocation()
		{
			if (m_receiver)
				instance_handles().release(m_receiver);
		}

	private:

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

		static void on_resume(RBX::Reflection::YieldFunctionState* state, const RBX::Reflection::Variant& result) noexcept
		{
			auto* const self = reinterpret_cast<YieldInvocation*>(state);
			try
			{
				self->m_result.take_values(&result);
			}
			catch (...)
			{
			}
			self->report_and_destroy();
		}

		static void on_error(const std::shared_ptr<RBX::Reflection::YieldFunctionState>& state, std::string message) noexcept
		{
			auto* const self = reinterpret_cast<YieldInvocation*>(state.get());
			try
			{
				self->m_result.take_error(std::move(message));
			}
			catch (...)
			{
			}
			self->report_and_destroy();
		}
	};
} // namespace rml::dotnet
