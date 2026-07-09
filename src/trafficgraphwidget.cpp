#include "trafficgraphwidget.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr qreal kHistoryWindowSeconds = 60.0;
constexpr int kMaxSamples = 180;
constexpr int kHorizontalGridLines = 10;
}

TrafficGraphWidget::TrafficGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 180);
    setAttribute(Qt::WA_StyledBackground, true);
    trafficTimer.invalidate();
}

QSize TrafficGraphWidget::sizeHint() const
{
    return {560, 240};
}

QSize TrafficGraphWidget::minimumSizeHint() const
{
    return {320, 180};
}

void TrafficGraphWidget::clearSamples()
{
    samples.clear();
    update();
}

void TrafficGraphWidget::onByteCountReceived(qulonglong uploadBytes, qulonglong downloadBytes)
{
    if (!trafficTimer.isValid()) {
        trafficTimer.start();
    }

    const qint64 nowMs = trafficTimer.elapsed();

    if (!trafficSampleInitialized) {
        trafficSampleInitialized = true;
        lastUploadBytes = uploadBytes;
        lastDownloadBytes = downloadBytes;
        lastTrafficSampleMs = nowMs;
        appendSample(0.0, 0.0, 0.0);
        return;
    }

    const qreal elapsedSeconds = qMax<qreal>(0.001, (nowMs - lastTrafficSampleMs) / 1000.0);
    const qulonglong uploadDeltaBytes = uploadBytes >= lastUploadBytes ? (uploadBytes - lastUploadBytes) : 0;
    const qulonglong downloadDeltaBytes = downloadBytes >= lastDownloadBytes ? (downloadBytes - lastDownloadBytes) : 0;
    const qreal uploadSpeed = static_cast<qreal>(uploadDeltaBytes) / elapsedSeconds;
    const qreal downloadSpeed = static_cast<qreal>(downloadDeltaBytes) / elapsedSeconds;

    lastUploadBytes = uploadBytes;
    lastDownloadBytes = downloadBytes;
    lastTrafficSampleMs = nowMs;

    appendSample(nowMs / 1000.0, uploadSpeed, downloadSpeed);
}

void TrafficGraphWidget::resetTraffic()
{
    trafficTimer.invalidate();
    lastUploadBytes = 0;
    lastDownloadBytes = 0;
    lastTrafficSampleMs = 0;
    trafficSampleInitialized = false;
    clearSamples();
}

void TrafficGraphWidget::appendSample(qreal timeSeconds,
                                      qreal uploadSpeedBytesPerSecond,
                                      qreal downloadSpeedBytesPerSecond)
{
    samples.push_back({timeSeconds, uploadSpeedBytesPerSecond, downloadSpeedBytesPerSecond});
    trimSamples();
    update();
}

QString TrafficGraphWidget::formatSpeed(qreal bytesPerSecond)
{
    const qreal value = qMax<qreal>(0.0, bytesPerSecond);
    const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    int unitIndex = 0;
    qreal scaled = value;
    while (scaled >= 1024.0 && unitIndex < 3) {
        scaled /= 1024.0;
        ++unitIndex;
    }
    return QStringLiteral("%1 %2").arg(QString::number(scaled, 'f', scaled < 10.0 ? 1 : 0), units[unitIndex]);
}

void TrafficGraphWidget::trimSamples()
{
    while (!samples.isEmpty() && samples.first().timeSeconds < samples.last().timeSeconds - kHistoryWindowSeconds) {
        samples.removeFirst();
    }

    while (samples.size() > kMaxSamples) {
        samples.removeFirst();
    }
}

void TrafficGraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF outer = rect().adjusted(1, 1, -1, -1);
    const QRectF plotArea = outer.adjusted(16, 22, -16, -12);
    const QColor borderColor(0, 0, 0, 22);
    const QColor gridColor(0, 0, 0, 20);
    const QColor textColor(92, 99, 112);
    const QColor uploadColor(194, 23, 23);
    const QColor downloadColor(30, 136, 229);

    if (samples.isEmpty()) {
        painter.setPen(QColor(textColor.red(), textColor.green(), textColor.blue(), 160));
        painter.setFont(font());
        painter.drawText(plotArea, Qt::AlignCenter, QStringLiteral("Waiting for traffic samples"));
        return;
    }

    qreal minTime = samples.first().timeSeconds;
    qreal maxTime = samples.last().timeSeconds;
    qreal maxSpeed = 0.0;
    for (const auto &sample : samples) {
        minTime = qMin(minTime, sample.timeSeconds);
        maxTime = qMax(maxTime, sample.timeSeconds);
        maxSpeed = qMax(maxSpeed, qMax(sample.uploadSpeed, sample.downloadSpeed));
    }

    if (qFuzzyCompare(minTime, maxTime)) {
        maxTime = minTime + 1.0;
    }
    if (maxSpeed <= 0.0) {
        maxSpeed = 1.0;
    }

    const qreal timeRange = maxTime - minTime;
    const qreal valueRange = maxSpeed;
    auto xForTime = [&](qreal timeSeconds) {
        return plotArea.left() + ((timeSeconds - minTime) / timeRange) * plotArea.width();
    };
    auto yForValue = [&](qreal value) {
        return plotArea.bottom() - (qBound<qreal>(0.0, value, valueRange) / valueRange) * plotArea.height();
    };

    painter.setPen(QPen(gridColor, 1.0));
    painter.setBrush(Qt::NoBrush);

    for (int row = 0; row < kHorizontalGridLines; ++row) {
        const qreal y = plotArea.top() + (plotArea.height() * row / (kHorizontalGridLines - 1.0));
        painter.drawLine(QPointF(plotArea.left(), y), QPointF(plotArea.right(), y));
    }
    auto drawSeries = [&](const QVector<SamplePoint> &series,
                          const QColor &seriesColor,
                          auto extractor) {
        if (series.isEmpty()) {
            return;
        }

        QPainterPath path;
        QPainterPath fillPath;
        bool started = false;
        for (const auto &sample : series) {
            const QPointF point(xForTime(sample.timeSeconds), yForValue(extractor(sample)));
            if (!started) {
                path.moveTo(point);
                fillPath.moveTo(QPointF(point.x(), plotArea.bottom()));
                fillPath.lineTo(point);
                started = true;
            } else {
                path.lineTo(point);
                fillPath.lineTo(point);
            }
        }

        fillPath.lineTo(QPointF(series.isEmpty() ? plotArea.left() : xForTime(series.last().timeSeconds), plotArea.bottom()));
        fillPath.closeSubpath();

        QColor fillColor = seriesColor;
        fillColor.setAlpha(28);
        painter.fillPath(fillPath, fillColor);

        QPen linePen(seriesColor, 0.5);
        linePen.setCapStyle(Qt::RoundCap);
        linePen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(linePen);
        painter.drawPath(path);
    };

    drawSeries(samples, uploadColor, [](const SamplePoint &sample) { return sample.uploadSpeed; });
    drawSeries(samples, downloadColor, [](const SamplePoint &sample) { return sample.downloadSpeed; });

    QFont smallFont = font();
    smallFont.setBold(true);
    smallFont.setPointSizeF(qMax<qreal>(8.0, smallFont.pointSizeF() - 2.0));
    painter.setFont(smallFont);

    const qreal labelBottom = plotArea.top() - 4.0;
    const qreal labelHeight = 14.0;
    const QString maxText = QStringLiteral("max %1").arg(formatSpeed(maxSpeed));
    painter.setPen(textColor);
    painter.drawText(QRectF(plotArea.right() - 8.0 - 180.0, labelBottom - labelHeight, 180.0, labelHeight),
                     Qt::AlignRight | Qt::AlignBottom, maxText);

    if (!samples.isEmpty()) {
        const auto &last = samples.last();
        const QString upText = QStringLiteral("↑ %1").arg(formatSpeed(last.uploadSpeed));
        const QString downText = QStringLiteral("↓ %1").arg(formatSpeed(last.downloadSpeed));

        painter.setFont(smallFont);

        painter.setPen(uploadColor);
        painter.drawText(QRectF(plotArea.left() + 8.0, labelBottom - labelHeight, 180.0, labelHeight),
                 Qt::AlignLeft | Qt::AlignBottom, upText);

        painter.setPen(downloadColor);
        painter.drawText(QRectF(plotArea.left() + (plotArea.width() - 180.0) / 2.0, labelBottom - labelHeight, 180.0, labelHeight),
                 Qt::AlignHCenter | Qt::AlignBottom, downText);
    }
}