#include "class_registry.hpp"

#include "category_labels.hpp"
#include "enum_registry.hpp"
#include "metadata_registry.hpp"
#include "mod_descriptors.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/memory/vtable.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/roblox/reflection/described_creatable.hpp"
#include "RobloxModLoader/mod/init_context.hpp"
#include "RobloxModLoader/roblox/util/array_view.hpp"
#include "app/init_gate.hpp"
#include "pointers.hpp"

#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

RML_LOG_SCOPE("ClassRegistry");

namespace rml::reflection
{
	struct ConstructArgs
	{
		RegisteredClass* entry;
		RBX::ForceConstructionInCreatable force;
	};

	static const RBX::Reflection::ClassDescriptor* descriptor_of(const void* instance)
	{
		return static_cast<const RBX::Reflection::DescribedBase*>(instance)->descriptor;
	}

	static void* construct_mod_instance(void* object, const void* args)
	{
		const auto& [entry, force] = *static_cast<const ConstructArgs*>(args);
		g_pointers->m_roblox_pointers.instance_ctor(object, &force, entry->name.c_str());
		entry->layout.construct(object);

		memory::set_vtable(object, ClassRegistry::instance().vtable_for(*entry, memory::vtable_of(object)));
		static_cast<RBX::Reflection::DescribedBase*>(object)->descriptor = entry->descriptor;
		return object;
	}

	static std::size_t engine_vtable_slots(void* const* vtable)
	{
		const memory::module image(platform::studio_image_name());
		std::size_t slots = 0;
		while (image.contains(memory::handle(vtable[slots])))
			++slots;
		return slots;
	}

#if defined(RML_WINDOWS)
	static void* mod_scalar_deleting_dtor(void* self, unsigned int flags)
	{
		auto* entry = ClassRegistry::instance().class_of(self);
		entry->layout.destroy(self);
		return reinterpret_cast<void* (*)(void*, unsigned int)>(entry->engine_vtable[platform::abi::scalar_deleting_destructor_slot])(self, flags);
	}
#else
	static void mod_complete_dtor(void* self)
	{
		auto* entry = ClassRegistry::instance().class_of(self);
		entry->layout.destroy(self);
		reinterpret_cast<void (*)(void*)>(entry->engine_vtable[platform::abi::complete_destructor_slot])(self);
	}

	static void mod_deleting_dtor(void* self)
	{
		auto* entry = ClassRegistry::instance().class_of(self);
		entry->layout.destroy(self);
		reinterpret_cast<void (*)(void*)>(entry->engine_vtable[platform::abi::deleting_destructor_slot])(self);
	}
#endif

	static std::uint32_t memory_category_of(const RBX::Reflection::ClassDescriptor* descriptor)
	{
		for (; descriptor; descriptor = descriptor->base)
		{
			if (descriptor->memory_category)
				return *descriptor->memory_category;
		}
		return 0;
	}

	static const RBX::Reflection::PropertyDescriptor* shadowed_property(const RBX::Reflection::ClassDescriptor& owner, const std::string_view name, const RBX::Reflection::PropertyDescriptor* self)
	{
		const auto& container = static_cast<const RBX::Reflection::MemberDescriptorContainer<RBX::Reflection::PropertyDescriptor>&>(owner);
		const auto other = [&](const RBX::Reflection::PropertyDescriptor* descriptor) { return descriptor && descriptor != self && descriptor->name.to_string() == name; };
		if (container.finalized)
		{
			for (const auto* descriptor : container.get_descriptor_view())
			{
				if (other(descriptor))
					return descriptor;
			}
			return nullptr;
		}
		for (const auto* current = &container; current; current = current->base_container)
		{
			for (const auto& view : current->views)
			{
				for (const auto* descriptor : view)
				{
					if (other(descriptor))
						return descriptor;
				}
			}
		}
		return nullptr;
	}

