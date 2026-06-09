#include "connectionprogresswidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

ConnectionProgressWidget::ConnectionProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(220, 220);
}

QSize ConnectionProgressWidget::sizeHint() const
{
    return {240, 240};
}

QSize ConnectionProgressWidget::minimumSizeHint() const
{
    return {220, 220};
}

void ConnectionProgressWidget::setProgressStep(int stepIndex, int stepCount)
{
    currentStep = qBound(0, stepIndex, qMax(0, stepCount - 1));
    totalSteps = qMax(1, stepCount);
    update();
}

void ConnectionProgressWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QColor accent(34, 197, 94);
    accent.setAlphaF(0.7);
    const QColor iconColor(21, 128, 61);

    const QRectF widgetRect = rect();
    const QPointF center = widgetRect.center();
    const qreal minSide = qMin(widgetRect.width(), widgetRect.height());

    const qreal outerRadius = (minSide * 0.3) ;

    const qreal buttonRadius = qMax(28.0, minSide * 0.14);

    const qreal buttonBackgroundRadius = buttonRadius ;

    const qreal ringCount = qMax(1, totalSteps);

    const qreal ringSpacing =
        (outerRadius - buttonBackgroundRadius) / ringCount;

    const int completedBands =
        qBound(0, currentStep + 1, totalSteps);

    painter.setPen(Qt::NoPen);

    for (int bandIndex = 0; bandIndex < completedBands; ++bandIndex) {

        const qreal innerRadius =
            buttonBackgroundRadius + ringSpacing * bandIndex;

        const qreal outerBandRadius =
            innerRadius + ringSpacing;

        const qreal opacity =
            qBound(0.10,
                   0.6 - (0.10 * bandIndex),
                   0.6);

        QColor bandColor = accent;
        bandColor.setAlphaF(opacity);

        QPainterPath bandPath;
        bandPath.setFillRule(Qt::OddEvenFill);

        bandPath.addEllipse(QRectF(center.x() - outerBandRadius,
                                   center.y() - outerBandRadius,
                                   outerBandRadius * 2.0,
                                   outerBandRadius * 2.0));

        bandPath.addEllipse(QRectF(center.x() - innerRadius,
                                   center.y() - innerRadius,
                                   innerRadius * 2.0,
                                   innerRadius * 2.0));

        painter.setBrush(bandColor);
        painter.drawPath(bandPath);
    }

    painter.setBrush(accent);

    painter.drawEllipse(QRectF(center.x() - buttonBackgroundRadius,
                               center.y() - buttonBackgroundRadius,
                               buttonBackgroundRadius * 2.0,
                               buttonBackgroundRadius * 2.0));

    QFont powerFont = font();
    powerFont.setBold(true);
    powerFont.setPointSizeF(buttonRadius * 0.95);

    painter.setFont(powerFont);
    painter.setPen(iconColor);

    painter.drawText(QRectF(center.x() - buttonRadius,
                            center.y() - buttonRadius,
                            buttonRadius * 2.0,
                            buttonRadius * 2.0),
                     Qt::AlignCenter,
                     QString::fromUtf8("⏻"));
}