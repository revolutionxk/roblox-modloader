#include "RobloxModLoader/qt/qimage.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt
{
	QImage::QImage() = default;

	QImage::~QImage()
	{
		static const auto dtor = detail::gui<void (*)(QImage*)>("QImage::~QImage()");
		if (d && dtor)
			dtor(this);
	}

	int QImage::devType() const
	{
		static const auto fn = detail::gui<int (*)(const QImage*)>("QImage::devType() const");
		return fn ? fn(this) : 0;
	}

	QPaintEngine* QImage::paintEngine() const
	{
		static const auto fn = detail::gui<QPaintEngine* (*)(const QImage*)>("QImage::paintEngine() const");
		return fn ? fn(this) : nullptr;
	}

	int QImage::metric(const PaintDeviceMetric metric) const
	{
		static const auto fn = detail::gui<int (*)(const QImage*, PaintDeviceMetric)>("QImage::metric(QPaintDevice::PaintDeviceMetric) const");
		return fn ? fn(this, metric) : 0;
	}

	void QImage::initPainter(QPainter* painter) const
	{
		static const auto fn = detail::gui<void (*)(const QPaintDevice*, QPainter*)>("QPaintDevice::initPainter(QPainter*) const");
		if (fn)
			fn(this, painter);
	}

	QPaintDevice* QImage::redirected(QPoint* offset) const
	{
		static const auto fn = detail::gui<QPaintDevice* (*)(const QPaintDevice*, QPoint*)>("QPaintDevice::redirected(QPoint*) const");
		return fn ? fn(this, offset) : nullptr;
	}

	QPainter* QImage::sharedPainter() const
	{
		static const auto fn = detail::gui<QPainter* (*)(const QPaintDevice*)>("QPaintDevice::sharedPainter() const");
		return fn ? fn(this) : nullptr;
	}
}
