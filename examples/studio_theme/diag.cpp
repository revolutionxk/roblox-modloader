#include <RobloxModLoader/internal/platform.hpp>
#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/memory/foreign_call.hpp>
#include <RobloxModLoader/qt/qapplication.hpp>
#include <RobloxModLoader/qt/qstring.hpp>
#include <RobloxModLoader/qt/qwidget.hpp>

#if !defined(RML_WINDOWS)
#include <dlfcn.h>
#endif

RML_LOG_SCOPE("StudioThemeDiag")

namespace studio_theme
{
#if defined(RML_WINDOWS)
	void dump_widgets()
	{
		RML_WARN("diag: widget dump is only available on macOS");
	}
#else
	struct DiagPoint
	{
		int x;
		int y;
	};

	static std::string qstring_member(const char* symbol, const void* self)
	{
		void* fn = dlsym(RTLD_DEFAULT, symbol);
		if (!fn)
			return "?";
		rml::qt::QString out;
		rml::memory::call_returning_member(fn, out, self);
		return out.to_utf8();
	}

	static std::string brush_name(const rml::qt::QWidget* widget, int role)
	{
		auto* palette_fn = reinterpret_cast<const void* (*)(const void*)>(dlsym(RTLD_DEFAULT, "_ZNK7QWidget7paletteEv"));
		auto* brush_fn = reinterpret_cast<const void* (*)(const void*, int, int)>(dlsym(RTLD_DEFAULT, "_ZNK8QPalette5brushENS_10ColorGroupENS_9ColorRoleE"));
		auto* color_fn = reinterpret_cast<const void* (*)(const void*)>(dlsym(RTLD_DEFAULT, "_ZNK6QBrush5colorEv"));
		void* name_fn = dlsym(RTLD_DEFAULT, "_ZNK6QColor4nameENS_10NameFormatE");
		if (!palette_fn || !brush_fn || !color_fn || !name_fn)
			return "?";
		const void* color = color_fn(brush_fn(palette_fn(widget), 0, role));
		rml::qt::QString out;
		rml::memory::call_returning_member(name_fn, out, color, 0);
		return out.to_utf8();
	}

	void dump_widgets()
	{
		auto* map_fn = reinterpret_cast<DiagPoint (*)(const void*, const DiagPoint*)>(dlsym(RTLD_DEFAULT, "_ZNK7QWidget11mapToGlobalERK6QPoint"));
		const auto widgets = rml::qt::QApplication::all_widgets();
		RML_INFO("diag: {} widgets", widgets.size());
		for (auto* widget : widgets)
		{
			const int w = widget->width();
			const int h = widget->height();
			if (w < 200 || h < 4)
				continue;
			const DiagPoint origin{0, 0};
			const DiagPoint global = map_fn ? map_fn(widget, &origin) : DiagPoint{-1, -1};
			auto sheet = qstring_member("_ZNK7QWidget10styleSheetEv", widget);
			if (sheet.size() > 220)
				sheet.resize(220);
			for (auto& c : sheet)
				if (c == '\n')
					c = ' ';
			RML_INFO("diag: {} '{}' {}x{} at {},{} window={} base={} button={} css=[{}]",
			    widget->class_name(),
			    qstring_member("_ZNK7QObject10objectNameEv", widget),
			    w,
			    h,
			    global.x,
			    global.y,
			    brush_name(widget, 10),
			    brush_name(widget, 9),
			    brush_name(widget, 1),
			    sheet);
		}
	}
#endif
}
