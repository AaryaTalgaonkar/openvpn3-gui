#ifndef CONNECTIONLOGSDIALOG_H
#define CONNECTIONLOGSDIALOG_H

#include <QDialog>
#include <QStringList>

class ConnectionLogsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionLogsDialog(const QStringList &logs, QWidget *parent = nullptr);
};

#endif // CONNECTIONLOGSDIALOG_H