#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include <QWidget>
#include <QDateTime>

class QLabel;
class QPushButton;

#include "ui_statuswidget.h"

class StatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatusWidget(QWidget *parent = nullptr);
    ~StatusWidget();

    void showGenerateMode();
    void showStatusPill(const QString &text, const QString &textColor, const QString &bgColor);
    void showExpiredMode();

    void setGenerateEnabled(bool enabled);
    void setGenerateButtonText(const QString &text);

signals:
    void generateClicked();
    void renewClicked();

private:
    Ui::StatusWidget *m_ui;
};

#endif // STATUSWIDGET_H