	static std::expected<void, std::string> check_properties(const std::string& owner, const std::vector<PropertySpec>& properties, const RBX::Reflection::ClassDescriptor* existing)
	{
		std::set<std::string_view> names;
		for (const auto& property : properties)
		{
			if (!names.insert(property.name).second)
				return std::unexpected(std::format("{}: property '{}' is declared twice", owner, property.name));
			if (property.type == PropertyType::Enum && !EnumRegistry::instance().find(property.enum_key))
				return std::unexpected(std::format("{}.{}: its enum type is not registered; call define_enum or bind_enum for it earlier in on_init", owner, property.name));
			if (existing && shadowed_property(*existing, property.name, nullptr))
				return std::unexpected(std::format("class '{}' already has a property named '{}'", owner, property.name));
			if (const auto& slider = property.hints.slider)
			{
				if (!std::isfinite(slider->min) || !std::isfinite(slider->max) || slider->min >= slider->max)
					return std::unexpected(std::format("{}.{}: a slider needs finite bounds with min < max (got {} and {})", owner, property.name, slider->min, slider->max));
				if (slider->ticks < 0)
					return std::unexpected(std::format("{}.{}: slider ticks must not be negative (got {})", owner, property.name, slider->ticks));
			}
		}
		return {};
	}

	template<typename Entry>
	static std::expected<void, std::string> build_members(Entry& entry, void* owner, const std::vector<PropertySpec>& properties, const std::vector<FunctionSpec>& functions)
	{
		for (const auto& property : properties)
		{
			auto member = property.type == PropertyType::Enum
			                  ? make_enum_property(owner, property.name, property.category, *EnumRegistry::instance().find(property.enum_key), property.accessor.get())
			                  : make_property(owner, property.name, property.category, property.type, property.accessor.get());
			if (!member)
				return std::unexpected(member.error());
			if (property.hints.deprecated)
				reinterpret_cast<RBX::Reflection::Descriptor*>(member->storage.get())->attributes.is_deprecated = true;
			entry.property_table.push_back(reinterpret_cast<const RBX::Reflection::PropertyDescriptor*>(member->storage.get()));
			entry.member_storage.push_back(std::move(member->storage));
			entry.accessors.push_back(property.accessor);
		}

		for (const auto& function : functions)
		{
			auto member = make_function(owner, function.name, function.invoker.get());
			if (!member)
				return std::unexpected(member.error());
			entry.function_table.push_back(reinterpret_cast<const RBX::Reflection::FunctionDescriptor*>(member->storage.get()));
			entry.member_storage.push_back(std::move(member->storage));
			entry.invokers.push_back(function.invoker);
		}
		return {};
	}

	static void publish_metadata(const RBX::Reflection::ClassDescriptor* descriptor, const bool owned_class, const ClassHints& hints, const std::vector<PropertySpec>& properties, const std::vector<const RBX::Reflection::PropertyDescriptor*>& table)
	{
		try
		{
			if (hints.insert_category)
				CategoryLabels::instance().add(*hints.insert_category);
			ClassMetadata metadata{descriptor, owned_class, hints, {}};
			for (std::size_t i = 0; i < properties.size() && i < table.size(); ++i)
			{
				CategoryLabels::instance().add(properties[i].category);
				if (!properties[i].hints.empty())
					metadata.properties.push_back(PropertyMetadata{table[i], properties[i].hints});
			}
			if (owned_class || !metadata.properties.empty())
				MetadataRegistry::instance().add(std::move(metadata));
		}
		catch (const std::exception& e)
		{
			RML_ERROR("metadata for {} could not be published: {}", descriptor->name.to_string(), e.what());
		}
		catch (...)
		{
			RML_ERROR("metadata for {} could not be published: unknown exception", descriptor->name.to_string());
		}
	}

	ClassRegistry& ClassRegistry::instance()
	{
		static ClassRegistry registry;
		return registry;
	}

	bool ClassRegistry::available()
	{
		if (!g_pointers)
			return false;

		const auto& p = g_pointers->m_roblox_pointers;
		return p.class_descriptor_ctor && p.class_descriptor_all_classes && p.creatable_get_creator && p.instance_ctor && p.create_instance_impl;
	}

