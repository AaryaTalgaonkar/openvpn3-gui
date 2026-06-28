#ifndef CERTIFICATEBOXWIDGET_H
#define CERTIFICATEBOXWIDGET_H

#include <QFrame>
#include <QDateTime>

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;

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

    Ui::CertificateBox *ui() { return m_ui; }

signals:
    void generateClicked();

private:
    Ui::CertificateBox *m_ui;
    QDateTime m_certEffectiveDate;
    QDateTime m_certExpiryDate;
};

#endif // CERTIFICATEBOXWIDGET_H