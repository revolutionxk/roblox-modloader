#include "type_marshaler.hpp"

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/object.hpp"
#include "RobloxModLoader/roblox/reflection/property_descriptor.hpp"
#include "RobloxModLoader/roblox/util/BrickColor.h"
#include "RobloxModLoader/roblox/util/G3DCore.h"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "RobloxModLoader/util/memory.hpp"
#include "pointers.hpp"

#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <new>
#include <utility>
#include <vector>

RML_LOG_SCOPE("Interop");

namespace rml::dotnet
{
	namespace
	{
		RML_ASSERT_LAYOUT_SIZE(RBX::Vector2, 8);
		RML_ASSERT_LAYOUT_SIZE(RBX::Vector3, 12);
		RML_ASSERT_LAYOUT_SIZE(RBX::Color3, 12);
		RML_ASSERT_LAYOUT_SIZE(RBX::CoordinateFrame, 48);
		RML_ASSERT_LAYOUT_SIZE(RBX::Rect2D, 16);
		RML_ASSERT_LAYOUT_SIZE(RBX::BrickColor, 4);

		static_assert(sizeof(float) == 4 && sizeof(int32_t) == 4, "Ray/UDim/UDim2/NumberRange/Region3/Faces/Axes/sequence-stride sizes below are engine-ABI facts (no matching 1:1 reconstructed C++ struct in this codebase; RBX::Ray/RbxRay carries a vtable and is not wire-compatible) and assume 32-bit float and int32 engine fields");

		static_assert(TypeMarshaler::kMaxBlittableEngineTypeBytes >= sizeof(RBX::CoordinateFrame) && TypeMarshaler::kMaxBlittableEngineTypeBytes >= sizeof(RBX::Rect2D) && TypeMarshaler::kMaxBlittableEngineTypeBytes >= sizeof(RBX::Vector3) && TypeMarshaler::kMaxBlittableEngineTypeBytes >= sizeof(RBX::Color3) && TypeMarshaler::kMaxBlittableEngineTypeBytes >= sizeof(RBX::BrickColor), "TypeMarshaler::kMaxBlittableEngineTypeBytes must bound every reconstructed blittable engine type");

		[[nodiscard]] size_t blittable_size(const RBX::Name& type_name) noexcept
		{
			static constexpr std::pair<const char*, size_t> table[] = {
			    {"Vector3", sizeof(RBX::Vector3)},
			    {"Vector2", sizeof(RBX::Vector2)},
			    {"Color3", sizeof(RBX::Color3)},
			    {"CoordinateFrame", sizeof(RBX::CoordinateFrame)},
			    {"CFrame", sizeof(RBX::CoordinateFrame)},
			    {"UDim", 8},
			    {"UDim2", 16},
			    {"Ray", 24},
			    {"Rect2D", sizeof(RBX::Rect2D)},
			    {"NumberRange", 8},
			    {"Region3", TypeMarshaler::kMaxBlittableEngineTypeBytes},
			    {"Faces", 4},
			    {"Axes", 4},
			    {"BrickColor", sizeof(RBX::BrickColor)},
			};

			for (const auto& [name, size] : table)
			{
				if (type_name == name)
					return size;
			}
			return 0;
		}

		[[nodiscard]] size_t sequence_stride(const RBX::Name& type_name) noexcept
		{
			if (type_name == "NumberSequence")
				return 12;
			if (type_name == "ColorSequence")
				return 20;
			return 0;
		}

		[[nodiscard]] InteropVariant marshal_tuple(const RBX::Reflection::Tuple* tuple)
		{
			if (!tuple || !utils::memory::is_valid_pointer(reinterpret_cast<uintptr_t>(tuple)) || tuple->values.empty())
				return null_value();

			InteropStringPool strings;
			std::vector<InteropVariant> values;
			values.reserve(tuple->values.size());
			for (const auto& value : tuple->values)
				values.push_back(TypeMarshaler::encode_variant(value, &strings));

			return tuple_value(values);
		}

		using TupleSharedPtr = std::shared_ptr<const RBX::Reflection::Tuple>;

		void destroy_string_storage(void* storage)
		{
			static_cast<std::string*>(storage)->~basic_string();
		}
		void destroy_trivial_storage(void*)
		{
		}
		void destroy_tuple_storage(void* storage)
		{
			static_cast<TupleSharedPtr*>(storage)->~shared_ptr();
		}
		void* copy_tuple_storage(void* dst, const void* src)
		{
			::new (dst) TupleSharedPtr(*static_cast<const TupleSharedPtr*>(src));
			return dst;
		}

