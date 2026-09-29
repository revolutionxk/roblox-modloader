#include "RobloxModLoader/qt/qtimer.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"
#include "qt_connect.hpp"

#include <utility>

namespace rml::qt
{
	QTimer* QTimer::create(QObject* parent)
	{
		static const auto construct = detail::core<void (*)(void*, void*)>("QTimer::QTimer(QObject*)");
		return detail::heap_construct<QTimer>(sizeof(QTimer), construct, parent);
	}

	QtOwned<QTimer> QTimer::create_owned()
	{
		return QtOwned<QTimer>(create(nullptr));
	}

	void QTimer::destroy(QTimer* timer)
	{
		static const auto dtor = detail::core<void (*)(void*)>("QTimer::~QTimer()");
		detail::heap_destroy(dtor, timer);
	}

	void QTimer::setInterval(const int milliseconds)
	{
		static const auto fn = detail::core<void (*)(void*, int)>("QTimer::setInterval(int)");
		if (fn)
			fn(this, milliseconds);
	}

	void QTimer::setSingleShot(const bool single_shot)
	{
		static const auto fn = detail::core<void (*)(void*, bool)>("QTimer::setSingleShot(bool)");
		if (fn)
			fn(this, single_shot);
	}

	void QTimer::start()
	{
		static const auto fn = detail::core<void (*)(void*)>("QTimer::start()");
		if (fn)
			fn(this);
	}

	void QTimer::stop()
	{
		static const auto fn = detail::core<void (*)(void*)>("QTimer::stop()");
		if (fn)
			fn(this);
	}

	void QTimer::on_timeout(std::function<void()> handler) const
	{
		static void* const signal = detail::core_export("QTimer::timeout(QTimer::QPrivateSignal)");
		static const void* const meta = detail::core_export("QTimer::staticMetaObject");
		detail::connect_function(this, signal, meta, [handler = std::move(handler)](void**) {
			if (handler)
				handler();
		});
	}
}
