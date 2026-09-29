#pragma once

#include "RobloxModLoader/qt/qpaintdevice.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <string_view>

namespace rml::qt
{
	class QColor;
	class QPlatformPixmap;

	class RML_EXPORT QPixmap : public QPaintDevice
	{
	public:
		enum class AspectMode
		{
			Ignore = 0,
			Keep = 1,
			KeepByExpanding = 2,
		};

		enum class TransformMode
		{
			Fast = 0,
			Smooth = 1,
		};

		QPlatformPixmap* data{};

		QPixmap();
		explicit QPixmap(std::string_view file_path);
		QPixmap(int width, int height);
		QPixmap(const QPixmap& other);
		QPixmap(QPixmap&& other) noexcept;
		QPixmap& operator=(const QPixmap& other);
		QPixmap& operator=(QPixmap&& other) noexcept;
		~QPixmap() override;

		int devType() const override;
		QPaintEngine* paintEngine() const override;

		[[nodiscard]] bool loaded() const;
		[[nodiscard]] int width() const;
		[[nodiscard]] int height() const;
		void fill(const QColor& color) const;
		bool save(std::string_view file_path) const;
		[[nodiscard]] QPixmap scaled(int width, int height, AspectMode aspect = AspectMode::Ignore,
		                             TransformMode transform = TransformMode::Smooth) const;
		[[nodiscard]] QPixmap blurred(double radius) const;

	protected:
		int metric(PaintDeviceMetric metric) const override;
		void initPainter(QPainter* painter) const override;
		QPaintDevice* redirected(QPoint* offset) const override;
		QPainter* sharedPainter() const override;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QPixmap, data, 3 * sizeof(void*));
	RML_ASSERT_SIZE(QPixmap, 4 * sizeof(void*));
	RML_LAYOUT_DIAGNOSTIC_POP()
}
