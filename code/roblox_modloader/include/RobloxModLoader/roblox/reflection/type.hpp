#pragma once

#include "descriptor.hpp"
#include "member.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>

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

	class Type : public Descriptor
	{
	public:
		const Name& tag;
		const int type_id;
		const bool is_float;
		const bool is_number;
		const bool is_enum;

		Type() = delete;

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
		struct Storage
		{
			std::byte data[0x40]{};
		};

		const Type* m_type{nullptr};
		const void* m_value_ops{nullptr};
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

		[[nodiscard]] const void* value_ops() const noexcept
		{
			return m_value_ops;
		}

		[[nodiscard]] void* storage() noexcept
		{
			return m_storage.data;
		}

		void set_type_and_ops(const Type* type, const void* value_ops) noexcept
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
			const ClassDescriptor* class_descriptor;
			const void* reserved_18;
			const Variant default_handle;

			[[nodiscard]] bool has_default_value() const noexcept
			{
				return !default_handle.is_void();
			}
		};

		struct Result
		{
			const Type* type;
			const std::string* name;
			const ClassDescriptor* class_descriptor;
		};

		template<typename T>
		struct StdVector
		{
			T* m_begin{nullptr};
			T* m_end{nullptr};
			T* m_capacity{nullptr};

			[[nodiscard]] bool empty() const noexcept
			{
				return m_begin == m_end;
			}
			[[nodiscard]] std::size_t size() const noexcept
			{
				return static_cast<std::size_t>(m_end - m_begin);
			}
			[[nodiscard]] T& front() const
			{
				return *m_begin;
			}
			[[nodiscard]] std::span<T> span() const noexcept
			{
				return {m_begin, m_end};
			}
		};

		StdVector<Argument> m_arguments;
		StdVector<Result> m_result_types;

		[[nodiscard]] std::span<const Argument> arguments() const noexcept
		{
			return m_arguments.span();
		}

		[[nodiscard]] std::span<const Result> result_types() const noexcept
		{
			return m_result_types.span();
		}

		[[nodiscard]] const Type* first_result_type() const noexcept
		{
			return m_result_types.empty() ? nullptr : m_result_types.front().type;
		}
	};
	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(SignatureDescriptor::Argument, 0x70);
	RML_ASSERT_SIZE(SignatureDescriptor::Result, 0x18);
	RML_ASSERT_SIZE(SignatureDescriptor::StdVector<void*>, 0x18);
	RML_LAYOUT_DIAGNOSTIC_POP()
	static_assert(sizeof(SignatureDescriptor) == 0x30);
}