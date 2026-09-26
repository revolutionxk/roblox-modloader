#pragma once
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/function_descriptor.hpp"
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "RobloxModLoader/roblox/util/G3DCore.h"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "dotnet_variant.hpp"
#include "interop_registry.hpp"
#include "type_marshaler.hpp"

#include <array>
#include <cstddef>
#include <deque>
#include <memory>
#include <new>
#include <string>
#include <utility>

namespace rml::dotnet
{
	inline constexpr std::size_t kEngineReturnSlotTailBytes = 56;

	static_assert(sizeof(uint64_t) + kEngineReturnSlotTailBytes >= TypeMarshaler::kMaxBlittableEngineTypeBytes,
	    "EngineReturnSlot tail must leave enough contiguous room after Arguments::return_value for the largest blittable engine return type");

	using EngineReturnSlot = std::array<std::byte, kEngineReturnSlotTailBytes>;

	template<class T>
	class ByValueArguments
	{
	public:
		ByValueArguments() = default;
		ByValueArguments(const ByValueArguments&) = delete;
		ByValueArguments& operator=(const ByValueArguments&) = delete;

		~ByValueArguments()
		{
			if (m_handed_over)
				return;
			for (auto& slot : m_slots)
				std::destroy_at(slot.value());
		}

		template<class... Args>
		T& emplace(Args&&... args)
		{
			auto& slot = m_slots.emplace_back();
			return *std::construct_at(reinterpret_cast<T*>(slot.storage), std::forward<Args>(args)...);
		}

		void hand_over() noexcept
		{
			m_handed_over = true;
		}

	private:
		struct Slot
		{
			alignas(T) std::byte storage[sizeof(T)];

			T* value() noexcept
			{
				return std::launder(reinterpret_cast<T*>(storage));
			}
		};

		std::deque<Slot> m_slots;
		bool m_handed_over{};
	};

	class DotNetArguments final : public RBX::Reflection::FunctionDescriptor::Arguments
	{
		alignas(16) EngineReturnSlot m_return_slot{};
		const InteropVariant* m_args;
		uint32_t m_count;

		const RBX::Reflection::SignatureDescriptor* m_signature{nullptr};
		const RBX::Reflection::Type* m_tuple_type{nullptr};

		mutable ByValueArguments<std::string> m_strings;
		mutable ByValueArguments<std::shared_ptr<RBX::Instance>> m_instances;
		mutable RBX::Reflection::Variant m_tuple_value{};
		mutable bool m_tuple_built{false};

	public:
		DotNetArguments(const InteropVariant* args, const uint32_t count,
		    const RBX::Reflection::SignatureDescriptor* signature = nullptr) noexcept :
		    m_args(args),
		    m_count(count),
		    m_signature(signature)
		{
			return_value = 0;

			if (!m_signature)
				return;

			const auto sig_args = m_signature->arguments();
			if (sig_args.size() == 1 && sig_args[0].type && sig_args[0].type->type_id == RBX::Reflection::TypeId::Tuple)
				m_tuple_type = sig_args[0].type;
		}

		~DotNetArguments()
		{
			if (m_tuple_built)
				std::destroy_at(static_cast<std::shared_ptr<const RBX::Reflection::Tuple>*>(m_tuple_value.storage()));
		}

		DotNetArguments(const DotNetArguments&)            = delete;
		DotNetArguments& operator=(const DotNetArguments&) = delete;

		[[nodiscard]] size_t size() const override
		{
			return m_tuple_type ? 1u : m_count;
		}

		bool get_varint(const int index, RBX::Reflection::Variant& value) const override
		{
			if (!is_valid(index) || !m_signature)
				return false;

			const auto sig_args = m_signature->arguments();
			if (static_cast<size_t>(index - 1) >= sig_args.size())
				return false;

			const auto* type = sig_args[index - 1].type;
			if (!type)
				return false;

			return TypeMarshaler::decode_argument(type, m_args[index - 1], value, borrow_value_ops(type));
		}

		bool get_bool(const int index, bool& value) const override
		{
			return is_valid(index) && read_bool(m_args[index - 1], value);
		}

		bool get_long(const int index, long& value) const override
		{
			int64_t wide;
			if (!is_valid(index) || !read_int64(m_args[index - 1], wide))
				return false;
			value = static_cast<long>(wide);
			return true;
		}

		bool get_double(const int index, double& value) const override
		{
			return is_valid(index) && read_double(m_args[index - 1], value);
		}

