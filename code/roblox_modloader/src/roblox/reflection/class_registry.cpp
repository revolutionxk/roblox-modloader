#include "class_registry.hpp"

#include "mod_descriptors.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/memory/i_rtti_provider.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/roblox/reflection/described_creatable.hpp"
#include "RobloxModLoader/mod/init_context.hpp"
#include "RobloxModLoader/roblox/reflection/array_view.hpp"
#include "app/init_gate.hpp"
#include "pointers.hpp"

#include <cstring>

RML_LOG_SCOPE("ClassRegistry");

namespace rml::reflection
{
	static constexpr std::size_t k_class_descriptor_storage = 1024;
	static constexpr std::uint32_t k_protection_none = 0;
	static constexpr std::uint16_t k_functionality_persistent_local = 0x1 | 0x8 | 0x10;
	static constexpr std::size_t k_descriptor_field_offset = 0x18;
	static constexpr std::int32_t k_force_construction_tag = 6138;

	struct ClassAttributes
	{
		std::uint64_t descriptor_attributes[2]{};
		std::uint16_t functionality{k_functionality_persistent_local};
		std::uint8_t padding[6]{};
	};

	struct CreatedInstance
	{
		std::uintptr_t instance;
		std::uintptr_t control_block;
	};

	struct ConstructArgs
	{
		RegisteredClass* entry;
		RBX::ForceConstructionInCreatable force;
	};

	static const RBX::Reflection::ClassDescriptor* descriptor_of(const void* instance)
	{
		return *reinterpret_cast<const RBX::Reflection::ClassDescriptor* const*>(static_cast<const std::byte*>(instance) + k_descriptor_field_offset);
	}

	static void* construct_mod_instance(void* memory, const void* args)
	{
		const auto& [entry, force] = *static_cast<const ConstructArgs*>(args);
		g_pointers->m_roblox_pointers.instance_ctor(memory, &force, entry->name.c_str());
		entry->layout.construct(memory);

		auto& vptr = *static_cast<void***>(memory);
		vptr = ClassRegistry::instance().vtable_for(*entry, vptr);
		*reinterpret_cast<const RBX::Reflection::ClassDescriptor**>(static_cast<std::byte*>(memory) + k_descriptor_field_offset) = entry->descriptor;
		return memory;
	}

#if defined(RML_WINDOWS)
	static constexpr std::size_t k_first_merged_slot = 1;

	static void* mod_scalar_deleting_dtor(void* self, unsigned int flags)
	{
		auto* entry = ClassRegistry::instance().class_of(self);
		entry->layout.destroy(self);
		return reinterpret_cast<void* (*)(void*, unsigned int)>(entry->engine_vtable[0])(self, flags);
	}
#else
	static constexpr std::size_t k_first_merged_slot = 2;

	static void mod_complete_dtor(void* self)
	{
		auto* entry = ClassRegistry::instance().class_of(self);
		entry->layout.destroy(self);
		reinterpret_cast<void (*)(void*)>(entry->engine_vtable[0])(self);
	}