		const void* g_string_ops[3] = {nullptr, nullptr, reinterpret_cast<const void*>(&destroy_string_storage)};
		const void* g_trivial_ops[3] = {nullptr, nullptr, reinterpret_cast<const void*>(&destroy_trivial_storage)};
		const void* g_tuple_ops[3] = {reinterpret_cast<const void*>(&copy_tuple_storage), reinterpret_cast<const void*>(&copy_tuple_storage), reinterpret_cast<const void*>(&destroy_tuple_storage)};

		[[nodiscard]] int tag_to_type_id(const InteropValueTag tag) noexcept
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

		void destroy_tuple_contents(RBX::Reflection::Tuple* tuple)
		{
			for (auto& value : tuple->values)
			{
				if (const auto* const* ops = static_cast<const void* const*>(value.value_ops()); ops && ops[2])
					reinterpret_cast<void (*)(void*)>(const_cast<void*>(ops[2]))(value.storage());
			}
			delete tuple;
		}

		// A yield function that returns an unsupported type is called once per keystroke by an
		// editor mod; the first report is the useful one, the rest are noise. Types are registry
		// singletons, so the address identifies one without allocating on the suppressed path.
		void warn_unsupported_once(const RBX::Reflection::Type& type)
		{
			static std::mutex mutex;
			static std::unordered_set<const RBX::Reflection::Type*> reported;

			{
				const std::scoped_lock lock(mutex);
				if (!reported.insert(&type).second)
					return;
			}

			RML_WARN("Unsupported variant type '{}'", type.name.c_str());
		}
	} // namespace

	const RBX::Reflection::Type* TypeMarshaler::find_type_by_id(const int type_id) noexcept
	{
		if (!g_pointers || !g_pointers->m_roblox_pointers.type_registry)
			return nullptr;

		const auto registry = g_pointers->m_roblox_pointers.type_registry;

		if (!registry || registry->size() > 100000)
			return nullptr;

		for (const auto* type : *registry)
		{
			if (type && type->type_id == type_id)
				return type;
		}
		return nullptr;
	}

	bool TypeMarshaler::build_tuple_variant(const InteropVariant* args, const uint32_t count, const RBX::Reflection::Type* tuple_type, RBX::Reflection::Variant& out)
	{
		if (!tuple_type)
			return false;

		std::shared_ptr<RBX::Reflection::Tuple> tuple(new RBX::Reflection::Tuple(), &destroy_tuple_contents);
		tuple->values.reserve(count);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto* type = find_type_by_id(tag_to_type_id(args[i].tag));
			if (!type)
				continue;

			RBX::Reflection::Variant inner;
			const void* ops = args[i].tag == InteropValueTag::String ? g_string_ops : g_trivial_ops;
			if (decode_argument(type, args[i], inner, ops))
				tuple->values.push_back(std::move(inner));
		}

