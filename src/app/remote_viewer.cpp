#include "remote_viewer.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QWheelEvent>

#include <algorithm>

RemoteViewer::RemoteViewer(
    PointerHandler pointerHandler,
    ButtonHandler buttonHandler,
    WheelHandler wheelHandler,
    KeyHandler keyHandler
)
    : pointerHandler_(std::move(pointerHandler)),
      buttonHandler_(std::move(buttonHandler)),
      wheelHandler_(std::move(wheelHandler)),
      keyHandler_(std::move(keyHandler))
{
    setWindowTitle("D1Assist - Tela remota");
    setMinimumSize(640, 360);
    setAlignment(Qt::AlignCenter);
    setScaledContents(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void RemoteViewer::displayFrame(const QImage& frame)
{
    setPixmap(QPixmap::fromImage(frame));
}

void RemoteViewer::setCloseHandler(std::function<void()> closeHandler)
{
    closeHandler_ = std::move(closeHandler);
}

std::uint16_t RemoteViewer::normalizedX(qreal x) const noexcept
{
    if (width() <= 1)
    {
        return 0;
    }
    const auto bounded = std::clamp(x / static_cast<qreal>(width() - 1), 0.0, 1.0);
    return static_cast<std::uint16_t>(bounded * 65'535.0 + 0.5);
}

std::uint16_t RemoteViewer::normalizedY(qreal y) const noexcept
{
    if (height() <= 1)
    {
        return 0;
    }
    const auto bounded = std::clamp(y / static_cast<qreal>(height() - 1), 0.0, 1.0);
    return static_cast<std::uint16_t>(bounded * 65'535.0 + 0.5);
}

void RemoteViewer::mouseMoveEvent(QMouseEvent* event)
{
    pointerHandler_(normalizedX(event->position().x()), normalizedY(event->position().y()));
    QLabel::mouseMoveEvent(event);
}

void RemoteViewer::mousePressEvent(QMouseEvent* event)
{
    buttonHandler_(
        static_cast<std::uint16_t>(event->button()),
        true,
        normalizedX(event->position().x()),
        normalizedY(event->position().y())
    );
    setFocus(Qt::MouseFocusReason);
    QLabel::mousePressEvent(event);
}

void RemoteViewer::mouseReleaseEvent(QMouseEvent* event)
{
    buttonHandler_(
        static_cast<std::uint16_t>(event->button()),
        false,
        normalizedX(event->position().x()),
        normalizedY(event->position().y())
    );
    QLabel::mouseReleaseEvent(event);
}

void RemoteViewer::wheelEvent(QWheelEvent* event)
{
    wheelHandler_(
        static_cast<std::int16_t>(event->angleDelta().y()),
        normalizedX(event->position().x()),
        normalizedY(event->position().y())
    );
    event->accept();
}

void RemoteViewer::keyPressEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
    {
        keyHandler_(static_cast<std::uint32_t>(event->key()), true);
    }
    event->accept();
}

void RemoteViewer::keyReleaseEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
    {
        keyHandler_(static_cast<std::uint32_t>(event->key()), false);
    }
    event->accept();
}

void RemoteViewer::closeEvent(QCloseEvent* event)
{
    if (closeHandler_)
    {
        closeHandler_();
    }
    QLabel::closeEvent(event);
}
