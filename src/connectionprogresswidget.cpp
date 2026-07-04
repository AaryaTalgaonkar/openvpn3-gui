#include "connectionprogresswidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

namespace {
constexpr auto kConnectingAccentColor = "#16a34a";
}

ConnectionProgressWidget::ConnectionProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(220, 220);

    spinnerTimer = new QTimer(this);
    connect(spinnerTimer, &QTimer::timeout, this, &ConnectionProgressWidget::refreshSpinnerFrame);
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

void ConnectionProgressWidget::setupSteps(QVBoxLayout *stepsLayout, QVBoxLayout *hostLayout,
                                          const ConnectionStepDefinition *steps, int count)
{
    connectionSteps = steps;
    connectionStepCount = count;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hostLayout->addWidget(this, 0, Qt::AlignCenter);

    while (QLayoutItem *item = stepsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    stepStatusLabels.clear();

    for (int i = 0; i < connectionStepCount; ++i) {
        const auto &step = connectionSteps[i];
        auto *row = new QWidget(qobject_cast<QWidget *>(stepsLayout->parentWidget()));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        auto *iconLabel = new QLabel(QString::fromUtf8(step.icon), row);
        iconLabel->setFixedWidth(24);
        iconLabel->setAlignment(Qt::AlignCenter);

        auto *titleLabel = new QLabel(QString::fromUtf8(step.label), row);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto *statusLabel = new QLabel(row);
        statusLabel->setFixedSize(24, 24);
        statusLabel->setAlignment(Qt::AlignCenter);

        rowLayout->addWidget(iconLabel);
        rowLayout->addWidget(titleLabel, 1);
        rowLayout->addWidget(statusLabel);
        stepsLayout->addWidget(row);
        stepStatusLabels.push_back(statusLabel);
    }

    resetIndicators();
}

void ConnectionProgressWidget::updateStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= connectionStepCount) {
        return;
    }

    if (spinnerFrames.isEmpty()) {
        spinnerFrames = {QString::fromUtf8("◐"), QString::fromUtf8("◓"), QString::fromUtf8("◑"), QString::fromUtf8("◒")};
    }

    if (stepIndex < connectionStepCount - 1) {
        if (!spinnerTimer->isActive()) {
            spinnerTimer->start(140);
        }
    }
    updateStepRows(stepIndex);
}

void ConnectionProgressWidget::resetIndicators()
{
    spinnerFrameIndex = 0;
    activeStepIndex = 0;
    setProgressStep(0, qMax(1, connectionStepCount > 0 ? connectionStepCount : stepStatusLabels.size()));
    updateStepRows(0);
}

int ConnectionProgressWidget::stepIndexForState(const QString &state) const
{
    if (!connectionSteps || connectionStepCount == 0) {
        return -1;
    }
    const QString normalized = state.trimmed().toUpper();
    for (int index = 0; index < connectionStepCount; ++index) {
        if (normalized == QLatin1String(connectionSteps[index].state)) {
            return index;
        }
    }
    return -1;
}

void ConnectionProgressWidget::refreshSpinnerFrame()
{
    if (spinnerFrames.isEmpty() || activeStepIndex < 0 || activeStepIndex >= stepStatusLabels.size() - 1) {
        return;
    }

    spinnerFrameIndex = (spinnerFrameIndex + 1) % spinnerFrames.size();
    updateStepRows(activeStepIndex);
}

void ConnectionProgressWidget::updateStepRows(int currentIndex)
{
    if (stepStatusLabels.isEmpty()) {
        return;
    }

    const int lastIndex = stepStatusLabels.size() - 1;
    const int boundedIndex = qBound(0, currentIndex, lastIndex);
    activeStepIndex = boundedIndex;

    for (int index = 0; index < stepStatusLabels.size(); ++index) {
        QLabel *statusLabel = stepStatusLabels.at(index);
        if (!statusLabel) {
            continue;
        }

        if (index < boundedIndex) {
            statusLabel->setText(QString::fromUtf8("✓"));
            statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(34,197,94,0.75); border-radius: 12px; color: #16a34a; background-color: rgba(34,197,94,0.12); font-weight: bold; }"));
        } else if (index == boundedIndex) {
            if (boundedIndex >= lastIndex) {
                statusLabel->setText(QString::fromUtf8("✓"));
                statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(34,197,94,0.75); border-radius: 12px; color: #16a34a; background-color: rgba(34,197,94,0.12); font-weight: bold; }"));
            } else {
                const QString spinner = spinnerFrames.isEmpty() ? QStringLiteral("◐") : spinnerFrames.at(spinnerFrameIndex % spinnerFrames.size());
                statusLabel->setText(spinner);
                statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(34,197,94,0.85); border-radius: 12px; color: %1; background-color: rgba(34,197,94,0.08); font-weight: bold; }").arg(kConnectingAccentColor));
            }
        } else {
            statusLabel->setText(QString());
            statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(0,0,0,0.14); border-radius: 12px; color: rgba(0,0,0,0.35); background-color: transparent; }"));
        }
    }

    setProgressStep(boundedIndex, stepStatusLabels.size());

    if (boundedIndex >= lastIndex) {
        spinnerTimer->stop();
        emit allStepsCompleted();
    }
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

    const qreal outerRadius = (minSide * 0.3);

    const qreal buttonRadius = qMax(28.0, minSide * 0.14);

    const qreal buttonBackgroundRadius = buttonRadius;

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