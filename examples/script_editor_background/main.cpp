#include "RobloxModLoader/mod/mod_base.hpp"
#include "editor_overlay.hpp"
#include "settings.hpp"

#include <RobloxModLoader/config/mod_settings.hpp>
#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/qt/qcheckbox.hpp>
#include <RobloxModLoader/qt/qdialog.hpp>
#include <RobloxModLoader/qt/qfiledialog.hpp>
#include <RobloxModLoader/qt/qlabel.hpp>
#include <RobloxModLoader/qt/qpushbutton.hpp>
#include <RobloxModLoader/qt/qslider.hpp>
#include <RobloxModLoader/qt/qstring.hpp>
#include <RobloxModLoader/qt/qt_integration.hpp>
#include <RobloxModLoader/util/shell.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>

#if defined(RML_WINDOWS)
	#include <windows.h>
#endif

using namespace script_editor_bg;

namespace
{
	[[nodiscard]] int to_percent(const double opacity)
	{
		return static_cast<int>(std::lround(opacity * 100.0));
	}

	[[nodiscard]] std::string scale_label(const ScaleMode mode)
	{
		return "Scale: " + std::string(to_string(mode));
	}

	[[nodiscard]] std::string align_label(const Alignment alignment)
	{
		return "Align: " + std::string(to_string(alignment));
	}
}

class ScriptEditorBackground final : public ModBase
{
public:
	ScriptEditorBackground()
	{
		name = "Script Editor Background";
		version = "3.0.0";
		author = "RobloxModLoader";
		description = "Paints a configurable, dimmed background image behind the Studio script editor.";
		m_log = rml::Logger::get_logger("ScriptEditorBg");
	}

