#include "userinfowidget.h"

UserInfoWidget::UserInfoWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::UserInfo)
{
    m_ui->setupUi(this);
}

UserInfoWidget::~UserInfoWidget()
{
    delete m_ui;
}

void UserInfoWidget::setUserInfo(const QString &cn, const QString &org, const QString &email)
{
    m_ui->certUsernameLabel->setText(cn);
    m_ui->certInstituteLabel->setText(org);
    m_ui->certEmailLabel->setText(email);
}

void UserInfoWidget::clear()
{
    m_ui->certUsernameLabel->setText(QStringLiteral("—"));
    m_ui->certInstituteLabel->setText(QStringLiteral("—"));
    m_ui->certEmailLabel->setText(QStringLiteral("—"));
}