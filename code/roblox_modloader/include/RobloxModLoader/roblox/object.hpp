#pragma once
#include "flyweight.hpp"
#include "time.hpp"
#include "reflection/object.hpp"
#include "security/script_permissions.hpp"
#include "signals.hpp"

#include "RobloxModLoader/internal/engine_abi.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace RBX
{
	class Actor;
	class ComponentMap;
	class EngineContext;
	class RemoteInvocationMetadata;

	namespace DataModelChangeTracking
	{
		class ChangeTracker;
		enum class DeltaTypeTag : std::int32_t;
	}

	namespace Security
	{
		class Context;
	}

	namespace details
	{
		class AttributesAndTags;
	}

	class ObjectProp
	{
	};

	template<typename T>
	class GuidItem
	{
	public:
		void* scope;
		std::uint64_t index;
	};

	class RML_ENGINE_CLASS Object : public Reflection::DescribedBase, public ObjectProp
	{
	public:
		virtual void write_all_properties_for_change_tracking(
		    DataModelChangeTracking::ChangeTracker* tracker, DataModelChangeTracking::DeltaTypeTag tag)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void deprecated_predelete()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_property_changed(const Reflection::PropertyDescriptor& descriptor)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_guid_changed()
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool security_check(const Security::Context& context) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool security_check_permission_level_only(const Security::Context& context) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool security_check_capabilities(Security::Capabilities capabilities) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool security_check_capabilities_permission_level_only(
		    Security::Capabilities capabilities) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual Actor* get_actor()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void set_modified_flag(const Reflection::PropertyDescriptor* descriptor, bool modified)
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool filter_event_invocation(const Reflection::EventDescriptor& descriptor,
		    const RemoteInvocationMetadata& metadata, Reflection::SystemAddress source)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void* to_content_ownership()
		{
			rml::engine_virtual_unreachable();
		}

		virtual std::string get_object_name() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual std::string get_full_name() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void record_property_update_if_eligible(Time when, const std::string& property)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void raise_property_changed_post_virtual(
		    const Reflection::PropertyDescriptor& descriptor)
		{
			rml::engine_virtual_unreachable();
		}

		GuidItem<Object> guid;
		union
		{
			std::unique_ptr<ComponentMap> components;
		};
		union
		{
			std::unique_ptr<details::AttributesAndTags> attributes_and_tags;
		};
		union
		{
			rbx::signal<void(std::shared_ptr<Instance>, std::shared_ptr<Instance>)> ancestry_changed_signal;
		};
		union
		{
			rbx::signal<void(const Reflection::PropertyDescriptor*)> property_changed_signal;
		};
		EngineContext* engine_context;

	protected:
		Object()
		{
		}

	public:
		~Object() override
		{
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(Object, guid, 0x28);
	RML_ASSERT_OFFSET(Object, components, 0x38);
	RML_ASSERT_OFFSET(Object, attributes_and_tags, 0x40);
	RML_ASSERT_OFFSET(Object, ancestry_changed_signal, 0x48);
	RML_ASSERT_OFFSET(Object, property_changed_signal, 0x50);
	RML_ASSERT_OFFSET(Object, engine_context, 0x58);
	RML_ASSERT_SIZE(Object, 0x60);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
