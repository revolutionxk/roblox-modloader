#pragma once

#include "RobloxModLoader/qt/qpaintdevice.hpp"
#include "RobloxModLoader/rml_export.hpp"

namespace rml::qt
{
	struct QImageData;

	class RML_EXPORT QImage : public QPaintDevice
	{
	public:
		QImageData* d{};

		QImage();
		~QImage() override;

		QImage(const QImage&) = delete;
		QImage& operator=(const QImage&) = delete;

		int devType() const override;
		QPaintEngine* paintEngine() const override;

	protected:
		int metric(PaintDeviceMetric metric) const override;
		void initPainter(QPainter* painter) const override;
		QPaintDevice* redirected(QPoint* offset) const override;
		QPainter* sharedPainter() const override;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QImage, d, 3 * sizeof(void*));
	RML_ASSERT_SIZE(QImage, 4 * sizeof(void*));
	RML_LAYOUT_DIAGNOSTIC_POP()
}
