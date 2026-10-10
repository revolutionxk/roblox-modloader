#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

inline constexpr std::uint32_t MICROPROFILE_NAME_MAX_LEN = 64;
inline constexpr std::uint32_t MICROPROFILE_MAX_CATEGORIES = 16;
inline constexpr std::uint32_t MICROPROFILE_MAX_GROUPS = 50;
inline constexpr std::uint32_t MICROPROFILE_MAX_TIMERS = 0xA80;
inline constexpr std::uint32_t MICROPROFILE_MAX_THREADS = 128;
inline constexpr std::uint32_t MICROPROFILE_STACK_MAX = 32;
inline constexpr std::uint32_t MICROPROFILE_MAX_COUNTERS = 0x1000;
inline constexpr std::uint32_t MICROPROFILE_MAX_COUNTER_NAME_CHARS = 0x40000;
inline constexpr std::uint32_t MICROPROFILE_CONTEXT_SWITCH_BUFFER_SIZE = 1;
inline constexpr std::uint64_t MICROPROFILE_INVALID_TICK = ~std::uint64_t{0};

enum class MicroProfileTimerToken : std::uint64_t
{
};

enum class MicroProfileLabelToken : std::uint64_t
{
};

enum MicroProfileTokenType
{
	MicroProfileTokenTypeCpu,
	MicroProfileTokenTypeGpu,
};

enum MicroProfileCounterFormat
{
	MICROPROFILE_COUNTER_FORMAT_DEFAULT,
	MICROPROFILE_COUNTER_FORMAT_BYTES,
};

using MicroProfileLogEntry = std::uint64_t;
using MicroProfileThreadIdType = unsigned long;

struct MicroProfileScriptScopeStats;

namespace RBX
{
	enum class ThreadBufferSizeType : std::uint32_t
	{
	};
}

[[nodiscard]] inline std::uint32_t MicroProfileGetTimerIndex(const MicroProfileTimerToken token)
{
	return static_cast<std::uint32_t>(static_cast<std::uint64_t>(token) & 0x3FFF);
}

[[nodiscard]] inline std::uint64_t MicroProfileGetGroupMask(const MicroProfileTimerToken token)
{
	return static_cast<std::uint64_t>(token) >> 14;
}

struct MicroProfileCategory
{
	char name[MICROPROFILE_NAME_MAX_LEN];
	std::uint64_t group_mask;

	RML_LAYOUT_GUARD_BEGIN()
	RML_ASSERT_OFFSET(MicroProfileCategory, group_mask, 0x40);
	RML_ASSERT_SIZE(MicroProfileCategory, 0x48);
	RML_LAYOUT_GUARD_END()
};

struct MicroProfileGroupInfo
{
	char name[MICROPROFILE_NAME_MAX_LEN];
	std::uint32_t name_len;
	std::uint32_t group_index;
	std::uint32_t num_timers;
	std::uint32_t max_timer_name_len;
	std::uint32_t color;
	std::uint32_t category;
	MicroProfileTokenType type;

	RML_LAYOUT_GUARD_BEGIN()
	RML_ASSERT_OFFSET(MicroProfileGroupInfo, name_len, 0x40);
	RML_ASSERT_OFFSET(MicroProfileGroupInfo, color, 0x50);
	RML_ASSERT_OFFSET(MicroProfileGroupInfo, type, 0x58);
	RML_ASSERT_SIZE(MicroProfileGroupInfo, 0x5C);
	RML_LAYOUT_GUARD_END()
};

struct MicroProfileTimerInfo
{
	MicroProfileTimerToken token;
	std::uint32_t timer_index;
	std::uint32_t group_index;
	char name[MICROPROFILE_NAME_MAX_LEN];
	std::uint32_t name_len;
	std::uint32_t color;
	std::uint8_t flags;

private:
	[[maybe_unused]] std::uint8_t reserved_59;

public:
	bool user_token;