	template<typename T>
	static bool container_totals_match(const RBX::Reflection::ClassDescriptor* descriptor)
	{
		const auto& container = static_cast<const RBX::Reflection::MemberDescriptorContainer<T>&>(*descriptor);
		if (container.finalized)
			return false;

		std::uint64_t counted = 0;
		for (const auto* current = &container; current; current = current->base_container)
		{
			for (const auto& view : current->views)
				counted += view.size;
		}
		return counted == container.total;
	}

	std::expected<void, std::string> ClassRegistry::validate()
	{
		if (m_validation)
			return *m_validation;

		const auto check = [&]() -> std::expected<void, std::string> {
			auto* instance = find_engine_class("Instance");
			auto* folder = find_engine_class("Folder");
			if (!instance || !folder)
				return std::unexpected("Instance or Folder descriptor missing from allClasses");

			if (folder->base != instance)
				return std::unexpected("Folder.base does not point at Instance; ClassDescriptor layout drifted");

			if (!container_totals_match<RBX::Reflection::PropertyDescriptor>(folder) || !container_totals_match<RBX::Reflection::FunctionDescriptor>(folder))
				return std::unexpected("member container views do not add up to total; MemberDescriptorContainerV2 layout drifted");

			auto* const index = memory::rtti();
			if (!index)
				return std::unexpected("no RTTI index");

			const auto vtable = index->find("RBX::Instance");
			if (!vtable)
				return std::unexpected("RBX::Instance vtable not found");

			const auto slots = engine_virtual_slots<RBX::Instance>::value();
			const auto engine_slots = engine_vtable_slots(*vtable);
			if (engine_slots < slots)
				return std::unexpected(std::format("RBX::Instance vtable slot {} is not code; the Instance mirror has more virtuals than the engine", engine_slots));
			if (engine_slots > slots)
				return std::unexpected(std::format("RBX::Instance vtable has more than {} slots; the Instance mirror is missing virtuals", slots));

			return {};
		};

		m_validation = check();
		if (!*m_validation)
			RML_ERROR("reflection registration disabled: {}", m_validation->error());
		return *m_validation;
	}

	RBX::Reflection::ClassDescriptor* ClassRegistry::find_engine_class(std::string_view name) const
	{
		const auto all = g_pointers->m_roblox_pointers.class_descriptor_all_classes();
		if (!all)
			return nullptr;

		for (auto* descriptor : *all)
		{
			if (descriptor && descriptor->name.to_string() == name)
				return descriptor;
		}

		return nullptr;
	}

