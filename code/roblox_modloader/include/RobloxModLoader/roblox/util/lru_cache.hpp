#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace RBX
{
	template<typename T>
	struct Hash
	{
		std::size_t operator()(const T& value) const noexcept;
	};

	template<typename Key, typename Value, typename Hasher = Hash<Key>>
	class LRUCache
	{
	public:
		using Entries = std::list<std::pair<Key, std::pair<std::size_t, Value>>>;

		virtual ~LRUCache();

		Entries entries;
		std::unordered_map<Key, typename Entries::iterator, Hasher> index;
		std::size_t size;
	};

	namespace layout
	{
		using SizeLRUCache = LRUCache<std::size_t, int>;
	}

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if !defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(layout::SizeLRUCache, entries, 0x8);
	RML_ASSERT_OFFSET(layout::SizeLRUCache, index, 0x20);
	RML_ASSERT_OFFSET(layout::SizeLRUCache, size, 0x48);
	RML_ASSERT_SIZE(layout::SizeLRUCache, 0x50);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
