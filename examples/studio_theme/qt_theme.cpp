#include "qt_theme.hpp"

#include <RobloxModLoader/hooking/hooking.hpp>
#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/qt/qapplication.hpp>
#include <RobloxModLoader/qt/qstring.hpp>
#include <RobloxModLoader/qt/qt_integration.hpp>
#include <RobloxModLoader/qt/qwidget.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <unordered_map>
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

	using ColorTranslation = std::unordered_map<std::uint32_t, std::uint32_t>;
	using ColorVotes = std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, int>>;

	static std::optional<std::uint32_t> rgb_of(const nlohmann::ordered_json& node)
	{
		if (!node.is_string() || !is_color_text(node.get_ref<const std::string&>()))
			return std::nullopt;
		const auto color = parse_hex(node.get_ref<const std::string&>());
		return color ? std::optional<std::uint32_t>{rgb_key(*color)} : std::nullopt;
	}

	static void vote(const nlohmann::ordered_json& from, const nlohmann::ordered_json& to, ColorVotes& votes)
	{
		if (const auto source = rgb_of(from))
		{
			if (const auto target = rgb_of(to))
				++votes[*source][*target];
			return;
		}
		if (from.is_object() && to.is_object())
		{
			for (auto it = from.begin(); it != from.end(); ++it)
				if (const auto other = to.find(it.key()); other != to.end())
					vote(*it, *other, votes);
			return;
		}
		if (from.is_array() && to.is_array())
			for (std::size_t i = 0; i < std::min(from.size(), to.size()); ++i)
				vote(from[i], to[i], votes);
	}

	static void add_translation(ColorTranslation& translation, const std::string_view from, const std::string_view to)
	{
		const auto source = nlohmann::ordered_json::parse(from, nullptr, false);
		const auto target = nlohmann::ordered_json::parse(to, nullptr, false);
		if (source.is_discarded() || target.is_discarded())
			return;

		ColorVotes votes;
		vote(source, target, votes);
		for (const auto& [color, candidates] : votes)
		{
			const auto best = std::ranges::max_element(candidates, {}, &ColorVotes::mapped_type::value_type::second);
			if (best->first != color)
				translation.emplace(color, best->first);
		}
	}

	static std::optional<std::uint32_t> parse_hex_digits(const std::string_view digits)
	{
		std::uint32_t value{};
		const auto* const end = digits.data() + digits.size();
		if (const auto [last, error] = std::from_chars(digits.data(), end, value, 16); error != std::errc{} || last != end)
			return std::nullopt;
		return value;
	}

	static std::optional<int> parse_channel(std::string_view text)
	{
		while (!text.empty() && text.front() == ' ')
			text.remove_prefix(1);
		while (!text.empty() && text.back() == ' ')
			text.remove_suffix(1);
		int value{};
		const auto* const end = text.data() + text.size();
		if (const auto [last, error] = std::from_chars(text.data(), end, value); error != std::errc{} || last != end || value < 0 || value > 255)
			return std::nullopt;
		return value;
	}

	static bool translate_function(const std::string& css, std::size_t& at, const ColorTranslation& translation, std::string& out)
	{
		const auto open = css.find('(', at);
		const auto close = css.find(')', at);
		if (open == std::string::npos || close == std::string::npos || close < open)
			return false;

		const std::string_view inside{css.data() + open + 1, close - open - 1};
		std::array<std::string_view, 3> parts{};
		std::size_t start = 0;
		for (auto& part : parts)
		{
			const auto comma = inside.find(',', start);
			part = inside.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
			start = comma == std::string_view::npos ? inside.size() : comma + 1;
		}

		const auto r = parse_channel(parts[0]);
		const auto g = parse_channel(parts[1]);
		const auto b = parse_channel(parts[2]);
		if (!r || !g || !b)
			return false;

		const auto it = translation.find((static_cast<std::uint32_t>(*r) << 16) | (static_cast<std::uint32_t>(*g) << 8) | static_cast<std::uint32_t>(*b));
		if (it == translation.end())
			return false;

		out.append(css, at, open + 1 - at);
		out += std::format("{}, {}, {}", (it->second >> 16) & 0xFF, (it->second >> 8) & 0xFF, it->second & 0xFF);
		out.append(inside.substr(start < inside.size() ? start - 1 : inside.size()));
		out += ')';
		at = close + 1;
		return true;
	}

	static std::string translate_css(const std::string& css, const ColorTranslation& translation)
	{
		std::string out;
		out.reserve(css.size());
		std::size_t at = 0;
		while (at < css.size())
		{
			if (css[at] == '#')
			{
				std::size_t digits = 0;
				while (at + 1 + digits < css.size() && digits < 9 && std::isxdigit(static_cast<unsigned char>(css[at + 1 + digits])))
					++digits;
				if (digits == 6 || digits == 8)
				{
					if (const auto value = parse_hex_digits(std::string_view{css}.substr(at + 1, digits)))
					{
						if (const auto it = translation.find(*value & 0xFFFFFF); it != translation.end())
						{
							out += digits == 8 ? std::format("#{:02x}{:06x}", *value >> 24, it->second) : std::format("#{:06x}", it->second);
							at += 1 + digits;
							continue;
						}
					}
				}
			}
			else if ((css.compare(at, 4, "rgb(") == 0 || css.compare(at, 5, "rgba(") == 0) && translate_function(css, at, translation, out))
			{
				continue;
			}
			out += css[at++];
		}
		return out;
	}

	static std::size_t retheme_widgets(const ColorTranslation& translation)
	{
		if (translation.empty())
			return 0;

		std::size_t changed = 0;
		for (auto* widget : rml::qt::QApplication::all_widgets())
		{
			const auto css = widget->styleSheet().to_utf8();
			if (css.empty())
				continue;
			const auto translated = translate_css(css, translation);
			if (translated == css)
				continue;
			widget->setStyleSheet(rml::qt::QString{std::string_view{translated}});
			++changed;
		}
		return changed;
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
		auto document = palette && name == base_name(palette->theme().base) ? recolor_document(stock, *palette) : std::string{};
		{
			std::scoped_lock lock{m_mutex};
			if (const auto it = std::ranges::find(m_parsed, theme, &ParsedTheme::theme); it != m_parsed.end())
				it->applied = document.empty() ? std::string{stock} : document;
		}
		return document;
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

		const auto displayed = std::ranges::find(parsed, base_name(m_last_base.load()), &ParsedTheme::name);
		const auto previous_applied = displayed != parsed.end() ? displayed->applied : std::string{};
		const auto previous_stock = displayed != parsed.end() ? displayed->stock : std::string{};

		const auto original = rml::Hooking::get_original<&QtTheme::parse_theme_json>();
		std::string target_document;
		for (auto& entry : parsed)
		{
			auto document = palette && entry.name == target_name ? recolor_document(entry.stock, *palette) : entry.stock;
			const rml::qt::QByteArray bytes{document};
			original(entry.theme, bytes);
			if (entry.name == target_name)
				target_document = document;
			entry.applied = std::move(document);
		}

		{
			std::scoped_lock lock{m_mutex};
			for (const auto& entry : parsed)
				if (const auto it = std::ranges::find(m_parsed, entry.theme, &ParsedTheme::theme); it != m_parsed.end())
					it->applied = entry.applied;
		}

		ColorTranslation translation;
		add_translation(translation, previous_applied, target_document);
		add_translation(translation, previous_stock, target_document);
		const auto rethemed = retheme_widgets(translation);

		auto* const settings = m_engine.core_studio_settings();
		m_engine.set_current_theme(settings, *other->theme, false, false);
		m_engine.set_current_theme(settings, *target->theme, commit, commit);
		m_last_base.store(base);

		RML_INFO("Applied {} to the Qt shell on {} ({} widget stylesheets translated)", palette ? palette->theme().name : std::string{"the stock theme"}, target_name, rethemed);
		if (m_on_applied)
			m_on_applied(base);
	}
}
