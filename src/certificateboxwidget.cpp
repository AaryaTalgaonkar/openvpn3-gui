#include "certificateboxwidget.h"
#include "userinfowidget.h"
#include "statuswidget.h"
#include "validitytimelinewidget.h"
#include "generateprogresswidget.h"

#include <QFile>
#include <QRegularExpression>
#include <QSslCertificate>

CertificateBoxWidget::CertificateBoxWidget(QWidget *parent)
    : QFrame(parent)
    , m_ui(new Ui::CertificateBox)
{
    m_ui->setupUi(this);

    m_noCertStatus = findChild<StatusWidget *>(QStringLiteral("noCertStatusWidget"));
    m_certStatus = findChild<StatusWidget *>(QStringLiteral("certStatusWidget"));
    m_userInfo = findChild<UserInfoWidget *>(QStringLiteral("certUserInfo"));
    m_validityTimeline = findChild<ValidityTimelineWidget *>(QStringLiteral("certValidityTimeline"));
    m_generateProgress = findChild<GenerateProgressWidget *>(QStringLiteral("noCertProgress"));

    if (m_noCertStatus) {
        connect(m_noCertStatus, &StatusWidget::generateClicked,
                this, &CertificateBoxWidget::generateClicked);
    }

    if (m_certStatus) {
        connect(m_certStatus, &StatusWidget::generateClicked,
                this, &CertificateBoxWidget::generateClicked);
        connect(m_certStatus, &StatusWidget::renewClicked,
                this, &CertificateBoxWidget::onRenewClicked);
    }

    m_ui->certInfoStack->setCurrentIndex(0);
}

CertificateBoxWidget::~CertificateBoxWidget()
{
    delete m_ui;
}

void CertificateBoxWidget::showNoCertMode()
{
    m_ui->certInfoStack->setCurrentIndex(0);
    if (m_noCertStatus) {
        m_noCertStatus->showGenerateMode();
    }
}

void CertificateBoxWidget::showCertInfo(const QString &cn, const QString &org, const QString &email,
                                         const QDateTime &effectiveDate, const QDateTime &expiryDate)
{
    m_ui->certInfoStack->setCurrentIndex(1);

    m_certEffectiveDate = effectiveDate;
    m_certExpiryDate = expiryDate;

    if (m_userInfo) {
        m_userInfo->setUserInfo(cn, org, email);
    }

    if (m_validityTimeline) {
        m_validityTimeline->setValidityDates(effectiveDate, expiryDate);
    }

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

    const QString cn = cert.subjectInfo(QSslCertificate::CommonName).value(0, QStringLiteral("—"));
    const QString org = cert.subjectInfo(QSslCertificate::Organization).value(0, QStringLiteral("—"));
    const QString email = cert.subjectInfo(QSslCertificate::EmailAddress).value(0, QStringLiteral("—"));

    showCertInfo(cn, org, email, cert.effectiveDate().toUTC(), cert.expiryDate().toUTC());

    emit certificateParsed();
}

void CertificateBoxWidget::updateValidityDisplay(const QDateTime &now)
{
    if (!m_certEffectiveDate.isValid() || !m_certExpiryDate.isValid()) {
        return;
    }

    if (m_validityTimeline) {
        m_validityTimeline->updateDisplay(now);
    }

    if (!m_certStatus) {
        return;
    }

    if (now > m_certExpiryDate) {
        m_certStatus->showExpiredMode();
    } else if (now < m_certEffectiveDate) {
        m_certStatus->showStatusPill(
            QStringLiteral("Not Yet Valid"),
            QStringLiteral("#f59e0b"),
            QStringLiteral("rgba(245,158,11,0.15)"));
    } else {
        m_certStatus->showStatusPill(
            QStringLiteral("Active"),
            QStringLiteral("#16a34a"),
            QStringLiteral("rgba(22,163,74,0.15)"));
    }
}

void CertificateBoxWidget::setGenerateEnabled(bool enabled)
{
    if (m_noCertStatus) {
        m_noCertStatus->setGenerateEnabled(enabled);
    }
    if (m_certStatus) {
        m_certStatus->setGenerateEnabled(enabled);
    }
}

void CertificateBoxWidget::setGenerateButtonText(const QString &text)
{
    if (m_noCertStatus) {
        m_noCertStatus->setGenerateButtonText(text);
    }
    if (m_certStatus) {
        m_certStatus->setGenerateButtonText(text);
    }
}

void CertificateBoxWidget::showDownloadProgress(bool show)
{
    if (m_generateProgress) {
        m_generateProgress->showProgress(show);
    }
}

void CertificateBoxWidget::setDownloadStatus(const QString &text)
{
    if (m_generateProgress) {
        m_generateProgress->setStatusText(text);
    }
}

void CertificateBoxWidget::setDownloadProgress(int value)
{
    if (m_generateProgress) {
        m_generateProgress->setValue(value);
    }
}

void CertificateBoxWidget::onRenewClicked()
{
    emit generateClicked();
}