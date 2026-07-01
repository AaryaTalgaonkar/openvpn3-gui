#ifndef VALIDITYTIMELINEWIDGET_H
#define VALIDITYTIMELINEWIDGET_H

#include <QWidget>
#include <QDateTime>

class QLabel;
class QProgressBar;

#include "ui_validitytimeline.h"

class ValidityTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ValidityTimelineWidget(QWidget *parent = nullptr);
    ~ValidityTimelineWidget();

    void setValidityDates(const QDateTime &effectiveDate, const QDateTime &expiryDate);
    void updateDisplay(const QDateTime &now);
    void clear();

private:
    Ui::ValidityTimeline *m_ui;
    QDateTime m_effectiveDate;
    QDateTime m_expiryDate;
};

#endif // VALIDITYTIMELINEWIDGET_H