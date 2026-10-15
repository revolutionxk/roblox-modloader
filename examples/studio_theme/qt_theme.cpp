#include "qt_theme.hpp"

#include <RobloxModLoader/hooking/hooking.hpp>
#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/qt/qt_integration.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <utility>

RML_LOG_SCOPE("StudioTheme")

namespace studio_theme
{
	static bool is_color_text(const std::string& text)
	{
		return (text.size() == 7 || text.size() == 9) && text.front() == '#'
		    && std::all_of(text.begin() + 1, text.end(), [](const unsigned char c) {
			       return std::isxdigit(c) != 0;
		       });
	}

	static void recolor_values(nlohmann::ordered_json& node, const Palette& palette)
	{
		if (node.is_string())
		{
			if (const auto& text = node.get_ref<const std::string&>(); is_color_text(text))
				node = palette.map_hex(text);
			return;
		}
		if (node.is_structured())
			for (auto& child : node)
				recolor_values(child, palette);
	}

	static void recolor_presets(nlohmann::ordered_json& presets, const Palette& palette)
	{
		if (!presets.is_object())
			return;
		for (auto& preset : presets)
		{
			if (!preset.is_object())
				continue;
			for (auto it = preset.begin(); it != preset.end(); ++it)
				if (it->is_string() && is_color_text(it->get_ref<const std::string&>()))
					*it = palette.map_preset_hex(it.key(), it->get_ref<const std::string&>());
		}
	}

	static std::string recolor_document(const std::string_view stock, const Palette& palette)
	{
		auto document = nlohmann::ordered_json::parse(stock);
		for (auto it = document.begin(); it != document.end(); ++it)
		{
			if (it.key() == "Presets")
				recolor_presets(*it, palette);
			else if (it.key() != "Name" && it.key() != "Paths")
				recolor_values(*it, palette);
		}
		return document.dump();
	}

	static std::string theme_name(const std::string_view stock)
	{
		const auto document = nlohmann::ordered_json::parse(stock, nullptr, false);
		if (document.is_discarded() || !document.is_object())
			return {};
		const auto name = document.find("Name");
		return name != document.end() && name->is_string() ? name->get<std::string>() : std::string{};
	}

	QtTheme::QtTheme(Engine engine, std::function<void(Base)> on_applied) :
	    m_engine(engine),
	    m_on_applied(std::move(on_applied))
	{
	}

	QtTheme::~QtTheme()
	{
		uninstall();
	}

	void QtTheme::install()
	{
		if (m_installed.load() || !m_engine.complete())
			return;

		s_instance.store(this, std::memory_order_release);
		rml::Hooking::DetourHookHelper::add<&QtTheme::parse_theme_json>("Theme::parseThemeJson", reinterpret_cast<void*>(m_engine.parse_theme_json));
		m_installed.store(true);
	}

	void QtTheme::uninstall()
	{
		if (!m_installed.exchange(false))
			return;

		s_instance.store(nullptr, std::memory_order_release);
		rml::Hooking::DetourHookHelper::remove<&QtTheme::parse_theme_json>();
	}

	void QtTheme::set_palette(std::shared_ptr<const Palette> palette)
	{
		std::scoped_lock lock{m_mutex};
		m_palette = std::move(palette);
	}

	bool QtTheme::available() const
	{
		std::scoped_lock lock{m_mutex};
		return m_installed.load() && m_parsed.size() >= 2;
	}

	std::string QtTheme::record(Theme* const theme, const std::string_view stock)
	{
		auto name = theme_name(stock);
		std::shared_ptr<const Palette> palette;
		{
			std::scoped_lock lock{m_mutex};
			if (const auto it = std::ranges::find(m_parsed, theme, &ParsedTheme::theme); it != m_parsed.end())
			{
				it->stock = std::string{stock};
				it->name = name;
			}
			else
			{
				m_parsed.push_back({theme, std::string{stock}, name});
			}
			palette = m_palette;
		}

		RML_INFO("Recorded Studio theme '{}' ({} bytes)", name, stock.size());
		if (!palette || name != base_name(palette->theme().base))
			return {};
		return recolor_document(stock, *palette);
	}

	void QtTheme::parse_theme_json(Theme* const self, const rml::qt::QByteArray& json)
	{
		const auto original = rml::Hooking::get_original<&QtTheme::parse_theme_json>();
		auto* const instance = s_instance.load(std::memory_order_acquire);
		if (!instance)
			return original(self, json);

		std::string document;
		try
		{
			document = instance->record(self, json.view());
		}
		catch (const std::exception& error)
		{
			RML_ERROR("Recoloring the theme failed, Studio keeps the stock colors: {}", error.what());
		}
		catch (...)
		{
			RML_ERROR("Recoloring the theme failed, Studio keeps the stock colors");
		}

		if (document.empty())
			return original(self, json);

		const rml::qt::QByteArray recolored{document};
		original(self, recolored);
	}

	void QtTheme::apply(const bool commit)
	{
		auto* const qt = rml::qt::QtIntegration::instance();
		if (!qt || !m_installed.load())
			return;

		std::shared_ptr<const Palette> palette;
		{
			std::scoped_lock lock{m_mutex};
			palette = m_palette;
		}

		qt->run_on_gui_thread([this, palette = std::move(palette), commit] {
			if (s_instance.load(std::memory_order_acquire) != this)
				return;
			try
			{
				apply_on_gui_thread(palette, commit);
			}
			catch (const std::exception& error)
			{
				RML_ERROR("Applying the Qt theme failed: {}", error.what());
			}
			catch (...)
			{
				RML_ERROR("Applying the Qt theme failed");
			}
		});
	}

	void QtTheme::apply_on_gui_thread(const std::shared_ptr<const Palette>& palette, const bool commit)
	{
		const Base base = palette ? palette->theme().base : m_last_base.load();
		const auto target_name = base_name(base);

		std::vector<ParsedTheme> parsed;
		{
			std::scoped_lock lock{m_mutex};
			parsed = m_parsed;
		}

		const auto target = std::ranges::find(parsed, target_name, &ParsedTheme::name);
		const auto other = std::ranges::find_if(parsed, [target_name](const ParsedTheme& entry) {
			return entry.name != target_name;
		});
		if (target == parsed.end() || other == parsed.end())
		{
			RML_WARN("The Qt theme was not applied: {} of the two Studio themes were recorded", parsed.size());
			return;
		}

		const auto original = rml::Hooking::get_original<&QtTheme::parse_theme_json>();
		for (const auto& entry : parsed)
		{
			const auto document = palette && entry.name == target_name ? recolor_document(entry.stock, *palette) : entry.stock;
			const rml::qt::QByteArray bytes{document};
			original(entry.theme, bytes);
		}

		auto* const settings = m_engine.core_studio_settings();
		m_engine.set_current_theme(settings, *other->theme, false, false);
		m_engine.set_current_theme(settings, *target->theme, commit, commit);
		m_last_base.store(base);

		RML_INFO("Applied {} to the Qt shell on {}", palette ? palette->theme().name : std::string{"the stock theme"}, target_name);
		if (m_on_applied)
			m_on_applied(base);
	}
}