		out.set_type_and_ops(tuple_type, g_tuple_ops);
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
		case RBX::Reflection::TypeId::Vector3: return {MarshalKind::Blittable, sizeof(RBX::Vector3)};
		case RBX::Reflection::TypeId::Vector2: return {MarshalKind::Blittable, sizeof(RBX::Vector2)};
		case RBX::Reflection::TypeId::Color3: return {MarshalKind::Blittable, sizeof(RBX::Color3)};
		case RBX::Reflection::TypeId::CoordinateFrame: return {MarshalKind::Blittable, sizeof(RBX::CoordinateFrame)};
		case RBX::Reflection::TypeId::Rect2D: return {MarshalKind::Blittable, sizeof(RBX::Rect2D)};
		case RBX::Reflection::TypeId::BrickColor: return {MarshalKind::Blittable, sizeof(RBX::BrickColor)};
		case RBX::Reflection::TypeId::UDim: return {MarshalKind::Blittable, 8};
		case RBX::Reflection::TypeId::UDim2: return {MarshalKind::Blittable, 16};
		case RBX::Reflection::TypeId::Ray: return {MarshalKind::Blittable, 24};
		case RBX::Reflection::TypeId::NumberRange: return {MarshalKind::Blittable, 8};
		case RBX::Reflection::TypeId::Region3: return {MarshalKind::Blittable, kMaxBlittableEngineTypeBytes};
		case RBX::Reflection::TypeId::Faces: return {MarshalKind::Blittable, 4};
		case RBX::Reflection::TypeId::Axes: return {MarshalKind::Blittable, 4};
		case RBX::Reflection::TypeId::NumberSequence: return {MarshalKind::Sequence, 12};
		case RBX::Reflection::TypeId::ColorSequence: return {MarshalKind::Sequence, 20};
		default: break;
		}

		if (const auto size = blittable_size(type.name); size != 0)
			return {MarshalKind::Blittable, size};

		if (const auto stride = sequence_stride(type.name); stride != 0)
			return {MarshalKind::Sequence, stride};

		if (type.is_enum)
			return {MarshalKind::Enum, 0};

		if (type.is_float)
			return {MarshalKind::Double, 0};

		if (type.is_number)
			return {MarshalKind::Number, 0};

		return {MarshalKind::Unsupported, 0};
	}

	InteropVariant TypeMarshaler::encode_variant(const RBX::Reflection::Variant& variant, InteropStringPool* strings)
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
			return instance_value(instance);
		}
		case MarshalKind::Instance:
		{
			const auto* instance = variant.try_cast<RBX::Instance*>();
			return instance ? instance_value(reinterpret_cast<uintptr_t>(*instance)) : null_value();
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
			return marshal_tuple(shared ? shared->get() : nullptr);
		}
		default: warn_unsupported_once(type); return null_value();
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
			return instance_value(reinterpret_cast<uintptr_t>(ref_descriptor->get_ref_value(instance)));
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

		if (kind == MarshalKind::Sequence)
			return pack_sequence(variant.try_cast<std::byte>(), byte_size);

		if (kind == MarshalKind::Blittable)
			return blittable_value(variant.try_cast<std::byte>(), byte_size);

		return encode_variant(variant);
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

		if (plan.kind == MarshalKind::Sequence)
		{
			if (value.tag != InteropValueTag::Blittable || value.as_instance == 0)
				return false;

			const auto* buffer = reinterpret_cast<const std::byte*>(value.as_instance);
			const auto count = *reinterpret_cast<const int32_t*>(buffer);
			const auto* keys = buffer + sizeof(int32_t);

			engine_vector_header header{};
			header.begin = keys;
			header.end = keys + static_cast<size_t>(count < 0 ? 0 : count) * plan.byte_size;
			header.capacity = header.end;

			RBX::Property property(*descriptor, instance);
			property.set(*reinterpret_cast<const std::array<std::byte, sizeof(engine_vector_header)>*>(&header));
			return true;
		}

		if (plan.kind == MarshalKind::Blittable)
		{
			if (value.tag != InteropValueTag::Blittable || value.as_instance == 0)
				return false;

			const auto* bytes = reinterpret_cast<const void*>(value.as_instance);
			RBX::Property property(*descriptor, instance);

			switch (plan.byte_size)
			{
			case 4: property.set(*static_cast<const std::array<std::byte, 4>*>(bytes)); return true;
			case 8: property.set(*static_cast<const std::array<std::byte, 8>*>(bytes)); return true;
			case 12: property.set(*static_cast<const std::array<std::byte, 12>*>(bytes)); return true;
			case 16: property.set(*static_cast<const std::array<std::byte, 16>*>(bytes)); return true;
			case 24: property.set(*static_cast<const std::array<std::byte, 24>*>(bytes)); return true;
			case 48: property.set(*static_cast<const std::array<std::byte, 48>*>(bytes)); return true;
			case 60: property.set(*static_cast<const std::array<std::byte, 60>*>(bytes)); return true;
			default: return false;
			}
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

	bool TypeMarshaler::decode_argument(const RBX::Reflection::Type* type, const InteropVariant& value, RBX::Reflection::Variant& out, const void* value_ops)
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

	void TypeMarshaler::encode_return_value(const RBX::Reflection::Type* type, const uint64_t raw_return, const uintptr_t return_slot_address, InteropVariant& out) noexcept
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
			auto* slot = reinterpret_cast<std::shared_ptr<const RBX::Reflection::Tuple>*>(return_slot_address);
			out = marshal_tuple(slot ? slot->get() : nullptr);
			std::destroy_at(slot);
			return;
		}
		case MarshalKind::InstanceArray:
		{
			out.tag = InteropValueTag::InstanceArray;
			out.as_instance = 0;

			auto* slot = reinterpret_cast<std::shared_ptr<RBX::Instances>*>(return_slot_address);
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
						if (const auto handle = reinterpret_cast<uintptr_t>(element.get()))
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
			auto* slot = reinterpret_cast<std::shared_ptr<RBX::Instance>*>(return_slot_address);
			out = instance_value(reinterpret_cast<uintptr_t>(slot->get()));
			std::destroy_at(slot);
			return;
		}
		case MarshalKind::String:
		{
			auto* slot = reinterpret_cast<std::string*>(return_slot_address);
			out = string_value(slot->c_str());
			std::destroy_at(slot);
			return;
		}
		case MarshalKind::Blittable:
			out = blittable_value(reinterpret_cast<const void*>(return_slot_address), plan.byte_size);
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