	void on_load() override
	{
		m_store = std::make_unique<rml::config::ModSettings>(paths().config_file("config.toml"));
		m_store->write_default_if_missing(default_config_template());
		m_store->load();

		{
			std::scoped_lock lock(m_mutex);
			m_settings = read(*m_store);
		}

		m_overlay = std::make_unique<EditorOverlay>(mod_folder());
		m_overlay->apply(m_settings);

		if (rml::qt::QtIntegration* const qt = rml::qt::QtIntegration::instance())
			qt->menu().add_action("Script Editor Background", [this] {
				open_panel();
			});
		else
			m_log->warn("Qt integration unavailable; edit config.toml to configure this mod");

		m_store->watch([this] {
			std::scoped_lock lock(m_mutex);
			m_settings = read(*m_store);
			m_overlay->apply(m_settings);
			m_log->info("config.toml reloaded (enabled={}, opacity={}%, scale={}, align={})",
			    m_settings.enabled,
			    to_percent(m_settings.opacity),
			    to_string(m_settings.scale_mode),
			    to_string(m_settings.alignment));
		});

		s_active.store(this, std::memory_order_release);
		m_running.store(true, std::memory_order_release);
		m_scanner = std::thread([this] {
			while (m_running.load(std::memory_order_acquire))
			{
				if (rml::qt::QtIntegration* const qt = rml::qt::QtIntegration::instance())
					qt->run_on_gui_thread([this] {
						run_scan_on_gui();
					});
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		});

		m_log->info("loaded - mod folder '{}'", mod_folder().string());
	}

	void on_unload() override
	{
		m_running.store(false, std::memory_order_release);
		if (m_scanner.joinable())
			m_scanner.join();

		s_active.store(nullptr, std::memory_order_release);

		if (m_store)
			m_store->stop_watching();
		if (m_overlay)
			m_overlay->unhook_all();
		m_overlay.reset();
		m_log->info("unloaded");
	}

private:
	template<typename Mutator>
	void edit_settings(Mutator&& mutator)
	{
		std::scoped_lock lock(m_mutex);
		std::forward<Mutator>(mutator)(m_settings);
		write(*m_store, m_settings);
		if (!m_store->save())
			m_log->warn("failed to write {}", m_store->path().string());
		m_overlay->apply(m_settings);
		m_overlay->repaint_editors();
	}

	[[nodiscard]] BackgroundSettings snapshot() const
	{
		std::scoped_lock lock(m_mutex);
		return m_settings;
	}

	void run_scan_on_gui()
	{
		if (m_overlay)
			m_overlay->hook_open_editors();
	}


	void open_panel()
	{
		m_overlay->hook_open_editors();

		const BackgroundSettings snap = snapshot();

		rml::qt::QtOwned<rml::qt::QDialog> dialog = rml::qt::QDialog::create_owned();
		if (!dialog)
		{
			m_log->warn("could not create settings dialog; edit config.toml instead ('{}')", m_store->path().string());
			return;
		}

		dialog->setWindowTitle("Script Editor Background");
		dialog->setModal(true);
		dialog->resize(380, 378);
		dialog->setStyleSheet("QDialog { background-color:#1e1f22; }"
		                      "QLabel { color:#e6e6e6; font-family:Segoe UI; font-size:13px; }"
		                      "QCheckBox { color:#e6e6e6; font-family:Segoe UI; font-size:13px; }"
		                      "QPushButton { color:#e6e6e6; background-color:#2b2d31; border:1px solid #3a3d42;"
		                      " border-radius:6px; padding:6px; font-family:Segoe UI; font-weight:600; }"
		                      "QPushButton:hover { background-color:#35373c; }"
		                      "QSlider::groove:horizontal { height:6px; background:#3a3d42; border-radius:3px; }"
		                      "QSlider::handle:horizontal { width:16px; margin:-6px 0; border-radius:8px; background:#5b9bff; }"
		                      "QSlider::sub-page:horizontal { background:#5b9bff; border-radius:3px; }");

		if (rml::qt::QLabel* const title = rml::qt::QLabel::create("Script Editor Background", dialog.get()))
		{
			title->setStyleSheet("font-size:15px; font-weight:600;");
			title->setGeometry(20, 14, 340, 22);
		}

		rml::qt::QCheckBox* const enabled = rml::qt::QCheckBox::create("Enabled", dialog.get());
		if (enabled)
		{
			enabled->setChecked(snap.enabled);
			enabled->setGeometry(20, 48, 200, 24);
			enabled->on_toggled([this](const bool on) {
				edit_settings([on](BackgroundSettings& s) {
					s.enabled = on;
				});
			});
		}

		if (rml::qt::QLabel* const opacity_caption = rml::qt::QLabel::create("Opacity", dialog.get()))
			opacity_caption->setGeometry(20, 84, 120, 22);

		rml::qt::QLabel* const opacity_value =
		    rml::qt::QLabel::create(std::to_string(to_percent(snap.opacity)) + "%", dialog.get());
		if (opacity_value)
		{
			opacity_value->setAlignment(rml::qt::QLabel::AlignRight | rml::qt::QLabel::AlignVCenter);
			opacity_value->setGeometry(260, 84, 100, 22);
		}

		rml::qt::QSlider* const slider = rml::qt::QSlider::create(rml::qt::QSlider::Horizontal, dialog.get());
		if (slider)
		{
			slider->setRange(0, 100);
			slider->setValue(to_percent(snap.opacity));
			slider->setSingleStep(1);
			slider->setPageStep(10);
			slider->setGeometry(20, 112, 340, 24);
			slider->on_value_changed([this, opacity_value](const int percent) {
				if (opacity_value)
					opacity_value->setText(std::to_string(percent) + "%");
				edit_settings([percent](BackgroundSettings& s) {
					s.opacity = clamp_opacity(percent / 100.0);
				});
			});
		}

		if (rml::qt::QLabel* const blur_caption = rml::qt::QLabel::create("Blur", dialog.get()))
			blur_caption->setGeometry(20, 150, 120, 22);

		rml::qt::QLabel* const blur_value = rml::qt::QLabel::create(std::to_string(to_percent(snap.blur)) + "%", dialog.get());
		if (blur_value)
		{
			blur_value->setAlignment(rml::qt::QLabel::AlignRight | rml::qt::QLabel::AlignVCenter);
			blur_value->setGeometry(260, 150, 100, 22);
		}

		rml::qt::QSlider* const blur_slider = rml::qt::QSlider::create(rml::qt::QSlider::Horizontal, dialog.get());
		if (blur_slider)
		{
			blur_slider->setRange(0, 100);
			blur_slider->setValue(to_percent(snap.blur));
			blur_slider->setSingleStep(1);
			blur_slider->setPageStep(10);
			blur_slider->setGeometry(20, 178, 340, 24);
			blur_slider->on_value_changed([this, blur_value](const int percent) {
				if (blur_value)
					blur_value->setText(std::to_string(percent) + "%");
				edit_settings([percent](BackgroundSettings& s) {
					s.blur = clamp_blur(percent / 100.0);
				});
			});
		}

		rml::qt::QPushButton* const scale_button = rml::qt::QPushButton::create(scale_label(snap.scale_mode), dialog.get());
		if (scale_button)
		{
			scale_button->setGeometry(20, 216, 165, 30);
			scale_button->on_clicked([this, scale_button] {
				ScaleMode mode{};
				edit_settings([&mode](BackgroundSettings& s) {
					s.scale_mode = next_scale_mode(s.scale_mode);
					mode = s.scale_mode;
				});
				scale_button->setText(scale_label(mode));
			});
		}

		rml::qt::QPushButton* const align_button = rml::qt::QPushButton::create(align_label(snap.alignment), dialog.get());
		if (align_button)
		{
			align_button->setGeometry(195, 216, 165, 30);
			align_button->on_clicked([this, align_button] {
				Alignment alignment{};
				edit_settings([&alignment](BackgroundSettings& s) {
					s.alignment = next_alignment(s.alignment);
					alignment = s.alignment;
				});
				align_button->setText(align_label(alignment));
			});
		}

		if (rml::qt::QPushButton* const choose_button = rml::qt::QPushButton::create("Choose image...", dialog.get()))
		{
			choose_button->setGeometry(20, 254, 340, 30);
			choose_button->on_clicked([this] {
				const std::string picked =
				    rml::qt::QFileDialog::get_open_file_name(nullptr, "Choose background image", mod_folder().string(), "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.apng *.mng *.tif *.tiff *.ico);;Animated (*.gif *.webp *.apng *.mng);;All files (*)");
				if (picked.empty())
					return;
				edit_settings([&picked](BackgroundSettings& s) {
					s.image = picked;
				});
				m_log->info("background image set to '{}'", picked);
			});
		}

		if (rml::qt::QPushButton* const open_button = rml::qt::QPushButton::create("Open config file", dialog.get()))
		{
			open_button->setGeometry(20, 292, 165, 30);
			open_button->on_clicked([this] {
				rml::utils::shell::open(m_store->path());
			});
		}

		if (rml::qt::QPushButton* const reset_button = rml::qt::QPushButton::create("Reset to defaults", dialog.get()))
		{
			reset_button->setGeometry(195, 292, 165, 30);
			reset_button->on_clicked([this, enabled, slider, opacity_value, blur_slider, blur_value, scale_button, align_button] {
				const BackgroundSettings defaults{};
				edit_settings([&defaults](BackgroundSettings& s) {
					s = defaults;
				});

				if (enabled)
					enabled->setChecked(defaults.enabled);
				if (slider)
					slider->setValue(to_percent(defaults.opacity));
				if (opacity_value)
					opacity_value->setText(std::to_string(to_percent(defaults.opacity)) + "%");
				if (blur_slider)
					blur_slider->setValue(to_percent(defaults.blur));
				if (blur_value)
					blur_value->setText(std::to_string(to_percent(defaults.blur)) + "%");
				if (scale_button)
					scale_button->setText(scale_label(defaults.scale_mode));
				if (align_button)
					align_button->setText(align_label(defaults.alignment));
				m_log->info("settings reset to defaults");
			});
		}

		if (rml::qt::QPushButton* const close_button = rml::qt::QPushButton::create("Close", dialog.get()))
		{
			close_button->setGeometry(20, 330, 340, 32);
			close_button->on_clicked([raw_dialog = dialog.get()] {
				raw_dialog->close();
			});
		}

		dialog->exec();
	}

	std::shared_ptr<spdlog::logger> m_log;
	std::unique_ptr<rml::config::ModSettings> m_store;

	mutable std::mutex m_mutex;
	BackgroundSettings m_settings;
	std::unique_ptr<EditorOverlay> m_overlay;

	std::thread m_scanner;
	std::atomic<bool> m_running{false};

	static inline std::atomic<ScriptEditorBackground*> s_active{nullptr};
};

extern "C"
{
	RML_MOD_ABI_EXPORT ModBase* start_mod()
	{
		return new ScriptEditorBackground();
	}

	RML_MOD_ABI_EXPORT void uninstall_mod(const ModBase* mod)
	{
		delete mod;
	}
}

RML_EXPORT_MOD_ABI_VERSION()