	RML_LAYOUT_GUARD_BEGIN()
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, timer_index, 0x08);
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, group_index, 0x0C);
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, name, 0x10);
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, name_len, 0x50);
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, color, 0x54);
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, flags, 0x58);
	RML_ASSERT_OFFSET(MicroProfileTimerInfo, user_token, 0x5A);
	RML_ASSERT_SIZE(MicroProfileTimerInfo, 0x60);
	RML_LAYOUT_GUARD_END()
};

struct MicroProfileCounterInfo
{
	std::int32_t parent;
	std::int32_t sibling;
	std::int32_t first_child;
	std::uint16_t name_len;
	std::uint8_t level;
	const char* name;
	std::uint32_t flags;
	std::int64_t limit;
	MicroProfileCounterFormat format;

	RML_LAYOUT_GUARD_BEGIN()
	RML_ASSERT_OFFSET(MicroProfileCounterInfo, name_len, 0x0C);
	RML_ASSERT_OFFSET(MicroProfileCounterInfo, level, 0x0E);
	RML_ASSERT_OFFSET(MicroProfileCounterInfo, name, 0x10);
	RML_ASSERT_OFFSET(MicroProfileCounterInfo, flags, 0x18);
	RML_ASSERT_OFFSET(MicroProfileCounterInfo, limit, 0x20);
	RML_ASSERT_OFFSET(MicroProfileCounterInfo, format, 0x28);
	RML_ASSERT_SIZE(MicroProfileCounterInfo, 0x30);
	RML_LAYOUT_GUARD_END()
};

struct MicroProfileContextSwitch
{
	MicroProfileThreadIdType thread_out;
	MicroProfileThreadIdType thread_in;
	std::uint32_t process_in;
	std::int64_t cpu : 8;
	std::int64_t ticks : 56;

	RML_LAYOUT_GUARD_BEGIN()
#if defined(RML_WINDOWS)
	RML_ASSERT_SIZE(MicroProfileContextSwitch, 0x18);
#else
	RML_ASSERT_SIZE(MicroProfileContextSwitch, 0x20);
#endif
	RML_LAYOUT_GUARD_END()
};

struct MicroProfileThreadLog
{
	MicroProfileLogEntry* log;
	std::atomic<std::uint32_t> put;
	std::atomic<std::uint32_t> get;
	std::atomic_flag lock;
	MicroProfileLogEntry* log_gpu;
	std::atomic<std::uint32_t> put_gpu;
	std::uint32_t start_gpu;
	std::uint32_t gpu_active;
	void* gpu_context;
	std::uint32_t gpu;
	MicroProfileThreadIdType thread_id;
	std::uint32_t id;
	std::uint32_t log_index;
	std::uint32_t stack[MICROPROFILE_STACK_MAX];
	std::int64_t child_tick_stack[MICROPROFILE_STACK_MAX];
	std::uint32_t stack_pos;
	std::uint8_t group_stack_pos[MICROPROFILE_MAX_GROUPS];
	std::int64_t group_ticks[MICROPROFILE_MAX_GROUPS];
	std::int64_t aggregate_group_ticks[MICROPROFILE_MAX_GROUPS];
	char thread_name[64];
	std::uint32_t capacity;
	bool needs_lock;
	bool gpu_enabled;

	RML_LAYOUT_GUARD_BEGIN()
	RML_ASSERT_OFFSET(MicroProfileThreadLog, lock, 0x10);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, log_gpu, 0x18);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, put_gpu, 0x20);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, gpu_context, 0x30);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, gpu, 0x38);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(MicroProfileThreadLog, thread_id, 0x3C);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, id, 0x40);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, stack, 0x48);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, child_tick_stack, 0xC8);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, group_ticks, 0x200);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, thread_name, 0x520);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, capacity, 0x560);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, gpu_enabled, 0x565);
	RML_ASSERT_SIZE(MicroProfileThreadLog, 0x568);
#else
	RML_ASSERT_OFFSET(MicroProfileThreadLog, thread_id, 0x40);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, id, 0x48);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, stack, 0x50);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, child_tick_stack, 0xD0);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, group_ticks, 0x208);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, thread_name, 0x528);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, capacity, 0x568);
	RML_ASSERT_OFFSET(MicroProfileThreadLog, gpu_enabled, 0x56D);
	RML_ASSERT_SIZE(MicroProfileThreadLog, 0x570);
