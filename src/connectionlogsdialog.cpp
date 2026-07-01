#include "connectionlogsdialog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

ConnectionLogsDialog::ConnectionLogsDialog(const QStringList &logs, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Connection Logs"));
    setModal(true);
    setFixedSize(300, 400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *viewer = new QPlainTextEdit(this);
    viewer->setReadOnly(true);
    viewer->setPlaceholderText(QStringLiteral("Logs from the most recent VPN socket connection will appear here."));
    viewer->setPlainText(logs.join(QStringLiteral("\n\n")));
    QFont logFont;
    logFont.setFamily(QStringLiteral("Courier New"));
    logFont.setPointSize(10);
    viewer->setFont(logFont);
    // Wrap long lines to the widget width to avoid horizontal scrolling
    viewer->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    viewer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Modern thin scrollbar styling for the log viewer
    const QString logScrollStyle = QStringLiteral(
        "QPlainTextEdit { font-family: 'Courier New'; font-size: 10px; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0px 0px 0px 0px; }"
        "QScrollBar::handle:vertical { background: rgba(0,0,0,0.25); min-height: 20px; border-radius: 5px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
    );
    viewer->setStyleSheet(logScrollStyle);
    layout->addWidget(viewer, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);

    auto *saveButton = new QPushButton(QStringLiteral("Save Logs"), this);
    auto *closeButton = new QPushButton(QStringLiteral("Close"), this);
    saveButton->setCursor(Qt::PointingHandCursor);
    closeButton->setCursor(Qt::PointingHandCursor);
    buttonRow->addWidget(saveButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(saveButton, &QPushButton::clicked, this, [this, viewer]() {
        const QString defaultName = QStringLiteral("vpn-logs-%1.txt")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        const QString initialPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(defaultName);
        const QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("Save Logs"), initialPath, QStringLiteral("Text Files (*.txt);;All Files (*)"));
        if (filePath.isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, QStringLiteral("Save Failed"), QStringLiteral("Could not write the selected file."));
            return;
        }

        file.write(viewer->toPlainText().toUtf8());
    });

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}