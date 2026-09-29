#include "type_marshaler.hpp"

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/object.hpp"
#include "RobloxModLoader/roblox/reflection/property_descriptor.hpp"
#include "RobloxModLoader/roblox/util/BrickColor.h"
#include "RobloxModLoader/roblox/util/G3DCore.h"
#include "RobloxModLoader/roblox/util/color_sequence.hpp"
#include "RobloxModLoader/roblox/util/faces.hpp"
#include "RobloxModLoader/roblox/util/number_range.hpp"
#include "RobloxModLoader/roblox/util/ray.hpp"
#include "RobloxModLoader/roblox/util/region3.hpp"
#include "RobloxModLoader/roblox/util/udim.hpp"
#include "RobloxModLoader/roblox/util/number_sequence.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "RobloxModLoader/util/memory.hpp"
#include "instance_handles.hpp"
#include "pointers.hpp"
#include "roblox/reflection/type_index.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>

RML_LOG_SCOPE("Interop");

namespace rml::dotnet
{
	RML_ASSERT_SIZE(RBX::Vector2, 8);
	RML_ASSERT_SIZE(RBX::Vector3, 12);
	RML_ASSERT_SIZE(RBX::Color3, 12);
	RML_ASSERT_SIZE(RBX::CoordinateFrame, 48);
	RML_ASSERT_SIZE(RBX::Rect2D, 16);
	RML_ASSERT_SIZE(RBX::BrickColor, 4);

	using BlittableSetter = void (*)(RBX::Property& property, const void* bytes);

	template<typename T>
	static void set_blittable(RBX::Property& property, const void* bytes)
	{
		property.set(*static_cast<const T*>(bytes));
	}

	struct BlittableType
	{
		const char* name;
		int type_id;
		size_t size;
		BlittableSetter set;
	};

	static constexpr BlittableType k_blittable_types[] = {
	    {"Vector3", RBX::Reflection::TypeId::Vector3, sizeof(RBX::Vector3), &set_blittable<RBX::Vector3>},
	    {"Vector2", RBX::Reflection::TypeId::Vector2, sizeof(RBX::Vector2), &set_blittable<RBX::Vector2>},
	    {"Color3", RBX::Reflection::TypeId::Color3, sizeof(RBX::Color3), &set_blittable<RBX::Color3>},
	    {"CoordinateFrame", RBX::Reflection::TypeId::CoordinateFrame, sizeof(RBX::CoordinateFrame), &set_blittable<RBX::CoordinateFrame>},
	    {"CFrame", RBX::Reflection::TypeId::CoordinateFrame, sizeof(RBX::CoordinateFrame), &set_blittable<RBX::CoordinateFrame>},
	    {"Rect2D", RBX::Reflection::TypeId::Rect2D, sizeof(RBX::Rect2D), &set_blittable<RBX::Rect2D>},
	    {"BrickColor", RBX::Reflection::TypeId::BrickColor, sizeof(RBX::BrickColor), &set_blittable<RBX::BrickColor>},
	    {"UDim", RBX::Reflection::TypeId::UDim, sizeof(RBX::UDim), &set_blittable<RBX::UDim>},
	    {"UDim2", RBX::Reflection::TypeId::UDim2, sizeof(RBX::UDim2), &set_blittable<RBX::UDim2>},
	    {"Ray", RBX::Reflection::TypeId::Ray, sizeof(RBX::Ray), &set_blittable<RBX::Ray>},
	    {"NumberRange", RBX::Reflection::TypeId::NumberRange, sizeof(RBX::NumberRange), &set_blittable<RBX::NumberRange>},
	    {"Region3", RBX::Reflection::TypeId::Region3, sizeof(RBX::Region3), &set_blittable<RBX::Region3>},
	    {"Faces", RBX::Reflection::TypeId::Faces, sizeof(RBX::Faces), &set_blittable<RBX::Faces>},
	    {"Axes", RBX::Reflection::TypeId::Axes, sizeof(RBX::Axes), &set_blittable<RBX::Axes>},
	};

