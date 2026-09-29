#pragma once

#include "g3d/Color3.h"
#include "g3d/Vector3.h"

#include <cstdint>
#include <new>
#include <type_traits>
#include <string>
#include <utility>

namespace rml::reflection
{
	enum class PropertyType : std::uint8_t
	{
		Bool,
		Int,
		Float,
		Double,
		String,
		Color3,
		Vector3,
		Enum
	};

	template<typename T>
	struct property_type_of;

	template<>
	struct property_type_of<bool>
	{
		static constexpr PropertyType value = PropertyType::Bool;
	};

	template<>
	struct property_type_of<int>
	{
		static constexpr PropertyType value = PropertyType::Int;
	};

	template<>
	struct property_type_of<float>
	{
		static constexpr PropertyType value = PropertyType::Float;
	};

	template<>
	struct property_type_of<double>
	{
		static constexpr PropertyType value = PropertyType::Double;
	};

	template<>
	struct property_type_of<std::string>
	{
		static constexpr PropertyType value = PropertyType::String;
	};

	template<>
	struct property_type_of<G3D::Color3>
	{
		static constexpr PropertyType value = PropertyType::Color3;
	};

	template<>
	struct property_type_of<G3D::Vector3>
	{
		static constexpr PropertyType value = PropertyType::Vector3;
	};

	template<typename T>
	concept ModEnum = std::is_enum_v<T> && !std::is_convertible_v<T, int> && std::is_same_v<std::underlying_type_t<T>, int>;

	template<ModEnum T>
	struct property_type_of<T>
	{
		static constexpr PropertyType value = PropertyType::Enum;
	};

	template<ModEnum E>
	inline constexpr char enum_key_tag{};

	template<ModEnum E>
	[[nodiscard]] constexpr const void* enum_key()
	{
		return &enum_key_tag<E>;
	}

	template<typename T>
	struct abi_value
	{
		using type = T;

		static type from(const T& value)
		{
			return value;
		}
	};

	template<>
	struct abi_value<G3D::Color3>
	{
		struct type
		{
			float r, g, b;
		};

		static type from(const G3D::Color3& value)
		{
			return {value.r, value.g, value.b};
		}
	};

	template<>
	struct abi_value<G3D::Vector3>
	{
		struct type
		{
			float x, y, z;
		};

		static type from(const G3D::Vector3& value)
		{
			return {value.x, value.y, value.z};
		}
	};

	template<typename T>
	using abi_value_t = typename abi_value<T>::type;

	template<typename T>
	class GetSet
	{
	public:
		virtual ~GetSet() = default;
		virtual bool is_read_only() const = 0;
		virtual bool is_write_only() const = 0;
		virtual abi_value_t<T> get_value(const void* instance) const = 0;
		virtual void set_value(void* instance, const T& value) const = 0;
		virtual bool equal_values(const void* a, const void* b) const = 0;
		virtual bool is_value_equal_to(const void* instance, const T& value) const = 0;
	};

	template<typename Class, typename T>
	class MemberGetSet final : public GetSet<T>
	{
	public:
		explicit MemberGetSet(T Class::* member) :
		    m_member(member)
		{
		}

		bool is_read_only() const override
		{
			return false;
		}

		bool is_write_only() const override
		{
			return false;
		}

		abi_value_t<T> get_value(const void* instance) const override
		{
			return abi_value<T>::from(read(instance));
		}

		void set_value(void* instance, const T& value) const override
		{
			static_cast<Class*>(instance)->*m_member = value;
		}

		bool equal_values(const void* a, const void* b) const override
		{
			return read(a) == read(b);
		}

		bool is_value_equal_to(const void* instance, const T& value) const override
		{
			return read(instance) == value;
		}

	private:
		T read(const void* instance) const
		{
			return static_cast<const Class*>(instance)->*m_member;
		}

		T Class::* m_member;
	};

	template<typename Class, typename T, typename Getter, typename Setter>
	class MethodGetSet final : public GetSet<T>
	{
	public:
		MethodGetSet(Getter getter, Setter setter) :
		    m_getter(getter),
		    m_setter(setter)
		{
		}

		bool is_read_only() const override
		{
			return m_setter == nullptr;
		}

		bool is_write_only() const override
		{
			return m_getter == nullptr;
		}

		abi_value_t<T> get_value(const void* instance) const override
		{
			return abi_value<T>::from(read(instance));
		}

		void set_value(void* instance, const T& value) const override
		{
			if (m_setter)
				(static_cast<Class*>(instance)->*m_setter)(value);
		}

		bool equal_values(const void* a, const void* b) const override
		{
			return read(a) == read(b);
		}

		bool is_value_equal_to(const void* instance, const T& value) const override
		{
			return read(instance) == value;
		}

	private:
		T read(const void* instance) const
		{
			return m_getter ? (static_cast<const Class*>(instance)->*m_getter)() : T{};
		}

		Getter m_getter;
		Setter m_setter;
	};

	template<typename Class, typename T>
	class FunctionGetSet final : public GetSet<T>
	{
	public:
		using Getter = T (*)(Class*);
		using Setter = void (*)(Class*, const T&);

		FunctionGetSet(Getter getter, Setter setter) :
		    m_getter(getter),
		    m_setter(setter)
		{
		}

		bool is_read_only() const override
		{
			return m_setter == nullptr;
		}

		bool is_write_only() const override
		{
			return m_getter == nullptr;
		}

		abi_value_t<T> get_value(const void* instance) const override
		{
			return abi_value<T>::from(read(instance));
		}

		void set_value(void* instance, const T& value) const override
		{
			if (m_setter)
				m_setter(static_cast<Class*>(instance), value);
		}

		bool equal_values(const void* a, const void* b) const override
		{
			return read(a) == read(b);
		}

		bool is_value_equal_to(const void* instance, const T& value) const override
		{
			return read(instance) == value;
		}

	private:
		T read(const void* instance) const
		{
			return m_getter ? m_getter(static_cast<Class*>(const_cast<void*>(instance))) : T{};
		}

		Getter m_getter;
		Setter m_setter;
	};
}
