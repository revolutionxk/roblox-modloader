#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace rml::qt
{
	struct QArrayData
	{
		std::atomic<int> ref;
		int size;
		std::uint32_t alloc : 31;
		std::uint32_t capacityReserved : 1;
		std::ptrdiff_t offset;

		[[nodiscard]] const char* data() const noexcept
		{
			return reinterpret_cast<const char*>(this) + offset;
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QArrayData, size, 4);
	RML_ASSERT_OFFSET(QArrayData, offset, 16);
	RML_ASSERT_SIZE(QArrayData, 24);
	RML_LAYOUT_DIAGNOSTIC_POP()

	namespace detail
	{
		void release_array_data(QArrayData*& d, std::size_t element_size);
		void destroy_qstring(QArrayData*& d);
		void destroy_qbytearray(QArrayData*& d);
	}
}