#endif
	RML_LAYOUT_GUARD_END()
};

struct MicroProfile
{
private:
	[[maybe_unused]] std::uint64_t reserved_0;

public:
	std::uint32_t total_timers;

private:
	[[maybe_unused]] std::uint32_t reserved_c;

public:
	std::uint32_t group_count;
	std::uint32_t category_count;

private:
	[[maybe_unused]] std::byte reserved_18[0x20];

public:
	std::uint64_t active_group;

private:
	[[maybe_unused]] std::uint32_t reserved_40;

public:
	std::uint32_t force_enable;

private:
	[[maybe_unused]] std::uint64_t reserved_48;

public:
	std::uint64_t force_enable_group;
	std::uint64_t force_disable_group;

private:
	[[maybe_unused]] std::uint64_t reserved_60;

public:
	std::uint64_t active_group_wanted;
	std::uint32_t all_groups_wanted;

private:
	[[maybe_unused]] std::uint32_t reserved_74;

public:
	std::uint32_t overflow;

private:
	[[maybe_unused]] std::uint32_t reserved_7c;

public:
	MicroProfileTimerToken overflow_token[2];

private:
	[[maybe_unused]] std::byte reserved_90[0x20];

public:
	std::uint64_t group_mask;
	std::uint64_t group_mask_gpu;
	std::uint32_t running;
	std::uint32_t toggle_running;
	std::uint32_t max_group_size;

private:
	[[maybe_unused]] std::byte reserved_cc[0x1F3C];

public:
	MicroProfileCategory category_info[MICROPROFILE_MAX_CATEGORIES];
	MicroProfileGroupInfo group_info[MICROPROFILE_MAX_GROUPS];
	MicroProfileTimerInfo timer_info[MICROPROFILE_MAX_TIMERS];
	std::uint8_t timer_to_group[MICROPROFILE_MAX_TIMERS];

private:
	[[maybe_unused]] std::byte reserved_43100[0x122B80];

public:
	MicroProfileThreadLog* pool[MICROPROFILE_MAX_THREADS];
	MicroProfileThreadLog pool_storage[MICROPROFILE_MAX_THREADS];

private:
	[[maybe_unused]] std::byte reserved_191880[0x50090];

public:
	MicroProfileContextSwitch context_switch[MICROPROFILE_CONTEXT_SWITCH_BUFFER_SIZE];

private:
	[[maybe_unused]] std::byte reserved_1e1930[0x4050];

public:
	char counter_names[MICROPROFILE_MAX_COUNTER_NAME_CHARS];
	MicroProfileCounterInfo counter_info[MICROPROFILE_MAX_COUNTERS];
	std::uint32_t num_counters;
	std::uint32_t counter_name_pos;
	std::atomic<std::int64_t> counters[MICROPROFILE_MAX_COUNTERS];
	std::uint32_t counter_history_put;

private:
	[[maybe_unused]] std::uint32_t reserved_25d98c;

public:
	std::int64_t counter_max[MICROPROFILE_MAX_COUNTERS];
	std::int64_t counter_min[MICROPROFILE_MAX_COUNTERS];

private:
	[[maybe_unused]] std::byte reserved_26d990[0x1290];

public:
	std::function<std::int64_t()> place_id_getter;
	std::function<std::string()> context_label_getter;
	std::function<MicroProfileScriptScopeStats()> script_scope_stats_getter;

private:
	[[maybe_unused]] bool reserved_26ec80;

