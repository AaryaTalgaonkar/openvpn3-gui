#ifndef TRAFFICGRAPHWIDGET_H
#define TRAFFICGRAPHWIDGET_H

#include <QWidget>
#include <QVector>
#include <QElapsedTimer>

class TrafficGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrafficGraphWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void clearSamples();
    void appendSample(qreal timeSeconds, qreal uploadSpeedBytesPerSecond, qreal downloadSpeedBytesPerSecond);

    void onByteCountReceived(qulonglong uploadBytes, qulonglong downloadBytes);
    void resetTraffic();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct SamplePoint {
        qreal timeSeconds = 0.0;
        qreal uploadSpeed = 0.0;
        qreal downloadSpeed = 0.0;
    };

    QVector<SamplePoint> samples;

    static QString formatSpeed(qreal bytesPerSecond);
    void trimSamples();

    // Traffic tracking
    QElapsedTimer trafficTimer;
    qulonglong lastUploadBytes = 0;
    qulonglong lastDownloadBytes = 0;
    qint64 lastTrafficSampleMs = 0;
    bool trafficSampleInitialized = false;
};

#endif