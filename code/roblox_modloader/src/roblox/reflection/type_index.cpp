#include "type_index.hpp"

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "RobloxModLoader/util/hash.hpp"

#include <algorithm>
#include <atomic>
#include <format>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

RML_LOG_SCOPE("TypeIndex");

namespace rml::reflection
{
	using RBX::Reflection::Type;

	struct FrozenTypes
	{
		std::size_t source_size{};
		std::vector<std::pair<std::uint64_t, const Type*>> by_name;
		std::vector<const Type*> by_id;
	};

	static constexpr std::size_t k_max_types = 16384;

	static std::atomic<const FrozenTypes*> s_frozen{};
	static std::mutex s_freeze_mutex;
	static std::vector<std::unique_ptr<FrozenTypes>> s_generations;

	static std::span<const Type* const> engine_registry()
	{
		const auto registry = Type::get_all_types();
		if (registry.size() > k_max_types)
		{
			RML_ERROR("Type registry reports {} types; TYPE_REGISTRY probably resolved to the wrong address", registry.size());
			return {};
		}
		return registry;
	}

	static const FrozenTypes* freeze(const bool refresh)
	{
		const auto* current = s_frozen.load(std::memory_order_acquire);
		if (current && !refresh)
			return current;

		std::scoped_lock lock(s_freeze_mutex);
		current = s_frozen.load(std::memory_order_relaxed);
		const auto registry = engine_registry();
		if (registry.empty() || (current && current->source_size == registry.size()))
			return current;

		auto types = std::make_unique<FrozenTypes>();
		types->source_size = registry.size();
		for (const auto* type : registry)
		{
			if (!type)
				continue;
			types->by_name.emplace_back(utils::fnv1a_64(type->name.str), type);
			types->by_id.push_back(type);
		}
		std::ranges::stable_sort(types->by_name, {}, &std::pair<std::uint64_t, const Type*>::first);
		std::ranges::stable_sort(types->by_id, {}, [](const Type* type) { return type->type_id; });

		RML_DEBUG("Froze {} engine types", types->by_id.size());
		current = s_generations.emplace_back(std::move(types)).get();
		s_frozen.store(current, std::memory_order_release);
		return current;
	}

	static const Type* by_name(const FrozenTypes& types, const std::uint64_t hash)
	{
		const auto it = std::ranges::lower_bound(types.by_name, hash, {}, &std::pair<std::uint64_t, const Type*>::first);
		return it != types.by_name.end() && it->first == hash ? it->second : nullptr;
	}

	static const Type* by_id(const FrozenTypes& types, const int type_id)
	{
		const auto it = std::ranges::lower_bound(types.by_id, type_id, {}, [](const Type* type) { return type->type_id; });
		return it != types.by_id.end() && (*it)->type_id == type_id ? *it : nullptr;
	}

	template<class Key, class Lookup>
	static const Type* lookup(const Key key, const Lookup lookup_in)
	{
		const auto* types = freeze(false);
		if (!types)
			return nullptr;

		if (const auto* type = lookup_in(*types, key))
			return type;

		const auto* refreshed = freeze(true);
		return refreshed && refreshed != types ? lookup_in(*refreshed, key) : nullptr;
	}

	const Type* TypeIndex::find(const std::string_view name)
	{
		return lookup(utils::fnv1a_64(name), &by_name);
	}

	const Type* TypeIndex::find_by_id(const int type_id)
	{
		return lookup(type_id, &by_id);
	}

	const Type& TypeIndex::require(const std::string_view name)
	{
		if (const auto* type = find(name))
			return *type;
		throw std::runtime_error(std::format("Type::getSingleton<{}> is not registered yet", name));
	}
}
