#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

namespace rml::qt
{
	class QPainterPrivate;
	class QPaintDevice;
	class QPixmap;
	class QRect;
	class QWidget;

	class RML_EXPORT QPainter
	{
	public:
		enum RenderHint
		{
			Antialiasing = 0x01,
			TextAntialiasing = 0x02,
			SmoothPixmapTransform = 0x04,
		};

		QPainterPrivate* d_ptr{};

		explicit QPainter(const QPaintDevice& target);
		~QPainter();

		QPainter(const QPainter&) = delete;
		QPainter& operator=(const QPainter&) = delete;

		void set_render_hint(RenderHint hint, bool on = true);
		void set_opacity(double opacity);
		void draw_pixmap(int x, int y, const QPixmap& pixmap);
		void draw_pixmap(const QRect& target, const QPixmap& pixmap);
	};

	RML_ASSERT_SIZE(QPainter, sizeof(void*));
}
