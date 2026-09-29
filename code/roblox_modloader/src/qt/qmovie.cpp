#include "RobloxModLoader/qt/qmovie.hpp"

#include "RobloxModLoader/memory/foreign_call.hpp"
#include "RobloxModLoader/qt/qstring.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"
#include "qt_connect.hpp"

#include <utility>

namespace rml::qt
{
	QMovie* QMovie::create(const QString& file_name, QObject* parent)
	{
		static const auto construct = detail::gui<void (*)(void*, void*)>("QMovie::QMovie(QObject*)");
		auto* const movie = detail::heap_construct<QMovie>(sizeof(QMovie), construct, parent);
		if (!movie)
			return nullptr;

		static const auto set_file_name = detail::gui<void (*)(void*, const void*)>("QMovie::setFileName(QString const&)");
		if (set_file_name)
			set_file_name(movie, &file_name);

		return movie;
	}

	QtOwned<QMovie> QMovie::create_owned(const QString& file_name)
	{
		return QtOwned<QMovie>(create(file_name, nullptr));
	}

	void QMovie::destroy(QMovie* movie)
	{
		static const auto dtor = detail::gui<void (*)(void*)>("QMovie::~QMovie()");
		detail::heap_destroy(dtor, movie);
	}

	bool QMovie::isValid() const
	{
		static const auto fn = detail::gui<bool (*)(const void*)>("QMovie::isValid() const");
		return fn && fn(this);
	}

	int QMovie::frameCount() const
	{
		static const auto fn = detail::gui<int (*)(const void*)>("QMovie::frameCount() const");
		return fn ? fn(this) : 0;
	}

	int QMovie::currentFrameNumber() const
	{
		static const auto fn = detail::gui<int (*)(const void*)>("QMovie::currentFrameNumber() const");
		return fn ? fn(this) : 0;
	}

	int QMovie::nextFrameDelay() const
	{
		static const auto fn = detail::gui<int (*)(const void*)>("QMovie::nextFrameDelay() const");
		return fn ? fn(this) : 0;
	}

	QPixmap QMovie::currentPixmap() const
	{
		static void* const fn = detail::gui_export("QMovie::currentPixmap() const");

		QPixmap result;
		if (fn)
			memory::call_returning_member(fn, result, this);
		return result;
	}

	void QMovie::setCacheMode(const CacheMode mode)
	{
		static const auto fn = detail::gui<void (*)(void*, int)>("QMovie::setCacheMode(QMovie::CacheMode)");
		if (fn)
			fn(this, static_cast<int>(mode));
	}

	void QMovie::setSpeed(const int percent)
	{
		static const auto fn = detail::gui<void (*)(void*, int)>("QMovie::setSpeed(int)");
		if (fn)
			fn(this, percent);
	}

	void QMovie::start()
	{
		static const auto fn = detail::gui<void (*)(void*)>("QMovie::start()");
		if (fn)
			fn(this);
	}

	void QMovie::stop()
	{
		static const auto fn = detail::gui<void (*)(void*)>("QMovie::stop()");
		if (fn)
			fn(this);
	}

	void QMovie::setPaused(const bool paused)
	{
		static const auto fn = detail::gui<void (*)(void*, bool)>("QMovie::setPaused(bool)");
		if (fn)
			fn(this, paused);
	}

	bool QMovie::jumpToNextFrame()
	{
		static const auto fn = detail::gui<bool (*)(void*)>("QMovie::jumpToNextFrame()");
		return fn && fn(this);
	}

	void QMovie::on_frame_changed(std::function<void()> handler) const
	{
		static void* const signal = detail::gui_export("QMovie::frameChanged(int)");
		static const void* const meta = detail::gui_export("QMovie::staticMetaObject");
		detail::connect_function(this, signal, meta, [handler = std::move(handler)](void**) {
			if (handler)
				handler();
		});
	}
}