	static void mod_deleting_dtor(void* self)
	{
		auto* entry = ClassRegistry::instance().class_of(self);
		entry->layout.destroy(self);
		reinterpret_cast<void (*)(void*)>(entry->engine_vtable[1])(self);
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
		return p.class_descriptor_ctor && p.class_descriptor_all_classes && (p.creatable_get_creator || p.creatable_register_creator) && p.instance_ctor && p.create_instance_impl;
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
				counted += view.size();
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

			if (!g_rtti_provider)
				return std::unexpected("no RTTI provider");

			const auto vtable = g_rtti_provider->find_class_vtable("RBX::Instance");
			if (!vtable)
				return std::unexpected("RBX::Instance vtable not found");

			const memory::module image(platform::studio_image_name());
			const auto inside = [&](void* p) { return image.contains(memory::handle(p)); };
			const auto slots = engine_virtual_slots<RBX::Instance>::value();
			for (std::size_t slot = 0; slot < slots; ++slot)
			{
				if (!inside((*vtable)[slot]))
					return std::unexpected(std::format("RBX::Instance vtable slot {} is not code; the Instance mirror has more virtuals than the engine", slot));
			}
			if (inside((*vtable)[slots]))
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

		if (!g_rtti_provider)
			return std::unexpected("no RTTI provider; engine vtables are unreachable");

		const auto engine_vtable = g_rtti_provider->find_class_vtable("RBX::" + spec.base);
		if (!engine_vtable)
			return std::unexpected(std::format("no engine vtable for RBX::{}", spec.base));

		auto& entry = m_classes.emplace_back();
		entry.name = spec.name;
		entry.layout = spec.layout;
		entry.base = base;
		entry.engine_vtable = *engine_vtable;
		entry.storage = std::make_unique<std::byte[]>(k_class_descriptor_storage);
		std::memset(entry.storage.get(), 0, k_class_descriptor_storage);

		for (const auto& property : spec.properties)
		{
			auto member = make_property(entry.storage.get(), property.name, property.category, property.type, property.accessor.get());
			if (!member)
			{
				m_classes.pop_back();
				return std::unexpected(member.error());
			}

			entry.property_table.push_back(reinterpret_cast<const RBX::Reflection::PropertyDescriptor*>(member->storage.get()));
			entry.member_storage.push_back(std::move(member->storage));
			entry.accessors.push_back(property.accessor);
		}

		for (const auto& function : spec.functions)
		{
			auto member = make_function(entry.storage.get(), function.name, function.invoker.get());
			if (!member)
			{
				m_classes.pop_back();
				return std::unexpected(member.error());
			}

			entry.function_table.push_back(reinterpret_cast<const RBX::Reflection::FunctionDescriptor*>(member->storage.get()));
			entry.member_storage.push_back(std::move(member->storage));
			entry.invokers.push_back(function.invoker);
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

		static ClassAttributes attributes;
		const auto& p = g_pointers->m_roblox_pointers;
		p.class_descriptor_ctor(entry.storage.get(), base, entry.name.c_str(), 0, 0, false, false, &attributes, k_protection_none, nullptr,
		    RBX::ArrayView<const RBX::Reflection::PropertyDescriptor*>{entry.property_table},
		    RBX::ArrayView<const RBX::Reflection::EventDescriptor*>{entry.event_table},
		    RBX::ArrayView<const RBX::Reflection::FunctionDescriptor*>{entry.function_table},
		    RBX::ArrayView<const RBX::Reflection::YieldFunctionDescriptor*>{},
		    RBX::ArrayView<const RBX::Reflection::CallbackDescriptor*>{});

		entry.descriptor = reinterpret_cast<RBX::Reflection::ClassDescriptor*>(entry.storage.get());
		entry.creator = std::make_unique<ModInstanceCreator>(entry);
		m_by_descriptor[entry.descriptor] = &entry;
		// Windows files the creator in the engine's own map, as Folder::classDescriptor() does.
		if (p.creatable_register_creator)
			p.creatable_register_creator(entry.descriptor, entry.creator.get());
		else
			m_creators[&entry.descriptor->name] = entry.creator.get();

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

		auto& entry = m_extensions.emplace_back();
		entry.descriptor = descriptor;

		for (const auto& property : spec.properties)
		{
			auto member = make_property(descriptor, property.name, property.category, property.type, property.accessor.get());
			if (!member)
			{
				m_extensions.pop_back();
				return std::unexpected(member.error());
			}

			entry.property_table.push_back(reinterpret_cast<const RBX::Reflection::PropertyDescriptor*>(member->storage.get()));
			entry.member_storage.push_back(std::move(member->storage));
			entry.accessors.push_back(property.accessor);
		}

		for (const auto& function : spec.functions)
		{
			auto member = make_function(descriptor, function.name, function.invoker.get());
			if (!member)
			{
				m_extensions.pop_back();
				return std::unexpected(member.error());
			}

			entry.function_table.push_back(reinterpret_cast<const RBX::Reflection::FunctionDescriptor*>(member->storage.get()));
			entry.member_storage.push_back(std::move(member->storage));
			entry.invokers.push_back(function.invoker);
		}

		append_members(descriptor, entry.property_table);
		append_members(descriptor, entry.function_table);

		RML_INFO("Extended class {} with {} properties and {} functions", spec.name, entry.property_table.size(), entry.function_table.size());
		return descriptor;
	}

	RegisteredClass* ClassRegistry::class_of(const void* instance)
	{
		const auto it = m_by_descriptor.find(descriptor_of(instance));
		return it == m_by_descriptor.end() ? nullptr : it->second;
	}

	void** ClassRegistry::vtable_for(RegisteredClass& entry, void** derived_vtable)
	{
		std::call_once(entry.vtable_once, [&] {
			entry.vtable = std::make_unique<ClonedVtable>();
			std::memcpy(entry.vtable->data(), entry.engine_vtable - k_vtable_prefix_slots, sizeof(ClonedVtable));
			auto* slots = entry.vtable->data() + k_vtable_prefix_slots;

#if defined(RML_WINDOWS)
			slots[0] = reinterpret_cast<void*>(&mod_scalar_deleting_dtor);
#else
			slots[0] = reinterpret_cast<void*>(&mod_complete_dtor);
			slots[1] = reinterpret_cast<void*>(&mod_deleting_dtor);
#endif

			std::size_t merged = 0;
			for (std::size_t slot = k_first_merged_slot; slot < entry.layout.virtual_slots && slot < k_cloned_vtable_slots; ++slot)
			{
				if (derived_vtable[slot] == entry.layout.base_vtable[slot])
					continue;
				slots[slot] = derived_vtable[slot];
				++merged;
			}

			RML_INFO("Built vtable for {} from RBX::{} at 0x{:X}: {} of {} slots overridden", entry.name, entry.base->name.to_string(),
			    reinterpret_cast<std::uintptr_t>(entry.engine_vtable), merged, entry.layout.virtual_slots);
		});

		return entry.vtable->data() + k_vtable_prefix_slots;
	}

	const RBX::ICreator* ClassRegistry::creator_for(const RBX::Name* name) const
	{
		const auto it = m_creators.find(name);
		return it == m_creators.end() ? nullptr : it->second;
	}

	std::shared_ptr<void> ModInstanceCreator::create(RBX::EngineContext* context, RBX::CreatorRole) const
	{
		const auto& p = g_pointers->m_roblox_pointers;
		ConstructArgs args{&m_entry, {context, k_force_construction_tag}};

		CreatedInstance created{};
		memory::call_returning<CreatedInstance>(reinterpret_cast<void*>(p.create_instance_impl), created, m_entry.descriptor->stable_id, m_entry.layout.size,
		    m_entry.layout.align, memory_category_of(m_entry.descriptor), &construct_mod_instance, static_cast<const void*>(&args));

		std::shared_ptr<void> result;
		static_assert(sizeof(result) == sizeof(created));
		std::memcpy(&result, &created, sizeof(created));
		return result;
	}

	bool ModInstanceCreator::is_serializable() const
	{
		return false;
	}

	bool ModInstanceCreator::is_script_creatable() const
	{
		return true;
	}

	const RBX::Reflection::ClassDescriptor* ModInstanceCreator::descriptor() const
	{
		return m_entry.descriptor;
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

	ClassBuilder& ClassBuilder::property(std::string_view name, const PropertyType type, std::shared_ptr<void> accessor, std::string_view category)
	{
		m_spec->properties.push_back(PropertySpec{std::string(name), std::string(category), type, std::move(accessor)});
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

	ExtensionBuilder& ExtensionBuilder::property(std::string_view name, const PropertyType type, std::shared_ptr<void> accessor, std::string_view category)
	{
		m_spec->properties.push_back(PropertySpec{std::string(name), std::string(category), type, std::move(accessor)});
		return *this;
	}

	ExtensionBuilder& ExtensionBuilder::function(std::string_view name, std::shared_ptr<FunctionInvoker> invoker)
	{
		m_spec->functions.push_back(FunctionSpec{std::string(name), std::move(invoker)});
		return *this;
	}

	const RBX::Reflection::ClassDescriptor* ExtensionBuilder::commit()
	{
		auto result = ClassRegistry::instance().extend(*m_spec);
		if (!result)
			throw std::logic_error(result.error());
		return *result;
	}
}