		bool get_string(const int index, std::string& value) const override
		{
			if (!is_valid(index))
				return false;
			const auto* str = read_string(m_args[index - 1]);
			if (!str)
				return false;
			value = str;
			return true;
		}

		bool get_vector3(const int index, RBX::Vector3& value) const override
		{
			return read_struct_arg(index, value);
		}

		bool get_vector3_int16(const int index, RBX::Vector3int16& value) const override
		{
			return read_struct_arg(index, value);
		}

		bool get_region3_int16([[maybe_unused]] int index, [[maybe_unused]] void* value) const override
		{
			return false;
		}

		bool get_region3([[maybe_unused]] int index, [[maybe_unused]] void* value) const override
		{
			return false;
		}

		bool get_rect(const int index, RBX::Rect2D& value) const override
		{
			return read_struct_arg(index, value);
		}

		bool get_object(const int index, std::shared_ptr<RBX::Reflection::DescribedBase>& value) const override
		{
			if (!is_valid(index))
				return false;
			auto* ptr = read_instance<RBX::Reflection::DescribedBase>(m_args[index - 1]);
			if (!ptr)
				return false;

			value = ptr->weak_from_this().lock();
			return value != nullptr;
		}

		bool get_enum(const int index, [[maybe_unused]] const RBX::Reflection::EnumDescriptor& desc, int& value) const override
		{
			int64_t wide;
			if (!is_valid(index) || !read_int64(m_args[index - 1], wide))
				return false;
			value = static_cast<int>(wide);
			return true;
		}

		[[nodiscard]] void* get(const int index) const override
		{
			if (m_tuple_type)
			{
				if (index != 1)
					return nullptr;

				if (!m_tuple_built)
					m_tuple_built = TypeMarshaler::build_tuple_variant(m_args, m_count, m_tuple_type, m_tuple_value);

				return m_tuple_built ? m_tuple_value.storage() : nullptr;
			}

			if (!is_valid(index))
				return nullptr;

			const auto& v = m_args[index - 1];

			if (v.tag == InteropValueTag::String)
				return &m_strings.emplace(v.as_string ? v.as_string : "");

			if (v.tag == InteropValueTag::Instance)
				return &m_instances.emplace(owner_of(v.as_instance));

			return reinterpret_cast<void*>(v.as_uint64);
		}

		void hand_over_by_value_arguments() noexcept
		{
			m_strings.hand_over();
			m_instances.hand_over();
		}

	private:
		[[nodiscard]] static std::shared_ptr<RBX::Instance> owner_of(const uintptr_t handle)
		{
			auto* const object = reinterpret_cast<RBX::Reflection::DescribedBase*>(handle);
			return object ? std::static_pointer_cast<RBX::Instance>(object->weak_from_this().lock()) : nullptr;
		}

		[[nodiscard]] bool is_valid(const int index) const noexcept
		{
			return index >= 1 && static_cast<uint32_t>(index) <= m_count;
		}

		[[nodiscard]] const void* borrow_value_ops(const RBX::Reflection::Type* type) const
		{
			if (m_signature)
			{
				for (const auto& arg : m_signature->arguments())
				{
					if (arg.type == type && !arg.default_handle.is_void())
					{
						if (const void* ops = arg.default_handle.value_ops())
							return ops;
					}
				}
			}

			static void (*const destroy_string)(void*) = [](void* storage) {
				static_cast<std::string*>(storage)->~basic_string();
			};
			static void (*const destroy_trivial)(void*) = [](void*) {};
			static const void* string_ops[3] = {nullptr, nullptr, reinterpret_cast<void*>(destroy_string)};
			static const void* trivial_ops[3] = {nullptr, nullptr, reinterpret_cast<void*>(destroy_trivial)};

			return type && type->name == "string" ? string_ops : trivial_ops;
		}

		template<typename T>
		[[nodiscard]] bool read_struct_arg(const int index, T& out) const
		{
			if (!is_valid(index))
				return false;
			const auto* ptr = read_struct_ptr<T>(m_args[index - 1]);
			if (!ptr)
				return false;
			out = *ptr;
			return true;
		}

		RML_LAYOUT_GUARD_BEGIN()
			RML_ASSERT_LAYOUT_OFFSET(DotNetArguments, m_return_slot,
			    offsetof(RBX::Reflection::FunctionDescriptor::Arguments, return_value) +
			        sizeof(RBX::Reflection::FunctionDescriptor::Arguments::return_value));
		RML_LAYOUT_GUARD_END()
	};

} // namespace rml::dotnet