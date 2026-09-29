#include "metadata_registry.hpp"

#include "reflected_access.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/mod/events.hpp"
#include "RobloxModLoader/platform/memory/memory_protection.hpp"
#include "RobloxModLoader/qt/qt_integration.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/metadata/reflection_metadata.hpp"
#include "RobloxModLoader/roblox/reflection/property_descriptor.hpp"
#include "app/init_gate.hpp"
#include "pointers.hpp"

#include <format>
#include <set>
#include <utility>

RML_LOG_SCOPE("Metadata");

namespace rml::reflection
{
	using RBX::Reflection::Metadata::Class;
	using RBX::Reflection::Metadata::Item;
	using RBX::Reflection::Metadata::Member;
	using RBX::Reflection::Metadata::Properties;
	using RBX::Reflection::Metadata::Reflection;

	static void warn_once(const std::string& key, const std::string& message)
	{
		static std::mutex reported_mutex;
		static std::set<std::string, std::less<>> reported;
		std::lock_guard lock(reported_mutex);
		if (reported.insert(key).second)
			RML_WARN("{}", message);
	}

	static void report(const std::expected<void, std::string>& result)
	{
		if (!result)
			warn_once(result.error(), std::format("{}; that hint is ignored", result.error()));
	}

	static void set_instance_name(RBX::Instance& instance, const std::string_view name)
	{
		if (const auto* descriptor = instance.get_descriptor().find_property("Name"))
			descriptor->set_string_value(&instance, std::string(name));
	}

