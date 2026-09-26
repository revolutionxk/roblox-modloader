#pragma once

#include "RobloxModLoader/internal/platform.hpp"

#include <cstddef>

#if defined(__clang__)
	#define RML_LAYOUT_DIAGNOSTIC_PUSH() \
		_Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"")
	#define RML_LAYOUT_DIAGNOSTIC_POP() _Pragma("clang diagnostic pop")
#else
	#define RML_LAYOUT_DIAGNOSTIC_PUSH()
	#define RML_LAYOUT_DIAGNOSTIC_POP()
#endif

#define RML_ASSERT_SIZE(type, size) \
	static_assert(sizeof(type) == (size), #type " size mismatch")

#define RML_ASSERT_OFFSET(type, field, offset) \
	static_assert(__builtin_offsetof(type, field) == (offset), #type "::" #field " offset mismatch")

// MSVC answers 0 for __builtin_offsetof on a reference member instead of where the engine keeps
// it, so a reference is pinned by its own offset on every other compiler and by the mirror's
// total size on MSVC.
#if defined(_MSC_VER) && !defined(__clang__)
	#define RML_ASSERT_REF_OFFSET(type, field, offset)
#else
	#define RML_ASSERT_REF_OFFSET(type, field, offset) RML_ASSERT_OFFSET(type, field, offset)
#endif

#if defined(RML_WINDOWS)
	#define RML_ASSERT_LAYOUT_SIZE(type, size) \
		static_assert(sizeof(type) == (size), #type " layout size mismatch")

	#define RML_ASSERT_LAYOUT_OFFSET(type, field, offset) \
		static_assert(offsetof(type, field) == (offset), #type "::" #field " layout offset mismatch")
#else
	#define RML_ASSERT_LAYOUT_SIZE(type, size)
	#define RML_ASSERT_LAYOUT_OFFSET(type, field, offset)
#endif

#define RML_LAYOUT_GUARD_BEGIN() \
	static void rml_assert_layout() noexcept \
	{

#define RML_LAYOUT_GUARD_END() \
	}
