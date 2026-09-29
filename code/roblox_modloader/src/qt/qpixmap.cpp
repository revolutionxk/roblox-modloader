#include "RobloxModLoader/qt/qpixmap.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/qt/qcolor.hpp"
#include "RobloxModLoader/qt/qimage.hpp"
#include "RobloxModLoader/qt/qsize.hpp"
#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"

#include <utility>

namespace rml::qt
{
	QPixmap::QPixmap() = default;

	QPixmap::QPixmap(const std::string_view file_path)
	{
		static const auto ctor = detail::gui<void (*)(QPixmap*, const QString*, const char*, int)>("QPixmap::QPixmap(QString const&, char const*, QFlags<Qt::ImageConversionFlag>)");
		if (!ctor)
			return;

		const QString path(file_path);
		ctor(this, &path, nullptr, 0);
	}

	QPixmap::QPixmap(const int width, const int height)
	{
		static const auto ctor = detail::gui<void (*)(QPixmap*, int, int)>("QPixmap::QPixmap(int, int)");
		if (ctor)
			ctor(this, width, height);
	}

	QPixmap::QPixmap(const QPixmap& other)
	{
		static const auto copy_ctor = detail::gui<void (*)(QPixmap*, const QPixmap*)>("QPixmap::QPixmap(QPixmap const&)");
		if (other.data && copy_ctor)
			copy_ctor(this, &other);
	}

	QPixmap::QPixmap(QPixmap&& other) noexcept :
	    QPixmap()
	{
		std::swap(data, other.data);
	}

	QPixmap& QPixmap::operator=(const QPixmap& other)
	{
		static const auto assign = detail::gui<QPixmap* (*)(QPixmap*, const QPixmap*)>("QPixmap::operator=(QPixmap const&)");
		if (this != &other && assign)
			assign(this, &other);
		return *this;
	}

	QPixmap& QPixmap::operator=(QPixmap&& other) noexcept
	{
		std::swap(data, other.data);
		return *this;
	}

	QPixmap::~QPixmap()
	{
		static const auto dtor = detail::gui<void (*)(QPixmap*)>("QPixmap::~QPixmap()");
		if (data && dtor)
			dtor(this);
	}

	int QPixmap::devType() const
	{
		static const auto fn = detail::gui<int (*)(const QPixmap*)>("QPixmap::devType() const");
		return fn ? fn(this) : 0;
	}

	QPaintEngine* QPixmap::paintEngine() const
	{
		static const auto fn = detail::gui<QPaintEngine* (*)(const QPixmap*)>("QPixmap::paintEngine() const");
		return fn ? fn(this) : nullptr;
	}

	int QPixmap::metric(const PaintDeviceMetric metric) const
	{
		static const auto fn = detail::gui<int (*)(const QPixmap*, PaintDeviceMetric)>("QPixmap::metric(QPaintDevice::PaintDeviceMetric) const");
		return fn ? fn(this, metric) : 0;
	}

	void QPixmap::initPainter(QPainter* painter) const
	{
		static const auto fn = detail::gui<void (*)(const QPaintDevice*, QPainter*)>("QPaintDevice::initPainter(QPainter*) const");
		if (fn)
			fn(this, painter);
	}

	QPaintDevice* QPixmap::redirected(QPoint* offset) const
	{
		static const auto fn = detail::gui<QPaintDevice* (*)(const QPaintDevice*, QPoint*)>("QPaintDevice::redirected(QPoint*) const");
		return fn ? fn(this, offset) : nullptr;
	}

	QPainter* QPixmap::sharedPainter() const
	{
		static const auto fn = detail::gui<QPainter* (*)(const QPaintDevice*)>("QPaintDevice::sharedPainter() const");
		return fn ? fn(this) : nullptr;
	}

	bool QPixmap::loaded() const
	{
		static const auto fn = detail::gui<bool (*)(const QPixmap*)>("QPixmap::isNull() const");
		return data && fn && !fn(this);
	}

	int QPixmap::width() const
	{
		static const auto fn = detail::gui<int (*)(const QPixmap*)>("QPixmap::width() const");
		return fn ? fn(this) : 0;
	}

	int QPixmap::height() const
	{
		static const auto fn = detail::gui<int (*)(const QPixmap*)>("QPixmap::height() const");
		return fn ? fn(this) : 0;
	}

	void QPixmap::fill(const QColor& color) const
	{
		static const auto fn = detail::gui<void (*)(const QPixmap*, const QColor*)>("QPixmap::fill(QColor const&)");
		if (fn)
			fn(this, &color);
	}

	bool QPixmap::save(const std::string_view file_path) const
	{
		static const auto fn = detail::gui<bool (*)(const QPixmap*, const QString*, const char*, int)>("QPixmap::save(QString const&, char const*, int) const");
		if (!fn)
			return false;
		const QString path(file_path);
		return fn(this, &path, nullptr, -1);
	}

	QPixmap QPixmap::scaled(const int width, const int height, const AspectMode aspect, const TransformMode transform) const
	{
		static void* const fn = detail::gui_export("QPixmap::scaled(QSize const&, Qt::AspectRatioMode, Qt::TransformationMode) const");

		QPixmap result;
		if (!fn || !loaded() || width <= 0 || height <= 0)
			return result;

		const QSize size(width, height);
		memory::call_returning_member(fn, result, this, &size, static_cast<int>(aspect), static_cast<int>(transform));
		return result;
	}

	QPixmap QPixmap::blurred(const double radius) const
	{
		static void* const to_image = detail::gui_export("QPixmap::toImage() const");
		static const auto blur = detail::widgets<void (*)(QImage* image, double radius, bool quality, int transposed)>("qt_blurImage(QImage&, double, bool, int)");
		static void* const from_image = detail::gui_export("QPixmap::fromImage(QImage const&, QFlags<Qt::ImageConversionFlag>)");

		QPixmap result;
		if (!to_image || !blur || !from_image || !loaded() || radius <= 0.0)
			return result;

		QImage image;
		memory::call_returning_member(to_image, image, this);
		blur(&image, radius, true, 0);
		memory::call_returning(from_image, result, static_cast<const QImage*>(&image), 0);
		return result;
	}
}
