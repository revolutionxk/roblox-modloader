#pragma once

#include "RobloxModLoader/roblox/rsl/arena.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>
#include <utility>

namespace RBX
{
	enum class SplitHashPolicy : std::uint32_t;

	namespace details
	{
		template<typename Key>
		struct DefaultHash;

		template<typename Key>
		struct DefaultEqualTo;

		template<typename Key, typename Value>
		struct GetMapKey;

		template<typename Key, typename Item, SplitHashPolicy Policy, unsigned InlineCapacity, typename Hash, typename Equal, typename GetKey>
		class SplitHashTable
		{
		public:
			std::uint32_t size;
			std::uint32_t capacity;
			std::uint32_t* table;
			Item* items;
			std::uint32_t index_mask;
			std::uint32_t table_bits;
			RSL::Arena* arena;
		};
	}

	template<typename Key, typename Value, SplitHashPolicy Policy>
	using SplitHashMap = details::SplitHashTable<Key, std::pair<Key, Value>, Policy, 0, details::DefaultHash<Key>, details::DefaultEqualTo<Key>, details::GetMapKey<Key, Value>>;

	namespace layout
	{
		using IntSplitHashMap = SplitHashMap<int, int, SplitHashPolicy{}>;
	}

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(layout::IntSplitHashMap, 0x28);
	RML_ASSERT_OFFSET(layout::IntSplitHashMap, index_mask, 0x18);
	RML_ASSERT_OFFSET(layout::IntSplitHashMap, arena, 0x20);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
