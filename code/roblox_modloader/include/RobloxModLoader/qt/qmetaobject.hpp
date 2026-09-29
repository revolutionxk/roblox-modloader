#pragma once

#include "RobloxModLoader/rml_export.hpp"

namespace rml::qt
{
	class RML_EXPORT QMetaObject
	{
	public:
		class RML_EXPORT Connection
		{
		public:
			void* d_ptr{};

			Connection() noexcept = default;
			~Connection();

			Connection(const Connection&) = delete;
			Connection& operator=(const Connection&) = delete;
		};

		[[nodiscard]] const char* className() const;
	};
}
