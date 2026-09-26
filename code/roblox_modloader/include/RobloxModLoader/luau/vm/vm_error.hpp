#pragma once

#include "RobloxModLoader/internal/common.hpp"

namespace rml::luau::vm
{
	inline constexpr char kUnknownError[] = "unknown Luau error";

	struct VmError
	{
		enum class Kind : std::uint8_t
		{
			Syntax,
			Runtime,
			Memory,
			Unavailable,
			Internal
		};

		Kind kind{Kind::Internal};
		std::string message;
		std::string traceback;
		bool reported{false};

		[[nodiscard]] std::string describe() const
		{
			return traceback.empty() ? message : std::format("{}\n{}", message, traceback);
		}

		[[nodiscard]] static VmError unavailable(std::string what)
		{
			return {.kind = Kind::Unavailable, .message = std::move(what)};
		}

		[[nodiscard]] static VmError internal(std::string what)
		{
			return {.kind = Kind::Internal, .message = std::move(what)};
		}

		[[nodiscard]] static VmError syntax(std::string what)
		{
			return {.kind = Kind::Syntax, .message = std::move(what)};
		}
	};

	[[nodiscard]] std::expected<int, VmError> protected_call(lua_State* L, int nargs, int nresults) noexcept;

	[[nodiscard]] VmError error_from_stack(lua_State* L, VmError::Kind kind);

	[[nodiscard]] VmError::Kind kind_from_status(int status) noexcept;

	[[nodiscard]] std::string capture_traceback(lua_State* L);
}
