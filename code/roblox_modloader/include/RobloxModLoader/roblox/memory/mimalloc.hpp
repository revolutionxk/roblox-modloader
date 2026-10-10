#pragma once

#include <cstddef>
#include <cstdint>

using mi_category_visitor_fn = void (*)(std::uint32_t category_word, std::size_t live_bytes, void* arg);
using mi_category_visit_live_bytes_fast_fn = void (*)(mi_category_visitor_fn visitor, void* arg);
using mi_get_category_fn = std::uint32_t (*)();
using mi_resolve_category_fn = std::uint32_t (*)(const void* block);
using mi_usable_size_fn = std::size_t (*)(const void* block);
using mi_is_in_heap_region_fn = bool (*)(const void* block);
using mi_malloc_fn = void* (*)(std::size_t size);
using mi_malloc_aligned_fn = void* (*)(std::size_t size, std::size_t alignment);
using mi_realloc_fn = void* (*)(void* block, std::size_t size);
using mi_realloc_aligned_fn = void* (*)(void* block, std::size_t size, std::size_t alignment);
using mi_free_prepared_fn = void (*)(void* block, void* state);
using mi_free_fn = void (*)(void* block);
