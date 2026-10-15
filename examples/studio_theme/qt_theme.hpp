#pragma once

#include "engine.hpp"
#include "palette.hpp"

#include <RobloxModLoader/qt/qbytearray.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace studio_theme
{
	class QtTheme
	{
	public:
		QtTheme(Engine engine, std::function<void(Base)> on_applied);
		~QtTheme();

		QtTheme(const QtTheme&) = delete;
		QtTheme& operator=(const QtTheme&) = delete;

		void install();
		void uninstall();
		void set_palette(std::shared_ptr<const Palette> palette);
		void apply(bool commit);

		[[nodiscard]] bool available() const;

	private:
		struct ParsedTheme
		{
			Theme* theme{};
			std::string stock;
			std::string name;
		};

		static void parse_theme_json(Theme* self, const rml::qt::QByteArray& json);

		[[nodiscard]] std::string record(Theme* theme, std::string_view stock);
		void apply_on_gui_thread(const std::shared_ptr<const Palette>& palette, bool commit);

		Engine m_engine;
		std::function<void(Base)> m_on_applied;
		mutable std::mutex m_mutex;
		std::vector<ParsedTheme> m_parsed;
		std::shared_ptr<const Palette> m_palette;
		std::atomic<Base> m_last_base{Base::Dark};
		std::atomic<bool> m_installed{false};

		static inline std::atomic<QtTheme*> s_instance{nullptr};
	};
}
