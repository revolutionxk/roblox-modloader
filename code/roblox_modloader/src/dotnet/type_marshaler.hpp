#pragma once
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "dotnet_variant.hpp"
#include "interop_registry.hpp"

#include <cstddef>
#include <cstdint>

namespace RBX::Reflection
{
	class PropertyDescriptor;
	class DescribedBase;
} // namespace RBX::Reflection

namespace rml::dotnet
{
	enum class MarshalKind : uint8_t
	{
		Unsupported,
		String,
		RefInstance,
		Instance,
		InstanceArray,
		Tuple,
		Bool,
		Enum,
		Float,
		Double,
		Number,
		Blittable,
		Sequence,
	};

	struct MarshalPlan
	{
		MarshalKind kind{MarshalKind::Unsupported};
		size_t byte_size{0};
	};

	class TypeMarshaler
	{
	public:
		static constexpr std::size_t kMaxBlittableEngineTypeBytes = 60;

		[[nodiscard]] static MarshalPlan classify(const RBX::Reflection::Type& type) noexcept;

		[[nodiscard]] static bool returns_indirectly(const RBX::Reflection::Type* type) noexcept;

		[[nodiscard]] static InteropVariant encode_variant(const RBX::Reflection::Variant& variant, InteropStringPool* strings = nullptr);
		[[nodiscard]] static InteropVariant encode_property(const RBX::Reflection::PropertyDescriptor* descriptor, const RBX::Reflection::DescribedBase* instance);

		[[nodiscard]] static bool decode_property(const RBX::Reflection::PropertyDescriptor* descriptor, RBX::Reflection::DescribedBase* instance, const InteropVariant& value);
		[[nodiscard]] static bool decode_argument(const RBX::Reflection::Type* type, const InteropVariant& value, RBX::Reflection::Variant& out, const void* value_ops);

		static void encode_return_value(const RBX::Reflection::Type* type, uint64_t raw_return, uintptr_t return_slot_address, InteropVariant& out) noexcept;


		[[nodiscard]] static bool build_tuple_variant(const InteropVariant* args, uint32_t count, const RBX::Reflection::Type* tuple_type, RBX::Reflection::Variant& out);
	};
} // namespace rml::dotnet
