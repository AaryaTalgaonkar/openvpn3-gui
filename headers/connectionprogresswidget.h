#ifndef CONNECTIONPROGRESSWIDGET_H
#define CONNECTIONPROGRESSWIDGET_H

#include <QWidget>
#include <QVector>
#include <QStringList>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include "ivpnbackend.h"

class ConnectionProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionProgressWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void setProgressStep(int stepIndex, int stepCount);

    void setupSteps(QVBoxLayout *stepsLayout, QVBoxLayout *hostLayout,
                    const ConnectionStepDefinition *steps, int count);
    void updateStep(int stepIndex);
    void resetIndicators();
    int stepIndexForState(const QString &state) const;

signals:
    void allStepsCompleted();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void refreshSpinnerFrame();

private:
    int currentStep = 0;
    int totalSteps = 7;

    // Step tracking
    QVector<QLabel *> stepStatusLabels;
    QTimer *spinnerTimer = nullptr;
    QStringList spinnerFrames;
    int spinnerFrameIndex = 0;
    int activeStepIndex = -1;
    const ConnectionStepDefinition *connectionSteps = nullptr;
    int connectionStepCount = 0;

    void updateStepRows(int currentIndex);
};

#endif