	std::expected<RBX::Reflection::ClassDescriptor*, std::string> ClassRegistry::define(const ClassSpec& spec)
	{
		if (!available())
			return std::unexpected("reflection registration is unavailable on this build");

		if (!g_init_gate || !g_init_gate->is_open())
			return std::unexpected("class registration is only possible inside on_init");

		if (const auto valid = validate(); !valid)
			return std::unexpected(valid.error());

		if (spec.name.empty())
			return std::unexpected("class name is empty");

		if (find_engine_class(spec.name))
			return std::unexpected(std::format("class '{}' already exists", spec.name));

		auto* base = find_engine_class(spec.base);
		if (!base)
			return std::unexpected(std::format("base class '{}' not found", spec.base));

		for (const auto& event : spec.events)
		{
			if (std::ranges::any_of(event.arguments, [](const EventArgument& argument) { return argument.type == PropertyType::Enum; }))
				return std::unexpected(std::format("{}.{}: enum event arguments are not supported yet", spec.name, event.name));
		}

		if (auto checked = check_properties(spec.name, spec.properties, base); !checked)
			return std::unexpected(checked.error());

		auto* const index = memory::rtti();
		if (!index)
			return std::unexpected("no RTTI index; engine vtables are unreachable");

		const auto engine_vtable = index->find("RBX::" + spec.base);
		if (!engine_vtable)
			return std::unexpected(std::format("no engine vtable for RBX::{}", spec.base));

		auto& entry = m_classes.emplace_back();
		entry.name = spec.name;
		entry.layout = spec.layout;
		entry.base = base;
		entry.engine_vtable = *engine_vtable;
		entry.storage = std::make_unique<std::byte[]>(sizeof(RBX::Reflection::ClassDescriptor));

		if (auto built = build_members(entry, entry.storage.get(), spec.properties, spec.functions); !built)
		{
			m_classes.pop_back();
			return std::unexpected(built.error());
		}

		for (const auto& event : spec.events)
		{
			auto member = make_event(entry.storage.get(), event.name, event.member_offset, event.arguments);
			if (!member)
			{
				m_classes.pop_back();
				return std::unexpected(member.error());
			}

			const auto* descriptor = reinterpret_cast<const RBX::Reflection::EventDescriptor*>(member->storage.get());
			entry.event_table.push_back(descriptor);
			entry.events_by_offset[event.member_offset] = descriptor;
			entry.member_storage.push_back(std::move(member->storage));
		}

		static const RBX::Reflection::ClassDescriptor::Attributes attributes(RBX::Reflection::ClassDescriptor::PERSISTENT_LOCAL);
		const auto& p = g_pointers->m_roblox_pointers;
		p.class_descriptor_ctor(entry.storage.get(), base, entry.name.c_str(), 0, 0, false, false, &attributes, RBX::Security::Permissions::None, nullptr,
		    RBX::ArrayView<const RBX::Reflection::PropertyDescriptor*>{entry.property_table},
		    RBX::ArrayView<const RBX::Reflection::EventDescriptor*>{entry.event_table},
		    RBX::ArrayView<const RBX::Reflection::FunctionDescriptor*>{entry.function_table},
		    RBX::ArrayView<const RBX::Reflection::YieldFunctionDescriptor*>{},
		    RBX::ArrayView<const RBX::Reflection::CallbackDescriptor*>{});

		entry.descriptor = reinterpret_cast<RBX::Reflection::ClassDescriptor*>(entry.storage.get());
		entry.creator = std::make_unique<ModInstanceCreator>(entry);
		m_creators[&entry.descriptor->name] = entry.creator.get();
		m_by_descriptor[entry.descriptor] = &entry;

		publish_metadata(entry.descriptor, true, spec.hints, spec.properties, entry.property_table);
		RML_INFO("Registered class {} : {} ({} bytes, {} properties, {} functions, {} events, descriptor 0x{:X})", spec.name, spec.base, spec.layout.size,
		    entry.property_table.size(), entry.function_table.size(), entry.event_table.size(), reinterpret_cast<std::uintptr_t>(entry.descriptor));
		return entry.descriptor;
	}

	template<typename T>
	static void append_members(RBX::Reflection::ClassDescriptor* descriptor, const std::vector<const T*>& table)
	{
		if (table.empty())
			return;

		auto& container = static_cast<RBX::Reflection::MemberDescriptorContainer<T>&>(*descriptor);
		container.views.push_back(RBX::ArrayView<const T*>{table});

		std::vector<RBX::Reflection::ClassDescriptor*> pending{descriptor};
		while (!pending.empty())
		{
			auto* current = pending.back();
			pending.pop_back();
			static_cast<RBX::Reflection::MemberDescriptorContainer<T>&>(*current).total += table.size();
			pending.insert(pending.end(), current->derived_classes.begin(), current->derived_classes.end());
		}
	}

	std::expected<RBX::Reflection::ClassDescriptor*, std::string> ClassRegistry::extend(const ExtensionSpec& spec)
	{
		if (!available())
			return std::unexpected("reflection registration is unavailable on this build");

		if (!g_init_gate || !g_init_gate->is_open())
			return std::unexpected("class extension is only possible inside on_init");

		if (const auto valid = validate(); !valid)
			return std::unexpected(valid.error());

		auto* descriptor = find_engine_class(spec.name);
		if (!descriptor)
			return std::unexpected(std::format("class '{}' not found", spec.name));

		if (static_cast<RBX::Reflection::MemberDescriptorContainer<RBX::Reflection::PropertyDescriptor>&>(*descriptor).finalized)
			return std::unexpected(std::format("class '{}' is already finalized", spec.name));

		if (auto checked = check_properties(spec.name, spec.properties, descriptor); !checked)
			return std::unexpected(checked.error());

		auto& entry = m_extensions.emplace_back();
		entry.descriptor = descriptor;

		if (auto built = build_members(entry, descriptor, spec.properties, spec.functions); !built)
		{
			m_extensions.pop_back();
			return std::unexpected(built.error());
		}

		append_members(descriptor, entry.property_table);
		append_members(descriptor, entry.function_table);

		publish_metadata(descriptor, false, {}, spec.properties, entry.property_table);
		RML_INFO("Extended class {} with {} properties and {} functions", spec.name, entry.property_table.size(), entry.function_table.size());
		return descriptor;
	}

