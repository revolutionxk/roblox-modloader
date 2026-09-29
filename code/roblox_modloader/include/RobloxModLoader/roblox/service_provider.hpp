#pragma once

#include "instance.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <map>
#include <memory>
#include <vector>

namespace RBX
{
	class Name;

	class ServiceProviderProp
	{
	};

	class RML_ENGINE_CLASS ServiceProvider : public Instance, public ServiceProviderProp
	{
	public:
		virtual bool can_find_service() const = 0;

		virtual bool can_create_service() const = 0;

		std::vector<std::shared_ptr<Instance>> services;
		std::map<const Name*, std::shared_ptr<Instance>> service_by_name;
		rbx::signal<void(std::shared_ptr<Instance>)> service_added_signal;
		rbx::signal<void(std::shared_ptr<Instance>)> service_removing_signal;
		rbx::signal<void()> close_signal;
		rbx::signal<void()> closing_signal;
	};

	template<typename Derived, typename Base>
	class Described : public Base
	{
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(ServiceProvider, services, 0xC8);
	RML_ASSERT_OFFSET(ServiceProvider, service_by_name, 0xE0);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(ServiceProvider, service_added_signal, 0xF0);
	RML_ASSERT_SIZE(ServiceProvider, 0x110);
#else
	RML_ASSERT_OFFSET(ServiceProvider, service_added_signal, 0xF8);
	RML_ASSERT_SIZE(ServiceProvider, 0x118);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
