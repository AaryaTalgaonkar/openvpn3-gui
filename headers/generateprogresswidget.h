#ifndef GENERATEPROGRESSWIDGET_H
#define GENERATEPROGRESSWIDGET_H

#include <QWidget>

class QLabel;
class QProgressBar;

#include "ui_generateprogress.h"

class GenerateProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GenerateProgressWidget(QWidget *parent = nullptr);
    ~GenerateProgressWidget();

    void showProgress(bool show);
    void setValue(int value);
    void setStatusText(const QString &text);
    void reset();

private:
    Ui::GenerateProgress *m_ui;
};

#endif // GENERATEPROGRESSWIDGET_H