	void ClassRegistry::report_engine_collisions() const
	{
		const auto report = [](const RBX::Reflection::ClassDescriptor& scope, const std::string_view mod_class, const RBX::Reflection::PropertyDescriptor* property) {
			if (shadowed_property(scope, property->name.to_string(), property))
				RML_ERROR("{}.{} has the same name as an engine property of {}; scripts and Studio may resolve either one, rename it", mod_class, property->name.to_string(), scope.name.to_string());
		};
		for (const auto& entry : m_classes)
		{
			for (const auto* property : entry.property_table)
				report(*entry.base, entry.name, property);
		}
		for (const auto& extension : m_extensions)
		{
			for (const auto* property : extension.property_table)
				report(*extension.descriptor, extension.descriptor->name.to_string(), property);
		}
	}

	RegisteredClass* ClassRegistry::class_of(const void* instance)
	{
		const auto it = m_by_descriptor.find(descriptor_of(instance));
		return it == m_by_descriptor.end() ? nullptr : it->second;
	}

	void* engine_virtual(const RBX::Instance* instance, const std::size_t slot)
	{
		const auto* entry = ClassRegistry::instance().class_of(instance);
		if (!entry || !entry->engine_vtable)
			throw std::logic_error("engine_virtual called on an instance that is not a mod class");
		return entry->engine_vtable[slot];
	}

	bool engine_base_overrides(const RBX::Instance* instance, const std::size_t slot)
	{
		static void** const instance_vtable = [] {
			auto* const index = memory::rtti();
			const auto vtable = index ? index->find("RBX::Instance") : std::nullopt;
			return vtable ? *vtable : nullptr;
		}();
		const auto* entry = ClassRegistry::instance().class_of(instance);
		if (!entry || !entry->engine_vtable || !instance_vtable)
			return false;
		return entry->engine_vtable[slot] != instance_vtable[slot];
	}

	void* const* ClassRegistry::vtable_for(RegisteredClass& entry, void* const* derived_vtable)
	{
		std::call_once(entry.vtable_once, [&] {
			auto& vtable = entry.vtable = memory::VtableCopy(entry.engine_vtable, engine_vtable_slots(entry.engine_vtable));

#if defined(RML_WINDOWS)
			vtable.set(platform::abi::scalar_deleting_destructor_slot, reinterpret_cast<void*>(&mod_scalar_deleting_dtor));
#else
			vtable.set(platform::abi::complete_destructor_slot, reinterpret_cast<void*>(&mod_complete_dtor));
			vtable.set(platform::abi::deleting_destructor_slot, reinterpret_cast<void*>(&mod_deleting_dtor));
#endif

			std::size_t merged = 0;
			for (std::size_t slot = platform::abi::destructor_slots; slot < entry.layout.virtual_slots && slot < vtable.size(); ++slot)
			{
				if (derived_vtable[slot] == entry.layout.base_vtable[slot])
					continue;
				vtable.set(slot, derived_vtable[slot]);
				++merged;
			}

			RML_INFO("Built vtable for {} from RBX::{} at 0x{:X}: {} of {} slots overridden", entry.name, entry.base->name.to_string(),
			    reinterpret_cast<std::uintptr_t>(entry.engine_vtable), merged, entry.layout.virtual_slots);
		});

		return entry.vtable.address_point();
	}

	const RBX::ICreator* ClassRegistry::creator_for(const RBX::Name* name) const
	{
		const auto it = m_creators.find(name);
		return it == m_creators.end() ? nullptr : it->second;
	}

