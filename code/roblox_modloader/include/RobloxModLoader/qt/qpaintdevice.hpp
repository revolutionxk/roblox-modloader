#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace rml::qt
{
	class QPaintDevicePrivate;
	class QPaintEngine;
	class QPainter;
	class QPoint;

	class RML_EXPORT QPaintDevice
	{
	public:
		enum PaintDeviceMetric
		{
			PdmWidth = 1,
			PdmHeight,
			PdmWidthMM,
			PdmHeightMM,
			PdmNumColors,
			PdmDepth,
			PdmDpiX,
			PdmDpiY,
			PdmPhysicalDpiX,
			PdmPhysicalDpiY,
			PdmDevicePixelRatio,
			PdmDevicePixelRatioScaled,
		};

		virtual ~QPaintDevice() = default;
		virtual int devType() const = 0;
		virtual QPaintEngine* paintEngine() const = 0;

		std::uint16_t painters{};
		QPaintDevicePrivate* reserved{};

	protected:
		QPaintDevice() noexcept = default;

		virtual int metric(PaintDeviceMetric metric) const = 0;
		virtual void initPainter(QPainter* painter) const = 0;
		virtual QPaintDevice* redirected(QPoint* offset) const = 0;
		virtual QPainter* sharedPainter() const = 0;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QPaintDevice, painters, sizeof(void*));
	RML_ASSERT_OFFSET(QPaintDevice, reserved, 2 * sizeof(void*));
	RML_ASSERT_SIZE(QPaintDevice, 3 * sizeof(void*));
	RML_LAYOUT_DIAGNOSTIC_POP()
}
