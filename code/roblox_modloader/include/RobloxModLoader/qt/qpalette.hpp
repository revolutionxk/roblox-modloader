#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace rml::qt
{
	class QBrush;
	class QPalettePrivate;

	class RML_EXPORT QPalette
	{
	public:
		enum ColorRole
		{
			Base = 9,
		};

		QPalettePrivate* d{};
		std::uint32_t data{};

		QPalette() noexcept = default;
		explicit QPalette(const QPalette* source);
		QPalette(QPalette&& other) noexcept;
		QPalette& operator=(QPalette&& other) noexcept;
		QPalette(const QPalette&) = delete;
		QPalette& operator=(const QPalette&) = delete;
		~QPalette();

		void set_brush(ColorRole role, const QBrush& brush);

		[[nodiscard]] bool valid() const noexcept
		{
			return d != nullptr;
		}

	private:
		void destroy() noexcept;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QPalette, data, 8);
	RML_ASSERT_SIZE(QPalette, 16);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