	std::shared_ptr<void> ModInstanceCreator::create(RBX::EngineContext* context, RBX::CreatorRole) const
	{
		const auto& p = g_pointers->m_roblox_pointers;
		ConstructArgs args{&m_entry, {context}};

		return p.create_instance_impl(m_entry.descriptor->stable_id, m_entry.layout.size, m_entry.layout.align, memory_category_of(m_entry.descriptor),
		    &construct_mod_instance, &args);
	}

	bool ModInstanceCreator::is_serializable() const
	{
		return false;
	}

	bool ModInstanceCreator::is_script_creatable() const
	{
		return true;
	}
}

namespace rml::reflection
{
	ClassBuilder::ClassBuilder(std::string_view name, std::string_view base, const ClassLayout& layout) :
	    m_spec(std::make_unique<ClassSpec>(std::string(name), std::string(base), layout))
	{
	}

	ClassBuilder::~ClassBuilder() = default;
	ClassBuilder::ClassBuilder(ClassBuilder&&) noexcept = default;
	ClassBuilder& ClassBuilder::operator=(ClassBuilder&&) noexcept = default;

	ClassBuilder& ClassBuilder::property(std::string_view name, const PropertyType type, std::shared_ptr<void> accessor)
	{
		m_spec->properties.push_back(PropertySpec{std::string(name), "Data", type, std::move(accessor), {}});
		return *this;
	}

	ClassBuilder& ClassBuilder::function(std::string_view name, std::shared_ptr<FunctionInvoker> invoker)
	{
		m_spec->functions.push_back(FunctionSpec{std::string(name), std::move(invoker)});
		return *this;
	}

	ClassBuilder& ClassBuilder::event(std::string_view name, const std::ptrdiff_t member_offset, std::vector<EventArgument> arguments)
	{
		m_spec->events.push_back(EventSpec{std::string(name), member_offset, std::move(arguments)});
		return *this;
	}

	PropertyOptions ClassBuilder::last_property()
	{
		if (m_spec->properties.empty())
			throw std::logic_error("property options requested before any property was added");
		return PropertyOptions(m_spec->properties, m_spec->properties.size() - 1);
	}

	ClassBuilder& ClassBuilder::description(const std::string_view text)
	{
		m_spec->hints.description = std::string(text);
		return *this;
	}

	ClassBuilder& ClassBuilder::insert_category(const std::string_view name)
	{
		m_spec->hints.insert_category = std::string(name);
		return *this;
	}

	ClassBuilder& ClassBuilder::explorer_order(const int value)
	{
		m_spec->hints.explorer_order = value;
		return *this;
	}

	ClassBuilder& ClassBuilder::preferred_parent(const std::string_view class_name)
	{
		m_spec->hints.preferred_parent = std::string(class_name);
		return *this;
	}

	ClassBuilder& ClassBuilder::insertable(const bool value)
	{
		m_spec->hints.insertable = value;
		return *this;
	}

	ClassBuilder& ClassBuilder::browsable(const bool value)
	{
		m_spec->hints.browsable = value;
		return *this;
	}

	ClassBuilder& ClassBuilder::icon_of(const std::string_view engine_class)
	{
		m_spec->hints.icon_of = std::string(engine_class);
		return *this;
	}

	void fire_event(RBX::Instance* instance, const std::ptrdiff_t member_offset, const RBX::Reflection::EventArguments& arguments)
	{
		if (!instance)
			return;

		auto* entry = ClassRegistry::instance().class_of(instance);
		if (!entry)
			return;

		const auto it = entry->events_by_offset.find(member_offset);
		if (it == entry->events_by_offset.end())
			return;

		it->second->fire_event_generic(reinterpret_cast<RBX::Reflection::EventSource*>(instance), arguments);
	}

	const RBX::Reflection::ClassDescriptor* ClassBuilder::commit()
	{
		auto result = ClassRegistry::instance().define(*m_spec);
		if (!result)
			throw std::logic_error(result.error());
		return *result;
	}
}

namespace rml::reflection
{
	ExtensionBuilder::ExtensionBuilder(std::string_view class_name) :
	    m_spec(std::make_unique<ExtensionSpec>(std::string(class_name)))
	{
	}

