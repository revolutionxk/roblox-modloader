#include "editor_overlay.hpp"

#include <RobloxModLoader/memory/vtable.hpp>
#include <RobloxModLoader/platform/memory/memory_protection.hpp>
#include <RobloxModLoader/qt/qapplication.hpp>
#include <RobloxModLoader/qt/qobject.hpp>
#include <RobloxModLoader/qt/qpainter.hpp>
#include <RobloxModLoader/qt/qrect.hpp>
#include <RobloxModLoader/qt/qstring.hpp>
#include <RobloxModLoader/qt/qwidget.hpp>
#include <RobloxModLoader/util/string.hpp>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace script_editor_bg
{
	EditorOverlay* g_active = nullptr;

	bool is_script_editor(const rml::qt::QObject* widget)
	{
		const std::string_view name = widget->class_name();
		return name.find("ScriptEditor") != std::string_view::npos;
	}

	std::pair<int, int> anchor(const Alignment alignment, const int w, const int h, const int vw, const int vh)
	{
		int x = (vw - w) / 2;
		int y = (vh - h) / 2;

		switch (alignment)
		{
		case Alignment::TopLeft:
			x = 0;
			y = 0;
			break;
		case Alignment::Top: y = 0; break;
		case Alignment::TopRight:
			x = vw - w;
			y = 0;
			break;
		case Alignment::Left: x = 0; break;
		case Alignment::Right: x = vw - w; break;
		case Alignment::BottomLeft:
			x = 0;
			y = vh - h;
			break;
		case Alignment::Bottom: y = vh - h; break;
		case Alignment::BottomRight:
			x = vw - w;
			y = vh - h;
			break;
		case Alignment::Center: break;
		}

		return {x, y};
	}

	EditorOverlay::EditorOverlay(std::filesystem::path mod_directory) :
	    m_mod_directory(std::move(mod_directory))
	{
		m_settings.store(std::make_shared<const BackgroundSettings>());
		g_active = this;
	}

	EditorOverlay::~EditorOverlay()
	{
		unhook_all();
		teardown_source();
		if (g_active == this)
			g_active = nullptr;
	}

	std::size_t EditorOverlay::paint_slot()
	{
		static const std::size_t slot = rml::memory::virtual_index(&rml::qt::QWidget::paintEvent);
		return slot;
	}

	void EditorOverlay::apply(const BackgroundSettings& settings)
	{
		m_settings.store(std::make_shared<const BackgroundSettings>(settings));
	}

	static bool has_valid_viewport(const rml::qt::QWidget* widget)
	{
		const rml::qt::QWidget* const viewport = widget->viewport();
		if (!viewport || viewport == widget)
			return false;

		for (const rml::qt::QWidget* known : rml::qt::QApplication::all_widgets())
			if (known == viewport)
				return true;

		return false;
	}

	void EditorOverlay::hook_open_editors()
	{
		for (rml::qt::QWidget* editor : rml::qt::QApplication::all_widgets())
			if (is_script_editor(editor) && has_valid_viewport(editor))
				hook_editor_class(editor->handle());
	}

	void EditorOverlay::repaint_editors()
	{
		for (rml::qt::QWidget* editor : rml::qt::QApplication::all_widgets())
			if (is_script_editor(editor))
				editor->update();
	}

	void EditorOverlay::hook_editor_class(void* editor_handle)
	{
		if (paint_slot() == static_cast<std::size_t>(-1))
			return;

		void** const vtable = rml::memory::vtable_of(editor_handle);
		if (m_originals.contains(vtable))
			return;

		void** const slot = vtable + paint_slot();
		const auto protection = rml::platform::set_protection(slot, sizeof(void*), rml::utils::MemoryProtection::ReadWrite);
		if (!protection)
			return;

		m_originals.emplace(vtable, reinterpret_cast<paint_fn>(*slot));
		*slot = reinterpret_cast<void*>(&paint_detour);
		rml::platform::restore_protection(slot, sizeof(void*), *protection);
	}

	void EditorOverlay::unhook_all()
	{
		for (const auto& [vtable, original] : m_originals)
		{
			void** const slot = vtable + paint_slot();
			const auto protection = rml::platform::set_protection(slot, sizeof(void*), rml::utils::MemoryProtection::ReadWrite);
			if (!protection)
				continue;
			*slot = reinterpret_cast<void*>(original);
			rml::platform::restore_protection(slot, sizeof(void*), *protection);
		}
		m_originals.clear();
	}

	std::size_t EditorOverlay::hooked_class_count() const
	{
		return m_originals.size();
	}

	void EditorOverlay::paint_detour(void* self, void* event)
	{
		EditorOverlay* const overlay = g_active;

		paint_fn original = nullptr;
		if (overlay)
			if (const auto it = overlay->m_originals.find(rml::memory::vtable_of(self)); it != overlay->m_originals.end())
				original = it->second;

		if (overlay)
			overlay->render(self);

		if (original)
			original(self, event);
	}

	bool EditorOverlay::is_animated_extension(const std::filesystem::path& path)
	{
		if (!path.has_extension())
			return false;

		const auto ext = rml::utils::to_lower(path.extension().string());
		return ext == ".gif" || ext == ".webp" || ext == ".apng" || ext == ".mng";
	}

	void EditorOverlay::teardown_source() const
	{
		if (m_movie)
		{
			m_movie->stop();
			m_movie.reset();
		}
		m_animated = false;
	}

	void EditorOverlay::on_movie_frame() const
	{
		if (!m_movie)
			return;

		m_frame = m_movie->currentPixmap();
		m_frame_dirty = true;
		repaint_editors();
	}

	void EditorOverlay::ensure_source(const BackgroundSettings& settings) const
	{
		std::filesystem::path image_path(settings.image);
		if (image_path.is_relative() && !settings.image.empty())
			image_path = m_mod_directory / image_path;

		const std::string key = image_path.generic_string();
		if (key == m_source_key)
			return;

		teardown_source();
		m_source_key = key;
		m_frame = rml::qt::QPixmap();
		m_display = rml::qt::QPixmap();
		m_display_blur = -1.0;
		m_frame_dirty = true;

		if (key.empty())
			return;

		if (is_animated_extension(image_path))
		{
			const rml::qt::QString qpath(key);
			if (rml::qt::QtOwned<rml::qt::QMovie> movie = rml::qt::QMovie::create_owned(qpath))
			{
				if (movie->isValid() && movie->frameCount() != 1)
				{
					movie->setCacheMode(rml::qt::QMovie::CacheMode::All);
					movie->on_frame_changed([this] {
						on_movie_frame();
					});
					movie->start();

					m_frame = movie->currentPixmap();
					m_animated = true;
					m_movie = std::move(movie);
					return;
				}
			}
		}

		m_animated = false;
		m_frame = rml::qt::QPixmap(key);
	}

	rml::qt::QPixmap EditorOverlay::blur_pixmap(const rml::qt::QPixmap& source, const double blur)
	{
		const int w = source.width();
		const int h = source.height();
		if (w <= 0 || h <= 0)
			return {};

		const double radius = clamp_blur(blur) * std::min(w, h) * 0.12;
		if (radius <= 0.0)
			return {};

		return source.blurred(radius);
	}

	void EditorOverlay::rebuild_display(const BackgroundSettings& settings) const
	{
		if (!m_frame.loaded())
		{
			m_display = rml::qt::QPixmap();
			return;
		}

		const double blur = clamp_blur(settings.blur);
		if (!m_frame_dirty && blur == m_display_blur && m_display.loaded())
			return;

		if (blur <= 0.0)
		{
			m_display = m_frame;
		}
		else
		{
			rml::qt::QPixmap blurred = blur_pixmap(m_frame, blur);
			m_display = blurred.loaded() ? std::move(blurred) : m_frame;
		}

		m_display_blur = blur;
		m_frame_dirty = false;
	}

	void EditorOverlay::render(void* editor) const
	{
		const std::shared_ptr<const BackgroundSettings> settings = m_settings.load();
		if (!settings || !settings->enabled)
			return;

		ensure_source(*settings);
		rebuild_display(*settings);

		if (!m_display.loaded())
			return;

		const auto* const widget = static_cast<rml::qt::QWidget*>(editor);
		const rml::qt::QWidget* const viewport = widget->viewport();
		if (!viewport)
			return;

		const int vw = viewport->width();
		const int vh = viewport->height();
		const int pw = m_display.width();
		const int ph = m_display.height();
		if (vw <= 0 || vh <= 0 || pw <= 0 || ph <= 0)
			return;

		rml::qt::QPainter painter(*viewport);
		painter.set_render_hint(rml::qt::QPainter::SmoothPixmapTransform, true);
		painter.set_opacity(settings->opacity);

		switch (settings->scale_mode)
		{
		case ScaleMode::Stretch:
		{
			painter.draw_pixmap(rml::qt::QRect(0, 0, vw, vh), m_display);
			break;
		}
		case ScaleMode::Tile:
		{
			for (int y = 0; y < vh; y += ph)
				for (int x = 0; x < vw; x += pw)
					painter.draw_pixmap(x, y, m_display);
			break;
		}
		case ScaleMode::Center:
		{
			const auto [x, y] = anchor(settings->alignment, pw, ph, vw, vh);
			painter.draw_pixmap(x, y, m_display);
			break;
		}
		case ScaleMode::Fit:
		case ScaleMode::Fill:
		{
			const double sx = static_cast<double>(vw) / pw;
			const double sy = static_cast<double>(vh) / ph;
			const double scale = settings->scale_mode == ScaleMode::Fit ? std::min(sx, sy) : std::max(sx, sy);
			const int tw = std::max(1, static_cast<int>(std::lround(pw * scale)));
			const int th = std::max(1, static_cast<int>(std::lround(ph * scale)));
			const auto [x, y] = anchor(settings->alignment, tw, th, vw, vh);
			painter.draw_pixmap(rml::qt::QRect(x, y, tw, th), m_display);
			break;
		}
		}
	}
}
