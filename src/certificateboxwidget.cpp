#include "certificateboxwidget.h"

#include <QFile>
#include <QRegularExpression>
#include <QSslCertificate>

CertificateBoxWidget::CertificateBoxWidget(QWidget *parent)
    : QFrame(parent)
    , m_ui(new Ui::CertificateBox)
{
    m_ui->setupUi(this);

    // Forward the generate button click as our own signal
    connect(m_ui->generateButton, &QPushButton::clicked,
            this, &CertificateBoxWidget::generateClicked);
}

CertificateBoxWidget::~CertificateBoxWidget()
{
    delete m_ui;
}

void CertificateBoxWidget::showNoCertMode()
{
    m_ui->certInfoStack->setCurrentIndex(0); // Show "no certificate" page
}

void CertificateBoxWidget::showCertInfo(const QString &cn, const QString &org, const QString &email,
                                         const QDateTime &effectiveDate, const QDateTime &expiryDate)
{
    m_ui->certInfoStack->setCurrentIndex(1); // Show certificate info page

    m_ui->certUsernameLabel->setText(cn);
    m_ui->certInstituteLabel->setText(org);
    m_ui->certEmailLabel->setText(email);

    m_certEffectiveDate = effectiveDate;
    m_certExpiryDate = expiryDate;

    const QString dateFormat = QStringLiteral("dd MMM yyyy");
    m_ui->certStartDateLabel->setText(m_certEffectiveDate.toString(dateFormat));
    m_ui->certEndDateLabel->setText(m_certExpiryDate.toString(dateFormat));

    // Show time remaining using local system time initially;
    // will be refined when Google time arrives.
    updateValidityDisplay(QDateTime::currentDateTimeUtc());
}

void CertificateBoxWidget::loadFromOvpnFile(const QString &ovpnPath)
{
    QFile file(ovpnPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Extract the <cert> ... </cert> section
    QRegularExpression certRegex(QStringLiteral("<cert>\\s*(-----BEGIN CERTIFICATE-----[\\s\\S]*?-----END CERTIFICATE-----)\\s*</cert>"),
                                 QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch certMatch = certRegex.match(content);
    if (!certMatch.hasMatch()) {
        return;
    }

    const QString pemData = certMatch.captured(1).trimmed();

    const QList<QSslCertificate> certs = QSslCertificate::fromData(pemData.toUtf8(), QSsl::Pem);
    if (certs.isEmpty()) {
        return;
    }

    const QSslCertificate &cert = certs.first();

    // Extract subject details
    const QString cn = cert.subjectInfo(QSslCertificate::CommonName).value(0, QStringLiteral("—"));
    const QString org = cert.subjectInfo(QSslCertificate::Organization).value(0, QStringLiteral("—"));
    const QString email = cert.subjectInfo(QSslCertificate::EmailAddress).value(0, QStringLiteral("—"));

    // Populate the UI via the existing helper
    showCertInfo(cn, org, email, cert.effectiveDate().toUTC(), cert.expiryDate().toUTC());

    // Notify the owner (e.g. MainWindow) that parsing succeeded
    emit certificateParsed();
}

void CertificateBoxWidget::updateValidityDisplay(const QDateTime &now)
{
    if (!m_certEffectiveDate.isValid() || !m_certExpiryDate.isValid()) {
        return;
    }

    // Calculate total validity span
    const qint64 totalSecs = m_certEffectiveDate.secsTo(m_certExpiryDate);
    if (totalSecs <= 0) {
        m_ui->certValidityProgress->setValue(100);
        m_ui->certTimeRemainingLabel->setText(QStringLiteral("Expired"));
        return;
    }

    // Calculate elapsed seconds since effective date
    qint64 elapsedSecs = m_certEffectiveDate.secsTo(now);
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

    // Calculate time remaining from now until expiry
    if (now >= m_certExpiryDate) {
        m_ui->certTimeRemainingLabel->setText(QStringLiteral("Expired"));
    } else {
        const qint64 secsRemaining = now.secsTo(m_certExpiryDate);
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

void CertificateBoxWidget::setGenerateEnabled(bool enabled)
{
    m_ui->generateButton->setEnabled(enabled);
}

void CertificateBoxWidget::setGenerateButtonText(const QString &text)
{
    m_ui->generateButton->setText(text);
}

void CertificateBoxWidget::showDownloadProgress(bool show)
{
    m_ui->downloadProgress->setVisible(show);
    m_ui->downloadStatusLabel->setVisible(show);
    if (!show) {
        m_ui->downloadProgress->setValue(0);
        m_ui->downloadStatusLabel->clear();
    }
}

void CertificateBoxWidget::setDownloadStatus(const QString &text)
{
    m_ui->downloadStatusLabel->setText(text);
}

void CertificateBoxWidget::setDownloadProgress(int value)
{
    m_ui->downloadProgress->setValue(value);
}
