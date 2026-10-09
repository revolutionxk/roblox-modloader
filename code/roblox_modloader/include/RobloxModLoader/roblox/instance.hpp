#pragma once
#include "object.hpp"

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RBX
{
	using namespace Reflection;

	class Instance;
	class Name;
	class ServiceProvider;
	class AncestorChanged;
	class ScalablePropertySource;
	class ScalablePropertyTarget;

	enum class CreatorRole : std::int32_t;

	namespace Reflection
	{
		class StyledProperties;
	}

	using Instances = std::vector<std::shared_ptr<Instance>>;

	class InstanceProp
	{
	private:
		[[maybe_unused]] boost::intrusive_ptr<rbx::signals::slots_holder> reserved_0;

	public:
		Instance* parent;
		Flyweight<std::string> name;
	};

	class RML_EXPORT Instance : public Object, public InstanceProp
	{
	public:
		virtual bool styled_properties_read() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void set_styled_properties(
		    boost::intrusive_ptr<Reflection::StyledProperties> properties)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void set_is_archivable(bool archivable)
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool is_property_hidden_in_studio(
		    const Reflection::PropertyDescriptor* descriptor) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool is_enum_hidden_in_studio(const Reflection::PropertyDescriptor* descriptor,
		    int value) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool is_instance_ignored_by_change_history() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void* get_override_instances(
		    const Reflection::PropertyDescriptor* descriptor) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void* get_override_instance_properties(
		    const Reflection::PropertyDescriptor* descriptor) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual int get_property_status_in_studio_internal() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void destroy()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void humanoid_changed()
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool has_scalable_properties()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void get_scalable_properties(ScalablePropertyTarget& target)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void set_scalable_properties(ScalablePropertySource& source)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void scale_scalable_properties(ScalablePropertySource& source,
		    ScalablePropertyTarget& target, float scale)
		{
			rml::engine_virtual_unreachable();
		}

		virtual std::shared_ptr<Instance> lua_clone()
		{
			rml::engine_virtual_unreachable();
		}

		virtual int get_persistent_data_cost_internal() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool can_serialize_user_data() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool can_client_create()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_service_provider(ServiceProvider* old_provider,
		    ServiceProvider* new_provider)
		{
			rml::engine_virtual_unreachable();
		}

		virtual std::shared_ptr<Instance> create_child(const Name& class_name, CreatorRole role)
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool verify_set_parent(const Instance* new_parent) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool verify_set_ancestor(const Instance* new_ancestor,
		    const Instance* old_ancestor) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void signal_completeness_change()
		{
			rml::engine_virtual_unreachable();
		}

		virtual int get_override_image_index() const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool get_overflow_modified_bit(unsigned int index) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void set_overflow_modified_bit(unsigned int index, bool value)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void clear_overflow_modified()
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool verify_add_child(const Instance* child) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool verify_add_descendant(const Instance* descendant,
		    const Instance* ancestor) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool ask_add_child(const Instance* child) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool ask_forbid_child(const Instance* child) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool ask_forbid_parent(const Instance* parent) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool ask_set_parent(const Instance* parent) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_ancestor_changed(const AncestorChanged& change)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void pre_on_ancestor_changed(const AncestorChanged& change)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_descendant_added(Instance* descendant)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_descendant_removing(const std::shared_ptr<Instance>& descendant)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_child_added(Instance* child)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_child_removing(Instance* child)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_child_removed(Instance* child)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_child_changed(Instance* child,
		    const Reflection::PropertyDescriptor& descriptor)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_descendant_changed(const std::shared_ptr<Instance>& descendant,
		    const Reflection::PropertyDescriptor& descriptor)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void on_styled_properties_changed(
		    boost::intrusive_ptr<Reflection::StyledProperties> old_properties,
		    boost::intrusive_ptr<Reflection::StyledProperties> new_properties)
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool get_styled_impl(const std::string& key,
		    const std::optional<std::string>& modifier) const
		{
			rml::engine_virtual_unreachable();
		}

		virtual bool filter_name_callback_for_subclass(const std::string& name)
		{
			rml::engine_virtual_unreachable();
		}

		virtual std::string transform_name_callback(std::string name)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void pre_equality_check_name_callback_for_subclass(const std::string& name)
		{
			rml::engine_virtual_unreachable();
		}

		virtual void pre_prop_signal_name_callback_for_subclass()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void post_prop_signal_name_callback_for_subclass()
		{
			rml::engine_virtual_unreachable();
		}

		virtual void post_equality_check_name_callback_for_subclass()
		{
			rml::engine_virtual_unreachable();
		}

		union
		{
			std::shared_ptr<std::vector<std::shared_ptr<Instance>>> children;
		};
		std::uint32_t num_local_replicated_children;

	private:
		[[maybe_unused]] std::uint32_t reserved_8c;
		[[maybe_unused]] std::uint32_t reserved_90;
		[[maybe_unused]] std::uint16_t reserved_94;
		[[maybe_unused]] std::uint8_t reserved_96;
		[[maybe_unused]] std::uint8_t reserved_97[0x2B];

	public:
		std::uint16_t flags;

	private:
		[[maybe_unused]] std::uint8_t reserved_c4[4];

	public:
		[[nodiscard]] bool is_archivable() const noexcept
		{
			return (flags & 0x8) != 0;
		}

	protected:
		Instance()
		{
		}

	public:
		~Instance() override
		{
		}

		template<typename T = Instance>
		T* as()
		{
			return static_cast<T*>(this);
		}

		std::string compute_full_name();

		[[nodiscard]] std::size_t num_children() const;
		[[nodiscard]] Instance* get_child(std::size_t index);
		[[nodiscard]] const Instance* get_child(std::size_t index) const;
		[[nodiscard]] const Instance* find_first_child_by_name(std::string_view find_name) const;
		[[nodiscard]] Instance* find_first_child_by_name(std::string_view find_name);
		[[nodiscard]] const Instance* find_first_child_of_type(std::string_view class_name) const;
		[[nodiscard]] Instance* find_first_child_of_type(std::string_view class_name);

		template<typename C>
		[[nodiscard]] const C* find_first_child_of_type() const
		{
			if (children)
			{
				for (const auto& child : *children)
				{
					if (child && child->get_descriptor().is_a(C::class_name.data()))
						return static_cast<const C*>(child.get());
				}
			}
			return nullptr;
		}

		template<typename C>
		[[nodiscard]] C* find_first_child_of_type()
		{
			return const_cast<C*>(std::as_const(*this).template find_first_child_of_type<C>());
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(Instance, parent, 0x68);
	RML_ASSERT_OFFSET(Instance, name, 0x70);
	RML_ASSERT_OFFSET(Instance, children, 0x78);
	RML_ASSERT_OFFSET(Instance, num_local_replicated_children, 0x88);
	RML_ASSERT_OFFSET(Instance, flags, 0xC2);
	RML_ASSERT_SIZE(Instance, 0xC8);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
