#include "thememanager.h"

#include <QApplication>
#include <QPushButton>

QString ThemeManager::accentColor()
{
    return QStringLiteral("#c21717");
}

QString ThemeManager::darkStyleSheet()
{
    const QString accent = accentColor();
    return QStringLiteral(R"(
        QMainWindow, QFrame, QLabel, QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QPushButton, QStatusBar, QDialog, QMessageBox { background-color: #0e0f10; color: #e6e6e6; }
        QLabel { color: #e6e6e6; }
        QLineEdit, QPlainTextEdit, QTextEdit, QComboBox { background-color: #1e1e1e; color: #e6e6e6; border: 1px solid #2b2b2b; border-radius: 4px; padding: 4px; }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid %1; }
        QPushButton { background-color: #1b1b1b; color: #e6e6e6; border: 1px solid #2f2f2f; border-radius: 6px; padding: 6px 10px; }
        QPushButton:hover { background-color: rgba(194,23,23,0.10); }
        QPushButton:pressed { background-color: rgba(194,23,23,0.18); }
        QStatusBar { background: transparent; color: #bdbdbd; }
        QDialog, QMessageBox { background-color: #121212; color: #e6e6e6; }
        QMessageBox QPushButton { background-color: #2b2b2b; color: #e6e6e6; border: 1px solid #363636; border-radius: 6px; padding: 4px 8px; }
    )").arg(accent);
}

QString ThemeManager::lightStyleSheet()
{
    const QString accent = accentColor();
    return QStringLiteral(R"(
        QMainWindow, QFrame, QLabel, QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QPushButton, QStatusBar, QDialog, QMessageBox { background-color: #ffffff; color: #1f1f1f; }
        QLabel { color: #1f1f1f; }
        QLineEdit, QPlainTextEdit, QTextEdit, QComboBox { background-color: #ffffff; color: #1f1f1f; border: 1px solid #dedede; border-radius: 4px; padding: 4px; }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid %1; }
        QPushButton { background-color: #f5f5f5; color: #1f1f1f; border: 1px solid #e6e6e6; border-radius: 6px; padding: 6px 10px; }
        QPushButton:hover { background-color: rgba(194,23,23,0.08); }
        QStatusBar { background: transparent; color: #666666; }
        QDialog, QMessageBox { background-color: #ffffff; color: #1f1f1f; }
        QMessageBox QPushButton { background-color: #ffffff; color: #1f1f1f; border: 1px solid #cccccc; border-radius: 6px; padding: 4px 8px; }
    )").arg(accent);
}

void ThemeManager::applyTheme(bool dark, QPushButton *themeToggleButton)
{
    if (dark) {
        qApp->setStyleSheet(darkStyleSheet());
        if (themeToggleButton) {
            themeToggleButton->setText(QStringLiteral("☀"));
        }
    } else {
        qApp->setStyleSheet(lightStyleSheet());
        if (themeToggleButton) {
            themeToggleButton->setText(QStringLiteral("☾"));
        }
    }
}