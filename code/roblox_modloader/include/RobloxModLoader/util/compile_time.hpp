#pragma once

#include <cstddef>
#include <string_view>

namespace rml::utils
{
	template<std::size_t N>
	struct fixed_string
	{
		char str[N]{};

		constexpr fixed_string() = default;

		consteval fixed_string(const char (&literal)[N])
		{
			for (std::size_t i = 0; i < N; ++i)
				str[i] = literal[i];
		}

		[[nodiscard]] static constexpr std::size_t size()
		{
			return N - 1;
		}

		[[nodiscard]] constexpr std::string_view view() const
		{
			return {str, N - 1};
		}

		[[nodiscard]] constexpr const char* c_str() const
		{
			return str;
		}

		template<std::size_t M>
		[[nodiscard]] consteval fixed_string<N + M - 1> operator+(const fixed_string<M>& other) const
		{
			fixed_string<N + M - 1> result;
			for (std::size_t i = 0; i < N - 1; ++i)
				result.str[i] = str[i];
			for (std::size_t i = 0; i < M; ++i)
				result.str[N - 1 + i] = other.str[i];
			return result;
		}
	};

	template<std::size_t Capacity>
	struct capped_string
	{
		char str[Capacity]{};

		constexpr capped_string() = default;

		template<std::size_t N>
		consteval capped_string(const char (&literal)[N])
		{
			static_assert(N <= Capacity, "literal is longer than the capacity; raise it rather than truncating");

			for (std::size_t i = 0; i < N; ++i)
				str[i] = literal[i];
		}

		[[nodiscard]] constexpr const char* c_str() const
		{
			return str;
		}

		[[nodiscard]] constexpr std::string_view view() const
		{
			return str;
		}
	};
}