	RML_LAYOUT_GUARD_BEGIN()
	RML_ASSERT_OFFSET(MicroProfile, total_timers, 0x08);
	RML_ASSERT_OFFSET(MicroProfile, group_count, 0x10);
	RML_ASSERT_OFFSET(MicroProfile, category_count, 0x14);
	RML_ASSERT_OFFSET(MicroProfile, active_group, 0x38);
	RML_ASSERT_OFFSET(MicroProfile, force_enable, 0x44);
	RML_ASSERT_OFFSET(MicroProfile, force_enable_group, 0x50);
	RML_ASSERT_OFFSET(MicroProfile, force_disable_group, 0x58);
	RML_ASSERT_OFFSET(MicroProfile, active_group_wanted, 0x68);
	RML_ASSERT_OFFSET(MicroProfile, all_groups_wanted, 0x70);
	RML_ASSERT_OFFSET(MicroProfile, overflow, 0x78);
	RML_ASSERT_OFFSET(MicroProfile, overflow_token, 0x80);
	RML_ASSERT_OFFSET(MicroProfile, group_mask, 0xB0);
	RML_ASSERT_OFFSET(MicroProfile, group_mask_gpu, 0xB8);
	RML_ASSERT_OFFSET(MicroProfile, running, 0xC0);
	RML_ASSERT_OFFSET(MicroProfile, toggle_running, 0xC4);
	RML_ASSERT_OFFSET(MicroProfile, max_group_size, 0xC8);
	RML_ASSERT_OFFSET(MicroProfile, category_info, 0x2008);
	RML_ASSERT_OFFSET(MicroProfile, group_info, 0x2488);
	RML_ASSERT_OFFSET(MicroProfile, timer_info, 0x3680);
	RML_ASSERT_OFFSET(MicroProfile, timer_to_group, 0x42680);
	RML_ASSERT_OFFSET(MicroProfile, pool, 0x165C80);
	RML_ASSERT_OFFSET(MicroProfile, pool_storage, 0x166080);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(MicroProfile, context_switch, 0x1E1510);
	RML_ASSERT_OFFSET(MicroProfile, counter_names, 0x1E5578);
	RML_ASSERT_OFFSET(MicroProfile, counter_info, 0x225578);
	RML_ASSERT_OFFSET(MicroProfile, num_counters, 0x255578);
	RML_ASSERT_OFFSET(MicroProfile, counter_name_pos, 0x25557C);
	RML_ASSERT_OFFSET(MicroProfile, counters, 0x255580);
	RML_ASSERT_OFFSET(MicroProfile, counter_history_put, 0x25D580);
	RML_ASSERT_OFFSET(MicroProfile, counter_max, 0x25D588);
	RML_ASSERT_OFFSET(MicroProfile, counter_min, 0x265588);
	RML_ASSERT_OFFSET(MicroProfile, place_id_getter, 0x26E818);
	RML_ASSERT_OFFSET(MicroProfile, context_label_getter, 0x26E858);
	RML_ASSERT_OFFSET(MicroProfile, script_scope_stats_getter, 0x26E898);
	RML_ASSERT_SIZE(MicroProfile, 0x26E8E0);
#else
	RML_ASSERT_OFFSET(MicroProfile, context_switch, 0x1E1910);
	RML_ASSERT_OFFSET(MicroProfile, counter_names, 0x1E5980);
	RML_ASSERT_OFFSET(MicroProfile, counter_info, 0x225980);
	RML_ASSERT_OFFSET(MicroProfile, num_counters, 0x255980);
	RML_ASSERT_OFFSET(MicroProfile, counter_name_pos, 0x255984);
	RML_ASSERT_OFFSET(MicroProfile, counters, 0x255988);
	RML_ASSERT_OFFSET(MicroProfile, counter_history_put, 0x25D988);
	RML_ASSERT_OFFSET(MicroProfile, counter_max, 0x25D990);
	RML_ASSERT_OFFSET(MicroProfile, counter_min, 0x265990);
	RML_ASSERT_OFFSET(MicroProfile, place_id_getter, 0x26EC20);
	RML_ASSERT_OFFSET(MicroProfile, context_label_getter, 0x26EC40);
	RML_ASSERT_OFFSET(MicroProfile, script_scope_stats_getter, 0x26EC60);
	RML_ASSERT_SIZE(MicroProfile, 0x26EC88);
#endif
	RML_LAYOUT_GUARD_END()
};
