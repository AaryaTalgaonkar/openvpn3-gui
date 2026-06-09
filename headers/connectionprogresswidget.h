#ifndef CONNECTIONPROGRESSWIDGET_H
#define CONNECTIONPROGRESSWIDGET_H

#include <QWidget>

class ConnectionProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionProgressWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void setProgressStep(int stepIndex, int stepCount);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int currentStep = 0;
    int totalSteps = 7;
};

#endif