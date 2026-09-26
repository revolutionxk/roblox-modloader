#pragma once

#include "RobloxModLoader/util/compile_time.hpp"
#include "handle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace rml::memory
{
	enum class anchor_step : std::uint8_t
	{
		call,
		call_from_end,
		caller,
		data_reference,
	};

	struct anchor_path
	{
		utils::capped_string<128> m_text;
		utils::capped_string<64> m_origin;
		std::array<anchor_step, 2> m_steps{};
		std::array<std::uint8_t, 2> m_indices{};
		std::uint8_t m_step_count{};

		[[nodiscard]] constexpr bool empty() const
		{
			return !*m_text.c_str() && !*m_origin.c_str();
		}

		[[nodiscard]] consteval anchor_path call(const std::uint8_t index) const
		{
			return then(anchor_step::call, index);
		}

		[[nodiscard]] consteval anchor_path call_from_end(const std::uint8_t index) const
		{
			return then(anchor_step::call_from_end, index);
		}

		[[nodiscard]] consteval anchor_path caller() const
		{
			return then(anchor_step::caller, 0);
		}

		[[nodiscard]] consteval anchor_path data_reference(const std::uint8_t index) const
		{
			return then(anchor_step::data_reference, index);
		}

		[[nodiscard]] consteval anchor_path then(const anchor_step step, const std::uint8_t index) const
		{
			auto next = *this;
			next.m_steps.at(m_step_count) = step;
			next.m_indices.at(m_step_count) = index;
			++next.m_step_count;
			return next;
		}
	};

	template<std::size_t N>
	consteval anchor_path referencing(const char (&text)[N])
	{
		anchor_path path;
		path.m_text = text;
		return path;
	}

	template<std::size_t N>
	consteval anchor_path from(const char (&name)[N])
	{
		anchor_path path;
		path.m_origin = name;
		return path;
	}

	std::expected<void*, std::string> follow(const anchor_path& path, void* origin);

	std::expected<void*, std::string> locate(const anchor_path& path);
}
