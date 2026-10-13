#pragma once

#include <QLabel>

#include <cstdint>
#include <functional>

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

class RemoteViewer : public QLabel
{
public:
    using PointerHandler = std::function<void(std::uint16_t, std::uint16_t)>;
    using ButtonHandler = std::function<void(std::uint16_t, bool, std::uint16_t, std::uint16_t)>;
    using WheelHandler = std::function<void(std::int16_t, std::uint16_t, std::uint16_t)>;
    using KeyHandler = std::function<void(std::uint32_t, bool)>;

    RemoteViewer(
        PointerHandler pointerHandler,
        ButtonHandler buttonHandler,
        WheelHandler wheelHandler,
        KeyHandler keyHandler
    );
    void displayFrame(const QImage& frame);
    void setCloseHandler(std::function<void()> closeHandler);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    std::uint16_t normalizedX(qreal x) const noexcept;
    std::uint16_t normalizedY(qreal y) const noexcept;

    PointerHandler pointerHandler_;
    ButtonHandler buttonHandler_;
    WheelHandler wheelHandler_;
    KeyHandler keyHandler_;
    std::function<void()> closeHandler_;
};