	static_assert(std::ranges::all_of(k_blittable_types, [](const BlittableType& type) { return type.size <= TypeMarshaler::kMaxBlittableEngineTypeBytes; }));

	[[nodiscard]] static const BlittableType* find_blittable(const RBX::Reflection::Type& type) noexcept
	{
		for (const auto& blittable : k_blittable_types)
		{
			if (blittable.type_id == type.type_id)
				return &blittable;
		}
		for (const auto& blittable : k_blittable_types)
		{
			if (type.name == blittable.name)
				return &blittable;
		}
		return nullptr;
	}

	[[nodiscard]] static MarshalKind sequence_kind(const RBX::Name& type_name) noexcept
	{
		if (type_name == "NumberSequence")
			return MarshalKind::NumberSequence;
		if (type_name == "ColorSequence")
			return MarshalKind::ColorSequence;
		return MarshalKind::Unsupported;
	}

	template<typename Sequence>
	[[nodiscard]] static InteropVariant pack_sequence(const Sequence& sequence)
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Blittable;
		out.as_instance = 0;

		const auto keypoints = sequence.keypoints.items();
		if (auto* buffer = static_cast<std::byte*>(std::malloc(sizeof(int32_t) + keypoints.size_bytes())))
		{
			*reinterpret_cast<int32_t*>(buffer) = static_cast<int32_t>(keypoints.size());
			std::memcpy(buffer + sizeof(int32_t), keypoints.data(), keypoints.size_bytes());
			out.as_instance = reinterpret_cast<uintptr_t>(buffer);
		}
		return out;
	}

	template<typename Sequence>
	static void set_sequence(RBX::Property property, const std::byte* buffer)
	{
		using Keypoint = typename Sequence::Keypoint;
		const auto count = static_cast<size_t>(std::max(*reinterpret_cast<const int32_t*>(buffer), 0));
		const std::span keypoints(reinterpret_cast<const Keypoint*>(buffer + sizeof(int32_t)), count);
		const Sequence sequence{decltype(Sequence::keypoints)(keypoints)};
		property.set(sequence);
	}

	[[nodiscard]] static InteropVariant owned_instance_value(std::shared_ptr<RBX::Reflection::DescribedBase> object)
	{
		const auto handle = instance_handles().retain(std::move(object));
		return handle ? instance_value(handle) : null_value();
	}

	[[nodiscard]] static InteropVariant owned_instance_value(RBX::Reflection::DescribedBase* object)
	{
		const auto handle = instance_handles().retain(object);
		return handle ? instance_value(handle) : null_value();
	}

	[[nodiscard]] static InteropVariant marshal_tuple(const RBX::Reflection::Tuple* tuple, const InstanceOwnership ownership)
	{
		if (!tuple || !utils::memory::is_valid_pointer(reinterpret_cast<uintptr_t>(tuple)) || tuple->values.empty())
			return null_value();

		InteropStringPool strings;
		std::vector<InteropVariant> values;
		values.reserve(tuple->values.size());
		for (const auto& value : tuple->values)
			values.push_back(TypeMarshaler::encode_variant(value, &strings, ownership));

		return tuple_value(values);
	}

	using TupleSharedPtr = std::shared_ptr<const RBX::Reflection::Tuple>;

	static void copy_trivial_storage(const char* source, char* storage)
	{
		std::memcpy(storage, source, RBX::Reflection::Variant::storage_size);
	}

	static void move_trivial_storage(char* source, char* storage)
	{
		std::memcpy(storage, source, RBX::Reflection::Variant::storage_size);
	}

	static void destroy_trivial_storage(char*)
	{
	}

	const RBX::Reflection::detail::holder* TypeMarshaler::trivially_copied_holder() noexcept
	{
		static constexpr RBX::Reflection::detail::holder holder{&copy_trivial_storage, &move_trivial_storage, &destroy_trivial_storage};
		return &holder;
	}

	[[nodiscard]] static int tag_to_type_id(const InteropValueTag tag) noexcept
	{
		switch (tag)
		{
		case InteropValueTag::Bool: return RBX::Reflection::TypeId::Bool;
		case InteropValueTag::Int64: return RBX::Reflection::TypeId::Int64;
		case InteropValueTag::Float: return RBX::Reflection::TypeId::Float;
		case InteropValueTag::Double: return RBX::Reflection::TypeId::Double;
		case InteropValueTag::String: return RBX::Reflection::TypeId::String;
		case InteropValueTag::Instance: return RBX::Reflection::TypeId::Instance;
		default: return -1;
		}
	}

	static void destroy_tuple_contents(RBX::Reflection::Tuple* tuple)
	{
		for (auto& value : tuple->values)
		{
			if (const auto* ops = value.value_ops(); ops && ops->destruct_func)
				ops->destruct_func(static_cast<char*>(value.storage()));
		}
		delete tuple;
	}

	bool TypeMarshaler::build_tuple_variant(const InteropVariant* args, const uint32_t count, const RBX::Reflection::Type* tuple_type, RBX::Reflection::Variant& out)
	{
		if (!tuple_type)
			return false;

		std::shared_ptr<RBX::Reflection::Tuple> tuple(new RBX::Reflection::Tuple(), &destroy_tuple_contents);
		tuple->values.reserve(count);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto* type = reflection::TypeIndex::find_by_id(tag_to_type_id(args[i].tag));
			if (!type)
				continue;

			RBX::Reflection::Variant inner;
			const auto* ops = args[i].tag == InteropValueTag::String ? RBX::Reflection::detail::typed_holder<std::string>::singleton() : trivially_copied_holder();
			if (decode_argument(type, args[i], inner, ops))
				tuple->values.push_back(std::move(inner));
		}

		out.set_type_and_ops(tuple_type, RBX::Reflection::detail::typed_holder<TupleSharedPtr>::singleton());
		::new (out.storage()) TupleSharedPtr(std::move(tuple));
		return true;
	}

	MarshalPlan TypeMarshaler::classify(const RBX::Reflection::Type& type) noexcept
	{
		if (RBX::Reflection::RefPropertyDescriptor::is_ref_property_descriptor(type))
			return {MarshalKind::RefInstance, 0};

		switch (type.type_id)
		{
		case RBX::Reflection::TypeId::Bool: return {MarshalKind::Bool, 0};
		case RBX::Reflection::TypeId::Int:
		case RBX::Reflection::TypeId::Int64:
		case RBX::Reflection::TypeId::Integer: return {MarshalKind::Number, 0};
		case RBX::Reflection::TypeId::Float: return {MarshalKind::Float, 0};
		case RBX::Reflection::TypeId::Double: return {MarshalKind::Double, 0};
		case RBX::Reflection::TypeId::String: return {MarshalKind::String, 0};
		case RBX::Reflection::TypeId::Instance: return {MarshalKind::Instance, 0};
		case RBX::Reflection::TypeId::Instances: return {MarshalKind::InstanceArray, 0};
		case RBX::Reflection::TypeId::Tuple: return {MarshalKind::Tuple, 0};
		case RBX::Reflection::TypeId::NumberSequence: return {MarshalKind::NumberSequence, sizeof(RBX::NumberSequence)};
		case RBX::Reflection::TypeId::ColorSequence: return {MarshalKind::ColorSequence, sizeof(RBX::ColorSequence)};
		default: break;
		}

		if (const auto* blittable = find_blittable(type))
			return {MarshalKind::Blittable, blittable->size};

		if (const auto kind = sequence_kind(type.name); kind == MarshalKind::NumberSequence)
			return {kind, sizeof(RBX::NumberSequence)};
		else if (kind == MarshalKind::ColorSequence)
			return {kind, sizeof(RBX::ColorSequence)};

		if (type.is_enum)
			return {MarshalKind::Enum, 0};

		if (type.is_float)
			return {MarshalKind::Double, 0};

		if (type.is_number)
			return {MarshalKind::Number, 0};

		return {MarshalKind::Unsupported, 0};
	}

	InteropVariant TypeMarshaler::encode_variant(const RBX::Reflection::Variant& variant, InteropStringPool* strings, const InstanceOwnership ownership)
	{
		if (variant.is_void())
			return null_value();

		const auto& type = variant.type();
		const auto [kind, byte_size] = classify(type);

		switch (kind)
		{
		case MarshalKind::RefInstance:
		{
			const auto* shared = variant.try_cast<std::shared_ptr<RBX::Instance>>();
			const auto instance = shared ? reinterpret_cast<uintptr_t>(shared->get()) : 0;
			if (!utils::memory::is_valid_pointer(instance))
			{
				RML_WARN("Dropping implausible instance handle {:#x} for type '{}'", instance, type.name.c_str());
				return null_value();
			}
			return ownership == InstanceOwnership::Transferred ? owned_instance_value(*shared) : instance_value(instance);
		}
		case MarshalKind::Instance:
		{
			const auto* instance = variant.try_cast<RBX::Instance*>();
			if (!instance)
				return null_value();
			return ownership == InstanceOwnership::Transferred ? owned_instance_value(*instance) : instance_value(reinterpret_cast<uintptr_t>(*instance));
		}
		case MarshalKind::String:
			return strings ? string_value(variant.try_cast<std::string>()->c_str(), *strings) :
			                 string_value(variant.try_cast<std::string>()->c_str());
		case MarshalKind::Bool: return bool_value(*variant.try_cast<bool>());
		case MarshalKind::Enum: return int64_value(*variant.try_cast<int>());
		case MarshalKind::Float: return float_value(*variant.try_cast<float>());
		case MarshalKind::Double: return double_value(*variant.try_cast<double>());
		case MarshalKind::Number:
			return int64_value(type.type_id == RBX::Reflection::TypeId::Int ? *variant.try_cast<int>() : *variant.try_cast<int64_t>());
		case MarshalKind::Tuple:
		{
			const auto* shared = variant.try_cast<TupleSharedPtr>();
			return marshal_tuple(shared ? shared->get() : nullptr, ownership);
		}
		default: RML_WARN("Unsupported variant type '{}'", type.name.c_str()); return null_value();
		}
	}

	InteropVariant TypeMarshaler::encode_property(const RBX::Reflection::PropertyDescriptor* descriptor, const RBX::Reflection::DescribedBase* instance)
	{
		const auto& type = descriptor->type;
		const auto [kind, byte_size] = classify(type);

		if (kind == MarshalKind::String)
			return string_value(descriptor->get_string_value(instance).c_str());

		if (kind == MarshalKind::RefInstance)
		{
			const auto* ref_descriptor = dynamic_cast<const RBX::Reflection::RefPropertyDescriptor*>(descriptor);
			return owned_instance_value(ref_descriptor->get_ref_value(instance));
		}

		if (kind == MarshalKind::Unsupported)
		{
			RML_WARN("Unsupported property type '{}' for get_property('{}')", type.name.c_str(), descriptor->name.c_str());
			return null_value();
		}

		RBX::Reflection::Variant variant;
		descriptor->get_variant(instance, variant);
		if (variant.is_void())
			return null_value();

		if (kind == MarshalKind::NumberSequence)
			return pack_sequence(*variant.try_cast<RBX::NumberSequence>());

		if (kind == MarshalKind::ColorSequence)
			return pack_sequence(*variant.try_cast<RBX::ColorSequence>());

		if (kind == MarshalKind::Blittable)
			return blittable_value(variant.try_cast<std::byte>(), byte_size);

		return encode_variant(variant, nullptr, InstanceOwnership::Transferred);
	}

	bool TypeMarshaler::decode_property(const RBX::Reflection::PropertyDescriptor* descriptor, RBX::Reflection::DescribedBase* instance, const InteropVariant& value)
	{
		const auto& type = descriptor->type;
		const auto plan = classify(type);

		if (plan.kind == MarshalKind::String)
		{
			if (value.tag != InteropValueTag::String || !value.as_string)
				return false;
			return descriptor->set_string_value(instance, value.as_string);
		}

		if (plan.kind == MarshalKind::RefInstance)
		{
			const auto* ref_descriptor = dynamic_cast<const RBX::Reflection::RefPropertyDescriptor*>(descriptor);
			auto* target =
			    value.tag == InteropValueTag::Instance ? reinterpret_cast<RBX::Reflection::DescribedBase*>(value.as_instance) : nullptr;
			ref_descriptor->set_ref_value(instance, target);
			return true;
		}

		if (plan.kind == MarshalKind::NumberSequence || plan.kind == MarshalKind::ColorSequence)
		{
			if (value.tag != InteropValueTag::Blittable || value.as_instance == 0)
				return false;

			const auto* buffer = reinterpret_cast<const std::byte*>(value.as_instance);
			RBX::Property property(*descriptor, instance);
			if (plan.kind == MarshalKind::NumberSequence)
				set_sequence<RBX::NumberSequence>(property, buffer);
			else
				set_sequence<RBX::ColorSequence>(property, buffer);
			return true;
		}

		if (plan.kind == MarshalKind::Blittable)
		{
			if (value.tag != InteropValueTag::Blittable || value.as_instance == 0)
				return false;

			const auto* bytes = reinterpret_cast<const void*>(value.as_instance);
			RBX::Property property(*descriptor, instance);

			const auto* blittable = find_blittable(type);
			if (!blittable)
				return false;
			blittable->set(property, bytes);
			return true;
		}

		if (plan.kind == MarshalKind::Bool)
		{
			bool decoded = false;
			if (!read_bool(value, decoded))
				return false;
			RBX::Property(*descriptor, instance).set<bool>(decoded);
			return true;
		}

		if (plan.kind == MarshalKind::Float)
		{
			double decoded = 0.0;
			if (!read_double(value, decoded))
				return false;
			RBX::Property(*descriptor, instance).set<float>(static_cast<float>(decoded));
			return true;
		}

		if (plan.kind == MarshalKind::Double)
		{
			double decoded = 0.0;
			if (!read_double(value, decoded))
				return false;
			RBX::Property(*descriptor, instance).set<double>(decoded);
			return true;
		}

		if (plan.kind == MarshalKind::Enum || plan.kind == MarshalKind::Number)
		{
			int64_t decoded = 0;
			if (!read_int64(value, decoded))
				return false;

			RBX::Property property(*descriptor, instance);
			if (type.type_id == RBX::Reflection::TypeId::Int64 || type.type_id == RBX::Reflection::TypeId::Integer)
				property.set<int64_t>(decoded);
			else
				property.set<int>(static_cast<int>(decoded));
			return true;
		}

		RML_WARN("Unsupported property type '{}' for set_property('{}')", type.name.c_str(), descriptor->name.c_str());
		return false;
	}

	bool TypeMarshaler::decode_argument(const RBX::Reflection::Type* type, const InteropVariant& value, RBX::Reflection::Variant& out, const RBX::Reflection::detail::holder* value_ops)
	{
		if (!type)
			return false;

		out.set_type_and_ops(type, value_ops);
		void* const storage = out.storage();
		const auto plan = classify(*type);

		switch (plan.kind)
		{
		case MarshalKind::String:
			::new (storage) std::string(value.tag == InteropValueTag::String && value.as_string ? value.as_string : "");
			return true;

		case MarshalKind::Bool:
		{
			bool decoded = false;
			(void)read_bool(value, decoded);
			*static_cast<bool*>(storage) = decoded;
			return true;
		}

		case MarshalKind::Float:
		case MarshalKind::Double:
		{
			double decoded = 0.0;
			(void)read_double(value, decoded);
			if (plan.kind == MarshalKind::Float)
				*static_cast<float*>(storage) = static_cast<float>(decoded);
			else
				*static_cast<double*>(storage) = decoded;
			return true;
		}

		case MarshalKind::Enum:
		case MarshalKind::Number:
		{
			int64_t decoded = 0;
			(void)read_int64(value, decoded);
			if (type->type_id == RBX::Reflection::TypeId::Int64 || type->type_id == RBX::Reflection::TypeId::Integer)
				*static_cast<int64_t*>(storage) = decoded;
			else
				*static_cast<int*>(storage) = static_cast<int>(decoded);
			return true;
		}

		default: return false;
		}
	}

	bool TypeMarshaler::returns_indirectly(const RBX::Reflection::Type* type) noexcept
	{
		if (!type)
			return false;

		switch (classify(*type).kind)
		{
		case MarshalKind::Tuple:
		case MarshalKind::InstanceArray:
		case MarshalKind::RefInstance:
		case MarshalKind::Instance:
		case MarshalKind::String:
		case MarshalKind::Blittable: return true;
		default: return false;
		}
	}

	void TypeMarshaler::encode_return_value(const RBX::Reflection::Type* type, const uint64_t raw_return, void* const return_storage, InteropVariant& out) noexcept
	{
		if (!type)
		{
			out = null_value();
			return;
		}

		const auto plan = classify(*type);

		switch (plan.kind)
		{
		case MarshalKind::Tuple:
		{
			auto* slot = static_cast<std::shared_ptr<const RBX::Reflection::Tuple>*>(return_storage);
			out = marshal_tuple(slot ? slot->get() : nullptr, InstanceOwnership::Transferred);
			std::destroy_at(slot);
			return;
		}
		case MarshalKind::InstanceArray:
		{
			out.tag = InteropValueTag::InstanceArray;
			out.as_instance = 0;

			auto* slot = static_cast<std::shared_ptr<RBX::Instances>*>(return_storage);
			if (const auto& instances_ptr = *slot; instances_ptr && !instances_ptr->empty())
			{
				const auto& instances = *instances_ptr;
				const auto count = static_cast<uint32_t>(instances.size());
				const auto buffer_size = sizeof(uint64_t) + static_cast<size_t>(count) * sizeof(uintptr_t);

				if (auto* buffer = static_cast<uint8_t*>(std::malloc(buffer_size)))
				{
					auto* count_field = reinterpret_cast<uint32_t*>(buffer);
					auto* handles = reinterpret_cast<uintptr_t*>(buffer + sizeof(uint64_t));
					uint32_t written = 0;

					for (const auto& element : instances)
					{
						if (const auto handle = instance_handles().retain(element))
							handles[written++] = handle;
					}
					*count_field = written;
					out.as_instance = reinterpret_cast<uintptr_t>(buffer);
				}
			}

			std::destroy_at(slot);
			return;
		}
		case MarshalKind::RefInstance:
		case MarshalKind::Instance:
		{
			auto* slot = static_cast<std::shared_ptr<RBX::Instance>*>(return_storage);
			out = owned_instance_value(*slot);
			std::destroy_at(slot);
			return;
		}
		case MarshalKind::String:
		{
			auto* slot = static_cast<std::string*>(return_storage);
			out = string_value(slot->c_str());
			std::destroy_at(slot);
			return;
		}
		case MarshalKind::Blittable:
			out = blittable_value(return_storage, plan.byte_size);
			return;
		case MarshalKind::Bool:
			out = bool_value(static_cast<std::uint8_t>(raw_return) != 0);
			return;
		case MarshalKind::Number:
			out = int64_value(type->type_id == RBX::Reflection::TypeId::Int ? static_cast<std::int32_t>(raw_return) : static_cast<std::int64_t>(raw_return));
			return;
		default: break;
		}

		if (!raw_return)
		{
			out = null_value();
			return;
		}

		out = int64_value(static_cast<int64_t>(raw_return));
	}
} // namespace rml::dotnet
