#ifndef CERTIFICATEBOXWIDGET_H
#define CERTIFICATEBOXWIDGET_H

#include <QFrame>
#include <QDateTime>
#include <QSslCertificate>

class QLabel;
class QPushButton;
class QStackedWidget;

class UserInfoWidget;
class StatusWidget;
class ValidityTimelineWidget;
class GenerateProgressWidget;

#include "ui_certificatebox.h"

class CertificateBoxWidget : public QFrame
{
    Q_OBJECT

public:
    explicit CertificateBoxWidget(QWidget *parent = nullptr);
    ~CertificateBoxWidget();

    void showNoCertMode();
    void showCertInfo(const QString &cn, const QString &org, const QString &email,
                      const QDateTime &effectiveDate, const QDateTime &expiryDate);
    void updateValidityDisplay(const QDateTime &now);
    void setGenerateEnabled(bool enabled);
    void setGenerateButtonText(const QString &text);

    void showDownloadProgress(bool show);
    void setDownloadStatus(const QString &text);
    void setDownloadProgress(int value);

    void loadFromOvpnFile(const QString &ovpnPath);

    Ui::CertificateBox *ui() { return m_ui; }

signals:
    void generateClicked();
    void certificateParsed();

private slots:
    void onRenewClicked();

private:
    Ui::CertificateBox *m_ui;

    UserInfoWidget *m_userInfo;
    StatusWidget *m_noCertStatus;
    StatusWidget *m_certStatus;
    ValidityTimelineWidget *m_validityTimeline;
    GenerateProgressWidget *m_generateProgress;

    QDateTime m_certEffectiveDate;
    QDateTime m_certExpiryDate;
};

#endif // CERTIFICATEBOXWIDGET_H