#include "validitytimelinewidget.h"

ValidityTimelineWidget::ValidityTimelineWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::ValidityTimeline)
{
    m_ui->setupUi(this);
}

ValidityTimelineWidget::~ValidityTimelineWidget()
{
    delete m_ui;
}

void ValidityTimelineWidget::setValidityDates(const QDateTime &effectiveDate, const QDateTime &expiryDate)
{
    m_effectiveDate = effectiveDate;
    m_expiryDate = expiryDate;

    const QString dateFormat = QStringLiteral("dd MMM yyyy");
    m_ui->certStartDateLabel->setText(m_effectiveDate.toString(dateFormat));
    m_ui->certEndDateLabel->setText(m_expiryDate.toString(dateFormat));

    updateDisplay(QDateTime::currentDateTimeUtc());
}

void ValidityTimelineWidget::updateDisplay(const QDateTime &now)
{
    if (!m_effectiveDate.isValid() || !m_expiryDate.isValid()) {
        return;
    }

    QString statusColor;
    if (now < m_effectiveDate) {
        statusColor = QStringLiteral("#f59e0b");
    } else if (now > m_expiryDate) {
        statusColor = QStringLiteral("#dc2626");
    } else {
        statusColor = QStringLiteral("#16a34a");
    }

    m_ui->certValidityProgress->setStyleSheet(
        QStringLiteral("QProgressBar { border: none; border-radius: 3px; background-color: rgba(128,128,128,0.15); } "
                       "QProgressBar::chunk { border-radius: 3px; background-color: %1; }").arg(statusColor));

    const qint64 totalSecs = m_effectiveDate.secsTo(m_expiryDate);
    if (totalSecs <= 0) {
        m_ui->certValidityProgress->setValue(100);
        m_ui->certTimeRemainingLabel->setText(QStringLiteral("Expired"));
        return;
    }

    qint64 elapsedSecs = m_effectiveDate.secsTo(now);
    if (elapsedSecs < 0) {
        elapsedSecs = 0;
    }

    int progressPercent = 0;
    if (elapsedSecs >= totalSecs) {
        progressPercent = 100;
    } else {
        progressPercent = static_cast<int>((elapsedSecs * 100) / totalSecs);
    }
    m_ui->certValidityProgress->setValue(progressPercent);

    if (now >= m_expiryDate) {
        m_ui->certTimeRemainingLabel->setText(QStringLiteral("Expired"));
    } else {
        const qint64 secsRemaining = now.secsTo(m_expiryDate);
        const qint64 daysRemaining = secsRemaining / 86400;
        const qint64 hoursRemaining = (secsRemaining % 86400) / 3600;
        const qint64 monthsRemaining = daysRemaining / 30;
        const qint64 remainingDays = daysRemaining % 30;

        QString timeStr;
        if (monthsRemaining > 0) {
            timeStr = QStringLiteral("%1 month%2 %3 day%4")
                          .arg(monthsRemaining)
                          .arg(monthsRemaining == 1 ? QString() : QStringLiteral("s"))
                          .arg(remainingDays)
                          .arg(remainingDays == 1 ? QString() : QStringLiteral("s"));
        } else if (daysRemaining > 0) {
            timeStr = QStringLiteral("%1 day%2 %3 hour%4")
                          .arg(daysRemaining)
                          .arg(daysRemaining == 1 ? QString() : QStringLiteral("s"))
                          .arg(hoursRemaining)
                          .arg(hoursRemaining == 1 ? QString() : QStringLiteral("s"));
        } else {
            timeStr = QStringLiteral("%1 hour%2")
                          .arg(hoursRemaining)
                          .arg(hoursRemaining == 1 ? QString() : QStringLiteral("s"));
        }
        m_ui->certTimeRemainingLabel->setText(timeStr);
    }
}

void ValidityTimelineWidget::clear()
{
    m_effectiveDate = QDateTime();
    m_expiryDate = QDateTime();
    m_ui->certStartDateLabel->setText(QStringLiteral("—"));
    m_ui->certEndDateLabel->setText(QStringLiteral("—"));
    m_ui->certTimeRemainingLabel->setText(QStringLiteral("—"));
    m_ui->certValidityProgress->setValue(0);
}