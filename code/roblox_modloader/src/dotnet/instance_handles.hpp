#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RBX::Reflection
{
	class DescribedBase;
}

namespace rml::dotnet
{
	class InstanceHandles
	{
	public:
		[[nodiscard]] std::uintptr_t retain(std::shared_ptr<RBX::Reflection::DescribedBase> object);
		[[nodiscard]] std::uintptr_t retain(RBX::Reflection::DescribedBase* object);
		[[nodiscard]] bool retain(std::uintptr_t handle);
		void release(std::uintptr_t handle);

		[[nodiscard]] bool has_released() const;
		void collect();

	private:
		struct Entry
		{
			std::shared_ptr<RBX::Reflection::DescribedBase> strong;
			std::uint32_t count{};
		};

		mutable std::mutex m_mutex;
		std::unordered_map<std::uintptr_t, Entry> m_entries;
		std::vector<std::shared_ptr<RBX::Reflection::DescribedBase>> m_released;
	};

	[[nodiscard]] InstanceHandles& instance_handles();
}
