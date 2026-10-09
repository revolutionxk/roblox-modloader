#pragma once

#include "descriptor.hpp"
#include "member.hpp"
#include "RobloxModLoader/rml_export.hpp"
#include "variant_holder.hpp"

#include <cstddef>
#include <span>
#include <vector>
#include <string>
#include <unordered_map>

namespace G3D
{
	class Color3;
	class Vector3;
}

namespace RBX::Reflection
{
	namespace TypeId
	{
		constexpr int Bool = 1, Int = 2, Int64 = 3, Float = 4, Double = 5, String = 6;
		constexpr int Instance = 8, Instances = 9, Ray = 10, Vector2 = 11, Vector3 = 12;
		constexpr int Rect2D = 15, CoordinateFrame = 16, Color3 = 17, UDim = 19, UDim2 = 20;
		constexpr int Faces = 21, Axes = 22, Region3 = 23, BrickColor = 28, Tuple = 35;
		constexpr int ColorSequence = 42, NumberRange = 44, NumberSequence = 45, Integer = 87;
	}

	class VariantData;

	class Type : public Descriptor
	{
	public:
		const Name& tag;
		const int type_id;
		const bool is_float;
		const bool is_number;
		const bool is_enum;

	private:
		[[maybe_unused]] const bool reserved_37;
		[[maybe_unused]] const bool reserved_38;

	public:
		Type() = delete;

		virtual std::string to_string(const VariantData* data) const = 0;

		RML_EXPORT static std::span<const Type* const> get_all_types();

		template<class T>
		static const Type& get_singleton();

		template<class T>
		static const Type& singleton()
		{
			return get_singleton<T>();
		}

		template<class T>
		static const Type* try_singleton() noexcept
		{
			try
			{
				return &get_singleton<T>();
			}
			catch (...)
			{
				return nullptr;
			}
		}

		template<class T>
		bool is_type() const
		{
			return this == try_singleton<T>();
		}

		bool operator==(const Type& other) const noexcept
		{
			return this == &other;
		}
		bool operator!=(const Type& other) const noexcept
		{
			return this != &other;
		}
	};

	template<>
	RML_EXPORT const Type& Type::get_singleton<void>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<bool>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<int>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<float>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<double>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<std::string>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<G3D::Color3>();
	template<>
	RML_EXPORT const Type& Type::get_singleton<G3D::Vector3>();

	template<typename T>
	class TType : public Type
	{
		friend class Type;

	protected:
		explicit TType(const char* name) :
		    Type(name, const_cast<T*>(nullptr))
		{
		}
		TType(const char* name, const char* tag) :
		    Type(name, tag, const_cast<T*>(nullptr))
		{
		}
	};

	class Variant
	{
	public:
		static constexpr std::size_t storage_size = 0x40;

	private:
		struct Storage
		{
			std::byte data[storage_size]{};
		};

		const Type* m_type{nullptr};
		const detail::holder* m_value_ops{nullptr};
		alignas(8) Storage m_storage;

	public:
		Variant() = default;
		Variant(const Variant&) = default;
		Variant& operator=(const Variant&) = default;

		[[nodiscard]] const Type& type() const
		{
			return *m_type;
		}

		bool is_float() const
		{
			return m_type->is_float;
		}

		bool is_number() const
		{
			return m_type->is_number;
		}

		bool is_enum() const
		{
			return m_type->is_enum;
		}

		[[nodiscard]] bool is_void() const noexcept
		{
			return m_type == nullptr;
		}

		template<typename T>
		T* try_cast()
		{
			return reinterpret_cast<T*>(m_storage.data);
		}

		template<typename T>
		const T* try_cast() const
		{
			return reinterpret_cast<const T*>(m_storage.data);
		}

		[[nodiscard]] const detail::holder* value_ops() const noexcept
		{
			return m_value_ops;
		}

		[[nodiscard]] void* storage() noexcept
		{
			return m_storage.data;
		}

		void set_type_and_ops(const Type* type, const detail::holder* value_ops) noexcept
		{
			m_type = type;
			m_value_ops = value_ops;
		}
	};

	using EventArguments = std::vector<Variant>;
	using ValueArray = std::vector<Variant>;
	using ValueMap = std::unordered_map<std::string, Variant>;

	struct Tuple
	{
		ValueArray values;

		Tuple() = default;
		explicit Tuple(const std::size_t count) :
		    values(count)
		{
		}

		Variant& at(const std::size_t i)
		{
			return values[i];
		}

		[[nodiscard]] const Variant& at(const std::size_t i) const
		{
			return values[i];
		}
	};

	class SignatureDescriptor
	{
	public:
		struct Argument
		{
			const Name* name;
			const Type* type;
			const Name* alias;
			const ClassDescriptor* class_descriptor;
			const Variant default_handle;

			[[nodiscard]] bool has_default_value() const noexcept
			{
				return !default_handle.is_void();
			}
		};

		struct Result
		{
			const Type* type;
			const Name* alias;
			const ClassDescriptor* class_descriptor;
		};

		std::vector<Argument> arguments;
		std::vector<Result> result_types;

		[[nodiscard]] const Type* first_result_type() const noexcept
		{
			return result_types.empty() ? nullptr : result_types.front().type;
		}
	};
	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(SignatureDescriptor::Argument, class_descriptor, 0x18);
	RML_ASSERT_OFFSET(SignatureDescriptor::Argument, default_handle, 0x20);
	RML_ASSERT_SIZE(SignatureDescriptor::Argument, 0x70);
	RML_ASSERT_SIZE(SignatureDescriptor::Result, 0x18);
	RML_LAYOUT_DIAGNOSTIC_POP()
	static_assert(sizeof(SignatureDescriptor) == 0x30);
}