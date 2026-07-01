#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>

class QPushButton;

class ThemeManager
{
public:
    static void applyTheme(bool dark, QPushButton *themeToggleButton);

private:
    static QString lightStyleSheet();
    static QString darkStyleSheet();
    static QString accentColor();
};

#endif // THEMEMANAGER_H