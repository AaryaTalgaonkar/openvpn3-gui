#ifndef USERINFOWIDGET_H
#define USERINFOWIDGET_H

#include <QWidget>

class QLabel;

#include "ui_userinfo.h"

class UserInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserInfoWidget(QWidget *parent = nullptr);
    ~UserInfoWidget();

    void setUserInfo(const QString &cn, const QString &org, const QString &email);
    void clear();

private:
    Ui::UserInfo *m_ui;
};

#endif // USERINFOWIDGET_H