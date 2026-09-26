#include "instance_handles.hpp"

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/roblox/reflection/object.hpp"

RML_LOG_SCOPE("InstanceHandles");

namespace rml::dotnet
{
	InstanceHandles& instance_handles()
	{
		static auto* const handles = new InstanceHandles;
		return *handles;
	}

	static bool is_engine_owned(const RBX::Reflection::DescribedBase& object)
	{
		return object.get_descriptor().name.str == "DataModel";
	}

	std::uintptr_t InstanceHandles::retain(std::shared_ptr<RBX::Reflection::DescribedBase> object)
	{
		if (!object)
			return 0;

		const auto handle = reinterpret_cast<std::uintptr_t>(object.get());
		std::scoped_lock lock(m_mutex);
		auto& entry = m_entries[handle];
		if (!entry.strong && entry.count == 0 && !is_engine_owned(*object))
			entry.strong = std::move(object);
		++entry.count;
		return handle;
	}

	std::uintptr_t InstanceHandles::retain(RBX::Reflection::DescribedBase* object)
	{
		if (!object)
			return 0;
		return retain(object->weak_from_this().lock());
	}

	bool InstanceHandles::retain(const std::uintptr_t handle)
	{
		if (!handle)
			return false;

		{
			std::scoped_lock lock(m_mutex);
			if (const auto it = m_entries.find(handle); it != m_entries.end())
			{
				++it->second.count;
				return true;
			}
		}

		return retain(reinterpret_cast<RBX::Reflection::DescribedBase*>(handle)) != 0;
	}

	void InstanceHandles::release(const std::uintptr_t handle)
	{
		std::scoped_lock lock(m_mutex);
		const auto it = m_entries.find(handle);
		if (it == m_entries.end())
		{
			RML_WARN("Released an instance handle {:#x} that holds no reference", handle);
			return;
		}

		if (--it->second.count > 0)
			return;

		if (it->second.strong)
			m_released.push_back(std::move(it->second.strong));
		m_entries.erase(it);
	}

	bool InstanceHandles::has_released() const
	{
		std::scoped_lock lock(m_mutex);
		return !m_released.empty();
	}

	void InstanceHandles::collect()
	{
		std::vector<std::shared_ptr<RBX::Reflection::DescribedBase>> released;
		{
			std::scoped_lock lock(m_mutex);
			released.swap(m_released);
		}
		if (!released.empty())
			RML_DEBUG("Dropping {} instance references released by managed code", released.size());
	}
}
