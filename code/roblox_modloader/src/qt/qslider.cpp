#include "RobloxModLoader/qt/qslider.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"
#include "qt_connect.hpp"

#include <utility>

namespace rml::qt
{
	QSlider* QSlider::create(const Orientation orientation, QWidget* parent)
	{
		static const auto construct = detail::widgets<void* (*)(void*, int, void*)>("QSlider::QSlider(Qt::Orientation, QWidget*)");
		return detail::heap_construct<QSlider>(sizeof(QSlider), construct, static_cast<int>(orientation), parent);
	}

	QtOwned<QSlider> QSlider::create_owned(const Orientation orientation)
	{
		return QtOwned<QSlider>(create(orientation, nullptr));
	}

	void QSlider::destroy(QSlider* slider)
	{
		static const auto dtor = detail::widgets<void (*)(void*)>("QSlider::~QSlider()");
		detail::heap_destroy(dtor, slider);
	}

	void QSlider::setRange(const int minimum, const int maximum)
	{
		static const auto fn = detail::widgets<void (*)(void*, int, int)>("QAbstractSlider::setRange(int, int)");
		if (fn)
			fn(this, minimum, maximum);
	}

	void QSlider::setValue(const int value)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QAbstractSlider::setValue(int)");
		if (fn)
			fn(this, value);
	}

	int QSlider::value() const
	{
		static const auto fn = detail::widgets<int (*)(const void*)>("QAbstractSlider::value() const");
		return fn ? fn(this) : 0;
	}

	void QSlider::setOrientation(const Orientation orientation)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QAbstractSlider::setOrientation(Qt::Orientation)");
		if (fn)
			fn(this, orientation);
	}

	void QSlider::setSingleStep(const int step)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QAbstractSlider::setSingleStep(int)");
		if (fn)
			fn(this, step);
	}

	void QSlider::setPageStep(const int step)
	{
		static const auto fn = detail::widgets<void (*)(void*, int)>("QAbstractSlider::setPageStep(int)");
		if (fn)
			fn(this, step);
	}

	void QSlider::setTracking(const bool enabled)
	{
		static const auto fn = detail::widgets<void (*)(void*, bool)>("QAbstractSlider::setTracking(bool)");
		if (fn)
			fn(this, enabled);
	}

	void QSlider::on_value_changed(std::function<void(int)> handler) const
	{
		static void* const signal = detail::widgets_export("QAbstractSlider::valueChanged(int)");
		static const void* const meta = detail::widgets_export("QAbstractSlider::staticMetaObject");
		detail::connect_function(this, signal, meta, [handler = std::move(handler)](void** args) {
			if (handler && args && args[1])
				handler(*static_cast<int*>(args[1]));
		});
	}
}
