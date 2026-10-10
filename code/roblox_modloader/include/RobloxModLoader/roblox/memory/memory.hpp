#pragma once

#include <cstddef>
#include <cstdint>

namespace RBX::Memory
{
	enum class MemoryType : std::uint8_t
	{
		Heap = 0,
		Gpu = 1,
	};

	enum class AllocatorType : std::uint8_t
	{
		Allocate = 0,
		Reallocate = 1,
		AllocateAligned = 2,
		ReallocateAligned = 3,
		Unknown = 4,
		ExternalAllocateHeap = 5,
		ExternalAllocateGPU = 6,
	};

	inline constexpr std::uint32_t max_categories = 1024;

	struct CategoryWord
	{
		std::uint32_t category : 10;
		std::uint32_t data_model_tag : 5;
		std::uint32_t script : 1;
		std::uint32_t reserved : 16;
	};

	static_assert(sizeof(CategoryWord) == sizeof(std::uint32_t));

	using GetCategoryCount = std::uint32_t (*)();
	using GetCategoryName = const char* (*)(std::uint32_t category);
	using GetCategoryTotal = std::size_t (*)(std::uint32_t category, MemoryType type);
	using TrackExternal = void (*)(std::uint32_t category, MemoryType type, std::uintptr_t id, std::size_t size);
}
