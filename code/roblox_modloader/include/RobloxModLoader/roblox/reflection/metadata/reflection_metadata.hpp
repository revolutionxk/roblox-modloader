#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/described.hpp"
#include "RobloxModLoader/roblox/reflection/reflected.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <string>

namespace RBX::Reflection
{
	class ClassDescriptor;
	class PropertyDescriptor;
	class FunctionDescriptor;
	class YieldFunctionDescriptor;
	class EventDescriptor;
	class CallbackDescriptor;
}

namespace RBX::Reflection::Metadata
{
	class Classes;
	class Enums;
	class Member;

	class Item : public Described<Item, Instance, "ReflectionMetadataItem">
	{
	public:
		Reflected<std::string, "ClassCategory"> class_category;
		Reflected<std::string, "Constraint"> constraint;
		Reflected<std::string, "EditorType"> editor_type;
		Reflected<std::string, "FFlag"> fflag;
		Reflected<std::string, "ScriptContext"> script_context;
		Reflected<std::string, "SliderScaling"> slider_scaling;
		Reflected<double, "UIMaximum"> ui_maximum;
		Reflected<double, "UIMinimum"> ui_minimum;
		Reflected<double, "UINumTicks"> ui_num_ticks;
		Reflected<int, "PropertyOrder"> property_order;
		Reflected<bool, "Browsable"> browsable;
		Reflected<bool, "ClientOnly"> client_only;
		Reflected<bool, "Deprecated"> deprecated;
		Reflected<bool, "EditingDisabled"> editing_disabled;
		Reflected<bool, "IsBackend"> backend;
		Reflected<bool, "ServerOnly"> server_only;
		std::string description;

	private:
		[[maybe_unused]] std::uint64_t reserved_198;

	public:
		[[nodiscard]] static bool is_deprecated(const Item* item, const Descriptor& descriptor)
		{
			if (item && item->deprecated)
				return true;
			return descriptor.is_deprecated;
		}

		[[nodiscard]] static bool is_backend(const Item* item, const Descriptor&)
		{
			return item && item->backend;
		}

		[[nodiscard]] static bool is_browsable(const Item* item, const Descriptor&)
		{
			return item && item->browsable;
		}
	};

	class RML_EXPORT Class : public Described<Class, Item, "ReflectionMetadataClass">
	{
	public:
		Reflected<std::string, "PreferredParent"> preferred_parent;
		Reflected<int, "ExplorerImageIndex"> explorer_image_index;
		Reflected<int, "ExplorerOrder"> explorer_order;
		Reflected<int, "ServiceVisibility"> service_visibility;
		Reflected<bool, "Insertable"> insertable;

		[[nodiscard]] const Class* get_base() const;
	};

	class Members : public Instance
	{
	};

	class Properties : public Described<Properties, Members, "ReflectionMetadataProperties">
	{
	};

	class Functions : public Described<Functions, Members, "ReflectionMetadataFunctions">
	{
	};

	class YieldFunctions : public Described<YieldFunctions, Members, "ReflectionMetadataYieldFunctions">
	{
	};

	class Events : public Described<Events, Members, "ReflectionMetadataEvents">
	{
	};

	class Callbacks : public Described<Callbacks, Members, "ReflectionMetadataCallbacks">
	{
	};

	class Member : public Described<Member, Item, "ReflectionMetadataMember">
	{
	};

	class RML_EXPORT Classes : public Described<Classes, Instance, "ReflectionMetadataClasses">
	{
	public:
		[[nodiscard]] const Class* get(const ClassDescriptor& descriptor, bool find_best_match) const;
		[[nodiscard]] Class* get(const ClassDescriptor& descriptor, bool find_best_match);
	};

	class Enums : public Described<Enums, Instance, "ReflectionMetadataEnums">
	{
	};

	class RML_EXPORT Reflection : public Described<Reflection, Instance, "ReflectionMetadata">
	{
	public:
		Classes* classes;
		Enums* enums;

		[[nodiscard]] static Reflection* singleton();
		[[nodiscard]] static Reflection* identify(void* candidate);

		[[nodiscard]] const Class* get(const ClassDescriptor& descriptor, bool find_best_match) const;
		[[nodiscard]] Class* get(const ClassDescriptor& descriptor, bool find_best_match);
		[[nodiscard]] const Member* get(const PropertyDescriptor& descriptor) const;
		[[nodiscard]] Member* get(const PropertyDescriptor& descriptor);
		[[nodiscard]] const Member* get(const FunctionDescriptor& descriptor) const;
		[[nodiscard]] Member* get(const FunctionDescriptor& descriptor);
		[[nodiscard]] const Member* get(const YieldFunctionDescriptor& descriptor) const;
		[[nodiscard]] Member* get(const YieldFunctionDescriptor& descriptor);
		[[nodiscard]] const Member* get(const EventDescriptor& descriptor) const;
		[[nodiscard]] Member* get(const EventDescriptor& descriptor);
		[[nodiscard]] const Member* get(const CallbackDescriptor& descriptor) const;
		[[nodiscard]] Member* get(const CallbackDescriptor& descriptor);
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if !defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(Reflection, classes, 0xC8);
	RML_ASSERT_OFFSET(Reflection, enums, 0xD0);
	RML_ASSERT_OFFSET(Item, class_category, 0xC8);
	RML_ASSERT_OFFSET(Item, constraint, 0xE0);
	RML_ASSERT_OFFSET(Item, editor_type, 0xF8);
	RML_ASSERT_OFFSET(Item, fflag, 0x110);
	RML_ASSERT_OFFSET(Item, script_context, 0x128);
	RML_ASSERT_OFFSET(Item, slider_scaling, 0x140);
	RML_ASSERT_OFFSET(Item, ui_maximum, 0x158);
	RML_ASSERT_OFFSET(Item, ui_minimum, 0x160);
	RML_ASSERT_OFFSET(Item, ui_num_ticks, 0x168);
	RML_ASSERT_OFFSET(Item, property_order, 0x170);
	RML_ASSERT_OFFSET(Item, browsable, 0x174);
	RML_ASSERT_OFFSET(Item, client_only, 0x175);
	RML_ASSERT_OFFSET(Item, deprecated, 0x176);
	RML_ASSERT_OFFSET(Item, editing_disabled, 0x177);
	RML_ASSERT_OFFSET(Item, backend, 0x178);
	RML_ASSERT_OFFSET(Item, server_only, 0x179);
	RML_ASSERT_OFFSET(Item, description, 0x180);
	RML_ASSERT_OFFSET(Class, preferred_parent, 0x1A0);
	RML_ASSERT_OFFSET(Class, explorer_image_index, 0x1B8);
	RML_ASSERT_OFFSET(Class, explorer_order, 0x1BC);
	RML_ASSERT_OFFSET(Class, insertable, 0x1C4);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
