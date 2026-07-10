#pragma once

#include <QMessageBox>
#include <QPushButton>
#include <QStyle>

class CustomMessageBox : public QMessageBox
{
public:
    using QMessageBox::QMessageBox;

    static StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                  StandardButtons buttons = Ok, StandardButton defaultButton = NoButton)
    {
        return showMessage(parent, Warning, title, text, buttons, defaultButton);
    }

    static StandardButton critical(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = Ok, StandardButton defaultButton = NoButton)
    {
        return showMessage(parent, Critical, title, text, buttons, defaultButton);
    }

    static StandardButton information(QWidget *parent, const QString &title, const QString &text,
                                       StandardButtons buttons = Ok, StandardButton defaultButton = NoButton)
    {
        return showMessage(parent, Information, title, text, buttons, defaultButton);
    }

private:
    static StandardButton showMessage(QWidget *parent, Icon icon, const QString &title, const QString &text,
                                      StandardButtons buttons, StandardButton defaultButton)
    {
        CustomMessageBox box(parent);
        box.setIcon(icon);
        box.setWindowTitle(title);
        box.setText(text);
        box.setStandardButtons(buttons);
        box.setWindowFlags(box.windowFlags() & ~(Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint));
        if (defaultButton != NoButton) {
            box.setDefaultButton(defaultButton);
        }

        for (QPushButton *button : box.findChildren<QPushButton *>()) {
            if (button) {
                button->setCursor(Qt::PointingHandCursor);
            }
        }

        const QStyle::StandardPixmap standardPixmap =
            icon == Warning ? QStyle::SP_MessageBoxWarning
            : icon == Critical ? QStyle::SP_MessageBoxCritical
            : QStyle::SP_MessageBoxInformation;

        box.setIconPixmap(box.style()->standardIcon(standardPixmap).pixmap(24, 24));
        return static_cast<StandardButton>(box.exec());
    }
};

#define QMessageBox CustomMessageBox