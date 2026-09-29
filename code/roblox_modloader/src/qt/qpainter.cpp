#include "RobloxModLoader/qt/qpainter.hpp"

#include "RobloxModLoader/qt/qpaintdevice.hpp"
#include "RobloxModLoader/qt/qpixmap.hpp"
#include "RobloxModLoader/qt/qpoint.hpp"
#include "RobloxModLoader/qt/qrect.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QPainter::QPainter(const QPaintDevice& target)
	{
		static const auto ctor = detail::gui<void (*)(QPainter*, QPaintDevice*)>("QPainter::QPainter(QPaintDevice*)");
		if (ctor)
			ctor(this, const_cast<QPaintDevice*>(&target));
	}

	QPainter::~QPainter()
	{
		static const auto dtor = detail::gui<void (*)(QPainter*)>("QPainter::~QPainter()");
		if (d_ptr && dtor)
			dtor(this);
	}

	void QPainter::set_render_hint(const RenderHint hint, const bool on)
	{
		static const auto fn = detail::gui<void (*)(QPainter*, int, bool)>("QPainter::setRenderHint(QPainter::RenderHint, bool)");
		if (fn && d_ptr)
			fn(this, static_cast<int>(hint), on);
	}

	void QPainter::set_opacity(const double opacity)
	{
		static const auto fn = detail::gui<void (*)(QPainter*, double)>("QPainter::setOpacity(double)");
		if (fn && d_ptr)
			fn(this, opacity);
	}

	void QPainter::draw_pixmap(const int x, const int y, const QPixmap& pixmap)
	{
		if (!d_ptr)
			return;

		static void* const inlined = detail::gui_export_optional("QPainter::drawPixmap(int, int, QPixmap const&)");
		if (inlined)
		{
			reinterpret_cast<void (*)(QPainter*, int, int, const QPixmap*)>(inlined)(this, x, y, &pixmap);
			return;
		}

		static const auto at_point = detail::gui<void (*)(QPainter*, const QPointF*, const QPixmap*)>("QPainter::drawPixmap(QPointF const&, QPixmap const&)");
		const QPointF point(x, y);
		if (at_point)
			at_point(this, &point, &pixmap);
	}

	void QPainter::draw_pixmap(const QRect& target, const QPixmap& pixmap)
	{
		if (!d_ptr)
			return;

		static void* const inlined = detail::gui_export_optional("QPainter::drawPixmap(QRect const&, QPixmap const&)");
		if (inlined)
		{
			reinterpret_cast<void (*)(QPainter*, const QRect*, const QPixmap*)>(inlined)(this, &target, &pixmap);
			return;
		}

		static const auto into_rect = detail::gui<void (*)(QPainter*, const QRectF*, const QPixmap*, const QRectF*)>("QPainter::drawPixmap(QRectF const&, QPixmap const&, QRectF const&)");
		const QRectF destination(target);
		const QRectF source(0.0, 0.0, pixmap.width(), pixmap.height());
		if (into_rect)
			into_rect(this, &destination, &pixmap, &source);
	}
}
