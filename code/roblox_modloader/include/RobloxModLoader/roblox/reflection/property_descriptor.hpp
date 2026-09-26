#pragma once

#include "member.hpp"
#include "type.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace RBX::Reflection
{
	class DescribedBase;
	class Property;

	class Variant;
	class StyledProperties;
	class XmlElement;
	class IReferenceBinder;
	class IReferenceBinderWrite;
	enum class PropertyMetadataType : std::int32_t;

	namespace detail
	{
		struct PropertyMetadataView;
	}

	class PropertyDescriptor : public MemberDescriptor
	{
	public:
		typedef Property ConstMember;
		typedef Property Member;

		enum Functionality : unsigned
		{
			STANDARD = 1 + 2 + 4 + 8 + 16,
			NO_XML_WRITE = 1 + 2 + 4 + 0 + 16,
			UI = 1 + 0 + 4 + 0 + 16,
			SCRIPTING = 1 + 2 + 0 + 0 + 16,
			STREAMING = 0 + 2 + 4 + 8 + 0,
			CLUSTER = 0 + 0 + 4 + 8 + 0,
			LEGACY = 0 + 0 + 4 + 0 + 0,
			REPLICATE_ONLY = 0 + 2 + 0 + 0 + 0,
			LEGACY_SCRIPTING = 0 + 0 + 4 + 0 + 16,
			HIDDEN_SCRIPTING = 0 + 0 + 0 + 0 + 16,
			PUBLIC_SERIALIZED = 1 + 0 + 4 + 8 + 0,
			REPLICATE_CLONE = 0 + 2 + 0 + 0 + 0 + 32,
			STANDARD_NO_REPLICATE = 1 + 0 + 4 + 8 + 16,
			STANDARD_NO_SCRIPTING = 1 + 2 + 4 + 8 + 0,
			PUBLIC_REPLICATE = 1 + 2 + 0 + 0 + 0,
		};

		struct Attributes
		{
			Descriptor::Attributes descriptor;
			std::uint64_t reserved_10;
			std::uint32_t reserved_18;
			std::uint8_t reserved_1c;
			std::uint8_t functionality;
			std::uint8_t reserved_1e;
			std::uint8_t mutability;
		};

		std::byte reserved_48[32];
		const Type& type;

		PropertyDescriptor() = delete;
		std::uint64_t reserved_70;
		Security::Permissions protection_set;
		std::uint32_t reserved_7c;
		std::uint32_t index;
		std::uint16_t reserved_84;
		std::uint16_t reserved_86;
		std::uint8_t reserved_88;
		std::uint8_t reserved_89;
		bool is_enum;
		unsigned is_public : 1;
		unsigned is_editable : 1;
		unsigned can_replicate : 1;
		unsigned can_xml_read : 1;
		unsigned can_xml_write : 1;
		unsigned is_scriptable : 1;
		unsigned always_clone : 1;
		unsigned mutability : 2;

		bool operator==(const PropertyDescriptor& other) const
		{
			return this == &other;
		}
		bool operator!=(const PropertyDescriptor& other) const
		{
			return this != &other;
		}

		virtual bool is_read_only() const = 0;
		virtual bool is_write_only() const = 0;
		virtual bool supports_styling() const = 0;
		virtual bool is_styling_read_only() const = 0;
		virtual void get_styled_variant(const DescribedBase* instance, Variant& out) const = 0;
		virtual void get_styled_variant(const StyledProperties* styled, Variant& out) const = 0;
		virtual void set_styled_variant(StyledProperties* styled, const Variant& value) const = 0;
		virtual bool are_styled_variants_equal(const StyledProperties* a, const StyledProperties* b) const = 0;
		virtual int get_large_asset_type() const = 0;
		virtual int get_defer_behavior() const = 0;
		virtual bool is_pull_interface() const = 0;
		virtual void* get_pull_info_item(DescribedBase* instance) const = 0;
		virtual void notify_pull_ready(DescribedBase* instance) const = 0;
		virtual bool equal_values(const DescribedBase* a, const DescribedBase* b) const = 0;
		virtual bool is_value_default_constructed(const DescribedBase* instance) const = 0;
		virtual void get_variant(const DescribedBase* instance, Variant& out) const = 0;
		virtual void get_variant_with_same_type_as_property(const DescribedBase* instance, Variant& out) const = 0;
		virtual void set_variant(DescribedBase* instance, const Variant& value) const = 0;
		virtual void set_variant_with_same_type_as_property(DescribedBase* instance, const Variant& value) const = 0;
		virtual void copy_value(const DescribedBase* source, DescribedBase* destination) const = 0;
		virtual int get_data_size(const DescribedBase* instance) const = 0;
		virtual bool supports_metadata(PropertyMetadataType type) const = 0;
		virtual bool has_string_value() const = 0;
		virtual std::string get_string_value(const DescribedBase* instance) const = 0;
		virtual bool set_string_value(DescribedBase* instance, const std::string& text) const = 0;
		virtual const PropertyDescriptor* get_as_delta_modifiable_prop_descriptor() const = 0;
		virtual bool is_typed_descriptor(const Type& type) const = 0;
		virtual void* get_metadata_value(const DescribedBase* instance, PropertyMetadataType type) const = 0;
		virtual void set_metadata_value(DescribedBase* instance, PropertyMetadataType type, detail::PropertyMetadataView view) const = 0;
		virtual void write_xml_value(const DescribedBase* instance, XmlElement* element, IReferenceBinderWrite& binder) const = 0;
		virtual void read_xml_value(DescribedBase* instance, const XmlElement* element, IReferenceBinder& binder) const = 0;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(PropertyDescriptor::Attributes, 0x20);
	RML_ASSERT_SIZE(PropertyDescriptor, 0x90);
	RML_ASSERT_REF_OFFSET(PropertyDescriptor, type, 0x68);
	RML_ASSERT_OFFSET(PropertyDescriptor, protection_set, 0x78);
	RML_ASSERT_OFFSET(PropertyDescriptor, index, 0x80);
	RML_ASSERT_OFFSET(PropertyDescriptor, is_enum, 0x8A);
	RML_LAYOUT_DIAGNOSTIC_POP()

	template<typename V>
	class TypedPropertyDescriptor : public PropertyDescriptor
	{
	public:
		class GetSet
		{
		public:
			virtual ~GetSet() = default;
			virtual bool is_read_only() const = 0;
			virtual bool is_write_only() const = 0;
			virtual V get_value(const DescribedBase* instance) const = 0;
			virtual void set_value(DescribedBase* instance, const V& value) const = 0;
			virtual bool equal_values(const DescribedBase* a, const DescribedBase* b) const = 0;
			virtual bool is_value_equal_to(const DescribedBase* instance, const V& value) const = 0;
		};

		class StyleGetSet;

	public:
		std::unique_ptr<GetSet> get_set;
		std::unique_ptr<StyleGetSet> style_get_set;
		std::uint64_t reserved_a0;

	public:
		[[nodiscard]] bool is_read_only() const override
		{
			return get_set ? get_set->is_read_only() : true;
		}
		[[nodiscard]] bool is_write_only() const override
		{
			return get_set ? get_set->is_write_only() : true;
		}

		V get(const DescribedBase* instance) const
		{
			return get_set->get_value(instance);
		}

		void set(DescribedBase* instance, const V& value) const
		{
			get_set->set_value(instance, value);
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(TypedPropertyDescriptor<float>, 0xA8);
	RML_ASSERT_OFFSET(TypedPropertyDescriptor<float>, get_set, 0x90);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class ConstProperty
	{
	protected:
		const PropertyDescriptor* descriptor;
		const DescribedBase* instance;

	public:
		ConstProperty() :
		    descriptor(nullptr),
		    instance(nullptr)
		{
		}
		ConstProperty(const PropertyDescriptor& descriptor, const DescribedBase* instance) :
		    descriptor(&descriptor),
		    instance(instance)
		{
		}

		ConstProperty(const ConstProperty& other) = default;

		[[nodiscard]] const DescribedBase* get_instance() const
		{
			return instance;
		}

		[[nodiscard]] const PropertyDescriptor& get_descriptor() const
		{
			return *descriptor;
		}

		ConstProperty& operator=(const ConstProperty& other) = default;

		bool operator==(const ConstProperty& other) const
		{
			return (this->descriptor == other.descriptor) && (this->instance == other.instance);
		}

		[[nodiscard]] const Name& name() const
		{
			return descriptor->name;
		}

		template<typename V>
		V get() const
		{
			return static_cast<const TypedPropertyDescriptor<V>*>(descriptor)->get(instance);
		}

		[[nodiscard]] bool has_string_value() const
		{
			return descriptor->has_string_value();
		}
		[[nodiscard]] std::string get_string_value() const
		{
			return descriptor->get_string_value(instance).c_str();
		}
	};

	class Property : public ConstProperty
	{
	public:
		inline Property(const PropertyDescriptor& descriptor, const DescribedBase* instance) :
		    ConstProperty(descriptor, instance)
		{
		}

		inline Property(const Property& other) :
		    ConstProperty(*other.descriptor, other.instance)
		{
		}
		inline Property& operator=(const Property& other)
		{
			this->descriptor = other.descriptor;
			this->instance = other.instance;
			return *this;
		}

		inline bool operator==(const Property& other) const
		{
			return this->descriptor == other.descriptor && this->instance == other.instance;
		}

		inline bool operator!=(const Property& other) const
		{
			return this->descriptor != other.descriptor || this->instance != other.instance;
		}

		DescribedBase* get_instance() const
		{
			return const_cast<DescribedBase*>(instance);
		}

		template<typename V>
		inline void set(const V& value)
		{
			static_cast<const TypedPropertyDescriptor<V>*>(descriptor)->set(const_cast<DescribedBase*>(instance), value);
		}

		inline bool set_string_value(const std::string& text) const
		{
			return descriptor->set_string_value(const_cast<DescribedBase*>(instance), text);
		}
	};

	class RefPropertyDescriptor : public PropertyDescriptor
	{
		typedef PropertyDescriptor Super;

	public:
		virtual DescribedBase* get_ref_value(const DescribedBase* instance) const = 0;
		virtual void set_ref_value(DescribedBase* instance, DescribedBase* value) const = 0;
		virtual void set_ref_value_unsafe(DescribedBase* instance, DescribedBase* value) const = 0;

		static bool is_ref_property_descriptor(const Type& type)
		{
			return type.tag == "Ref";
		}

		static bool is_ref_property_descriptor(const PropertyDescriptor& descriptor)
		{
			return is_ref_property_descriptor(descriptor.type);
		}
	};

	class InstanceHandle
	{
		std::shared_ptr<DescribedBase> m_target;

	public:
		InstanceHandle() = default;
		explicit InstanceHandle(DescribedBase* target);
		explicit InstanceHandle(const std::shared_ptr<DescribedBase>& target) :
		    m_target(target)
		{
		}
		InstanceHandle(const InstanceHandle& other) = default;

		InstanceHandle& operator=(const InstanceHandle& value) = default;
		InstanceHandle& operator=(const std::shared_ptr<DescribedBase>& value)
		{
			m_target = value;
			return *this;
		}

		[[nodiscard]] std::shared_ptr<DescribedBase> target() const
		{
			return m_target;
		}
	};

	class IIDREF
	{
		friend class IReferenceBinder;
		virtual void assign(DescribedBase* propertyOwner, const InstanceHandle& handle) const = 0;
	};
}