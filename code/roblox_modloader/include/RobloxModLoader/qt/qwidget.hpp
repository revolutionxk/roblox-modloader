#pragma once

#include "RobloxModLoader/internal/platform.hpp"
#include "RobloxModLoader/qt/qobject.hpp"
#include "RobloxModLoader/qt/qpaintdevice.hpp"
#include "RobloxModLoader/qt/qpalette.hpp"
#include "RobloxModLoader/qt/qsize.hpp"

namespace rml::qt
{
	class QActionEvent;
	class QByteArray;
	class QCloseEvent;
	class QContextMenuEvent;
	class QDragEnterEvent;
	class QDragLeaveEvent;
	class QDragMoveEvent;
	class QDropEvent;
	class QEvent;
	class QFocusEvent;
	class QHideEvent;
	class QInputMethodEvent;
	class QKeyEvent;
	class QMouseEvent;
	class QMoveEvent;
	class QPaintEvent;
	class QResizeEvent;
	class QShowEvent;
	class QString;
	class QTabletEvent;
	class QVariant;
	class QWheelEvent;
	class QWidgetData;

	class RML_EXPORT QWidget : public QObject, public QPaintDevice
	{
	public:
		int devType() const override = 0;
		virtual void setVisible(bool visible) = 0;
		virtual QSize sizeHint() const = 0;
		virtual QSize minimumSizeHint() const = 0;
		virtual int heightForWidth(int width) const = 0;
		virtual bool hasHeightForWidth() const = 0;
		QPaintEngine* paintEngine() const override = 0;
		virtual void mousePressEvent(QMouseEvent* event) = 0;
		virtual void mouseReleaseEvent(QMouseEvent* event) = 0;
		virtual void mouseDoubleClickEvent(QMouseEvent* event) = 0;
		virtual void mouseMoveEvent(QMouseEvent* event) = 0;
		virtual void wheelEvent(QWheelEvent* event) = 0;
		virtual void keyPressEvent(QKeyEvent* event) = 0;
		virtual void keyReleaseEvent(QKeyEvent* event) = 0;
		virtual void focusInEvent(QFocusEvent* event) = 0;
		virtual void focusOutEvent(QFocusEvent* event) = 0;
		virtual void enterEvent(QEvent* event) = 0;
		virtual void leaveEvent(QEvent* event) = 0;
		virtual void paintEvent(QPaintEvent* event) = 0;
		virtual void moveEvent(QMoveEvent* event) = 0;
		virtual void resizeEvent(QResizeEvent* event) = 0;
		virtual void closeEvent(QCloseEvent* event) = 0;
		virtual void contextMenuEvent(QContextMenuEvent* event) = 0;
		virtual void tabletEvent(QTabletEvent* event) = 0;
		virtual void actionEvent(QActionEvent* event) = 0;
		virtual void dragEnterEvent(QDragEnterEvent* event) = 0;
		virtual void dragMoveEvent(QDragMoveEvent* event) = 0;
		virtual void dragLeaveEvent(QDragLeaveEvent* event) = 0;
		virtual void dropEvent(QDropEvent* event) = 0;
		virtual void showEvent(QShowEvent* event) = 0;
		virtual void hideEvent(QHideEvent* event) = 0;
		virtual bool nativeEvent(const QByteArray& event_type, void* message, long* result) = 0;
		virtual void changeEvent(QEvent* event) = 0;
		int metric(PaintDeviceMetric metric) const override = 0;
		void initPainter(QPainter* painter) const override = 0;
		QPaintDevice* redirected(QPoint* offset) const override = 0;
		QPainter* sharedPainter() const override = 0;
		virtual void inputMethodEvent(QInputMethodEvent* event) = 0;
		virtual QVariant inputMethodQuery(int query) const = 0;
		virtual bool focusNextPrevChild(bool next) = 0;

		QWidgetData* data;

		void setWindowTitle(const QString& title);
		void setStyleSheet(const QString& style);
		void show();

		void resize(int width, int height);
		void setFixedSize(int width, int height);
		void setGeometry(int x, int y, int width, int height);
		void move(int x, int y);

		void setEnabled(bool enabled);
		void setToolTip(const QString& text);
		bool close();

		[[nodiscard]] QWidget* viewport() const;

		[[nodiscard]] QPalette palette() const;
		void set_palette(const QPalette& palette);

		[[nodiscard]] int width() const;
		[[nodiscard]] int height() const;

		void set_auto_fill_background(bool enabled);

		void update();

	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QWidget, data, 5 * sizeof(void*));
	RML_ASSERT_SIZE(QWidget, 6 * sizeof(void*));
	RML_LAYOUT_DIAGNOSTIC_POP()
}