	ExtensionBuilder::~ExtensionBuilder() = default;
	ExtensionBuilder::ExtensionBuilder(ExtensionBuilder&&) noexcept = default;
	ExtensionBuilder& ExtensionBuilder::operator=(ExtensionBuilder&&) noexcept = default;

	ExtensionBuilder& ExtensionBuilder::property(std::string_view name, const PropertyType type, std::shared_ptr<void> accessor)
	{
		m_spec->properties.push_back(PropertySpec{std::string(name), "Data", type, std::move(accessor), {}});
		return *this;
	}

	ExtensionBuilder& ExtensionBuilder::function(std::string_view name, std::shared_ptr<FunctionInvoker> invoker)
	{
		m_spec->functions.push_back(FunctionSpec{std::string(name), std::move(invoker)});
		return *this;
	}

	PropertyOptions ExtensionBuilder::last_property()
	{
		if (m_spec->properties.empty())
			throw std::logic_error("property options requested before any property was added");
		return PropertyOptions(m_spec->properties, m_spec->properties.size() - 1);
	}

	const RBX::Reflection::ClassDescriptor* ExtensionBuilder::commit()
	{
		auto result = ClassRegistry::instance().extend(*m_spec);
		if (!result)
			throw std::logic_error(result.error());
		return *result;
	}
}

namespace rml::reflection
{
	PropertyOptions::PropertyOptions(std::vector<PropertySpec>& properties, const std::size_t index) :
	    m_properties(&properties),
	    m_index(index)
	{
	}

	PropertySpec& PropertyOptions::spec() const
	{
		return (*m_properties)[m_index];
	}

	void PropertyOptions::category(const std::string_view name) const
	{
		spec().category = std::string(name);
	}

	void PropertyOptions::description(const std::string_view text) const
	{
		spec().hints.description = std::string(text);
	}

	void PropertyOptions::order(const int value) const
	{
		spec().hints.order = value;
	}

	void PropertyOptions::read_only() const
	{
		spec().hints.read_only = true;
	}

	void PropertyOptions::hidden() const
	{
		spec().hints.hidden = true;
	}

	void PropertyOptions::deprecated(const std::string_view message) const
	{
		spec().hints.deprecated = std::string(message);
	}

	void PropertyOptions::slider(const double min, const double max, const int ticks, const SliderScaling scaling) const
	{
		spec().hints.slider = Slider{min, max, ticks, scaling};
	}

	void PropertyOptions::enum_type(const void* key) const
	{
		spec().enum_key = key;
	}

	EnumBuilder::EnumBuilder(const std::string_view name, const void* key, const bool bind) :
	    m_spec(std::make_unique<EnumSpec>(EnumSpec{std::string(name), key, bind, {}, {}}))
	{
	}

	EnumBuilder::~EnumBuilder() = default;
	EnumBuilder::EnumBuilder(EnumBuilder&&) noexcept = default;
	EnumBuilder& EnumBuilder::operator=(EnumBuilder&&) noexcept = default;

	void EnumBuilder::description(const std::string_view text)
	{
		m_spec->hints.description = std::string(text);
	}

	void EnumBuilder::item(const std::string_view name, const int value)
	{
		m_spec->items.push_back(EnumItemSpec{std::string(name), value, {}});
	}

	void EnumBuilder::item_description(const std::string_view text)
	{
		if (!m_spec->items.empty())
			m_spec->items.back().hints.description = std::string(text);
	}

	void EnumBuilder::item_hidden()
	{
		if (!m_spec->items.empty())
			m_spec->items.back().hints.hidden = true;
	}

	void EnumBuilder::item_deprecated(const std::string_view message)
	{
		if (!m_spec->items.empty())
			m_spec->items.back().hints.deprecated = std::string(message);
	}

	const RBX::Reflection::EnumDescriptor* EnumBuilder::commit()
	{
		auto result = EnumRegistry::instance().commit(*m_spec);
		if (!result)
			throw std::logic_error(result.error());
		return *result;
	}
}