	static RBX::Instance* create_item(RBX::Instance& parent, const char* class_name, const std::string_view name)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		const auto* atom = p.get_string_atom ? p.get_string_atom(class_name) : nullptr;
		auto created = atom && p.object_create_by_name ? p.object_create_by_name(nullptr, *atom, RBX::CreatorRole::Engine) : nullptr;
		if (!created)
		{
			RML_ERROR("could not create a {}", class_name);
			return nullptr;
		}
		set_instance_name(*created, name);
		const auto* parent_property = created->get_descriptor().find_property("Parent");
		if (!parent_property)
			return nullptr;
		static_cast<const RBX::Reflection::RefPropertyDescriptor*>(parent_property)->set_ref_value(created.get(), &parent);
		return created.get();
	}

	static bool is_instance_of(const void* candidate, const std::string_view class_name)
	{
		if (!candidate || reinterpret_cast<std::uintptr_t>(candidate) % alignof(void*) != 0 || !platform::is_readable(candidate, sizeof(RBX::Instance)))
			return false;
		return static_cast<const RBX::Instance*>(candidate)->get_descriptor().is_a(class_name.data());
	}

	static bool validate(const Reflection& root)
	{
		if (!is_instance_of(root.classes, RBX::Reflection::Metadata::Classes::class_name))
			return false;
		const auto* sound = static_cast<const Class*>(std::as_const(*root.classes).find_first_child_by_name("Sound"));
		const auto* properties = is_instance_of(sound, Class::class_name) ? sound->find_first_child_of_type<Properties>() : nullptr;
		const auto* volume = properties ? static_cast<const Member*>(properties->find_first_child_by_name("Volume")) : nullptr;
		if (!is_instance_of(volume, Member::class_name) || !(volume->ui_minimum.get() < volume->ui_maximum.get()))
			return false;
		const bool numbers = mirrors(*volume, &Item::ui_minimum) && mirrors(*volume, &Item::ui_maximum) && mirrors(*volume, &Item::ui_num_ticks) &&
		                     mirrors(*volume, &Item::property_order) && mirrors(*sound, &Class::explorer_image_index) && mirrors(*sound, &Class::explorer_order) &&
		                     mirrors(*sound, &Class::insertable);
		if (!numbers || !mirrors(*sound, &Class::preferred_parent) || !mirrors(*sound, &Item::class_category) || volume->description.size() > 0x10000)
			return false;
		RML_INFO("metadata tree validated (Sound.Volume slider {}..{})", volume->ui_minimum.get(), volume->ui_maximum.get());
		return true;
	}

	static void write_class(Class& item, const RBX::Reflection::Metadata::Classes& classes, const ClassHints& hints)
	{
		report(assign(item, &Class::insertable, hints.insertable.value_or(true)));
		if (hints.insert_category)
			report(assign(item, &Item::class_category, *hints.insert_category));
		if (hints.explorer_order)
			report(assign(item, &Class::explorer_order, *hints.explorer_order));
		if (hints.preferred_parent)
			report(assign(item, &Class::preferred_parent, *hints.preferred_parent));
		if (hints.browsable)
			report(assign(item, &Item::browsable, *hints.browsable));
		if (hints.icon_of)
		{
			if (const auto* source = static_cast<const Class*>(classes.find_first_child_by_name(*hints.icon_of)))
				report(assign(item, &Class::explorer_image_index, source->explorer_image_index.get()));
			else
				RML_WARN("icon_of('{}'): no metadata for that class", *hints.icon_of);
		}
		if (hints.description)
			item.description = *hints.description;
	}

	static void write_member(Member& item, const PropertyHints& hints)
	{
		if (hints.order)
			report(assign(item, &Item::property_order, *hints.order));
		if (hints.read_only)
			report(assign(item, &Item::editing_disabled, true));
		if (hints.hidden)
			report(assign(item, &Item::browsable, false));
		if (hints.deprecated)
			report(assign(item, &Item::deprecated, true));
		if (const auto& slider = hints.slider)
		{
			report(assign(item, &Item::ui_minimum, slider->min));
			report(assign(item, &Item::ui_maximum, slider->max));
			if (slider->ticks > 0)
				report(assign(item, &Item::ui_num_ticks, static_cast<double>(slider->ticks)));
			if (slider->scaling == SliderScaling::Square)
				report(assign(item, &Item::slider_scaling, std::string("Square")));
		}
		if (hints.description)
			item.description = *hints.description;
		else if (hints.deprecated && !hints.deprecated->empty())
			item.description = *hints.deprecated;
	}

	MetadataRegistry& MetadataRegistry::instance()
	{
		static MetadataRegistry registry;
		return registry;
	}

	MetadataRegistry::State MetadataRegistry::state() const
	{
		return m_state.load(std::memory_order_acquire);
	}

	void MetadataRegistry::disable(const std::string_view reason)
	{
		if (m_state.exchange(State::Disabled, std::memory_order_acq_rel) != State::Disabled)
			RML_ERROR("mod metadata disabled: {}", reason);
	}

	void MetadataRegistry::add(ClassMetadata metadata)
	{
		std::lock_guard lock(m_mutex);
		m_pending.push_back(std::move(metadata));
		schedule_fallback();
		try_apply(Reflection::singleton(), false);
	}

	void MetadataRegistry::on_tree_loaded(void* candidate)
	{
		std::lock_guard lock(m_mutex);
		if (state() == State::Disabled)
			return;
		auto* root = Reflection::identify(candidate);
		if (!root)
		{
			disable("Reflection::load was not called on the ReflectionMetadata singleton");
			return;
		}
		for (auto& metadata : m_applied)
			m_pending.push_back(std::move(metadata));
		m_applied.clear();
		try_apply(root, true);
	}

	void MetadataRegistry::schedule_fallback()
	{
		std::call_once(m_fallback_once, [this] {
			events::event_manager().register_handler<events::DataModelChangedEvent>([this](events::DataModelChangedEvent&) {
				if (auto* qt = qt::QtIntegration::instance())
					qt->run_on_gui_thread([this] {
						std::lock_guard lock(m_mutex);
						if (!m_pending.empty())
							try_apply(Reflection::singleton(), true);
					});
			});
		});
	}

	void MetadataRegistry::try_apply(Reflection* root, const bool required)
	{
		std::lock_guard lock(m_mutex);
		if (state() == State::Disabled || m_pending.empty())
			return;
		if (!root || !validate(*root))
		{
			if (required)
				disable(root ? "the ReflectionMetadata tree does not have the expected shape" : "the ReflectionMetadata singleton was not found");
			return;
		}
		auto pending = std::exchange(m_pending, {});
		for (auto& metadata : pending)
		{
			try
			{
				apply(*root, metadata);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("metadata for {} failed: {}", metadata.descriptor->name.to_string(), e.what());
			}
			catch (...)
			{
				RML_ERROR("metadata for {} failed with an unknown exception", metadata.descriptor->name.to_string());
			}
			m_applied.push_back(std::move(metadata));
		}
		m_state.store(State::Applied, std::memory_order_release);
	}

	void MetadataRegistry::apply(Reflection& root, const ClassMetadata& metadata)
	{
		const auto class_name = metadata.descriptor->name.to_string();
		std::size_t created = 0;
		auto* item = root.get(*metadata.descriptor, false);
		if (!item && root.classes)
		{
			item = static_cast<Class*>(create_item(*root.classes, Class::class_name.data(), class_name));
			created += item != nullptr;
		}
		if (!item)
			return;

		if (metadata.owned_class)
			write_class(*item, *root.classes, metadata.hints);

		for (const auto& property : metadata.properties)
		{
			auto* member = root.get(*property.descriptor);
			if (!member)
			{
				auto* container = item->find_first_child_of_type<Properties>();
				if (!container)
				{
					container = static_cast<Properties*>(create_item(*item, Properties::class_name.data(), Properties::class_name));
					created += container != nullptr;
				}
				member = container ? static_cast<Member*>(create_item(*container, Member::class_name.data(), property.descriptor->name.to_string())) : nullptr;
				created += member != nullptr;
			}
			if (member)
				write_member(*member, property.hints);
		}

		RML_INFO("metadata applied for {} ({} member(s), {} new item(s))", class_name, metadata.properties.size(), created);
	}
}
