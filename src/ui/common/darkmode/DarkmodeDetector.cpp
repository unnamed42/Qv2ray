#include "DarkmodeDetector.hpp"

#include "base/Qv2rayBase.hpp"

#include <QApplication>
#include <QStyle>
#if defined(Q_OS_LINUX) && __has_include(<QDBusInterface>)
#include <QDBusInterface>
#elif defined(Q_OS_WIN32)
#include <QSettings>
#elif defined(Q_OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif

namespace Qv2ray::components::darkmode
{
    // Referenced from github.com/keepassxreboot/keepassxc. Licensed under GPL2/3.
    // Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
    bool isDarkMode()
    {
#if defined(Q_OS_WIN32)
        QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", QSettings::NativeFormat);
        return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#elif defined(Q_OS_MAC)
        bool isDark = false;

        CFStringRef uiStyleKey = CFSTR("AppleInterfaceStyle");
        CFStringRef uiStyle = nullptr;
        CFStringRef darkUiStyle = CFSTR("Dark");

        if (uiStyle = (CFStringRef) CFPreferencesCopyAppValue(uiStyleKey, kCFPreferencesCurrentApplication); uiStyle)
        {
            isDark = (kCFCompareEqualTo == CFStringCompare(uiStyle, darkUiStyle, 0));
            CFRelease(uiStyle);
        }

        return isDark;
#elif defined(Q_OS_LINUX) && __has_include(<QDBusInterface>)
        // see https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html
        QDBusInterface xdg("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings");
        auto reply = xdg.callWithArgumentList(QDBus::BlockWithGui, "ReadOne", { QString("org.freedesktop.appearance"), QString("color-scheme") });
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().empty())
            return false;
        auto result = qvariant_cast<QDBusVariant>(reply.arguments().first());
        return result.variant().toInt() == 1;
#endif

        if (!qApp || !qApp->style())
        {
            return false;
        }
        return qApp->style()->standardPalette().color(QPalette::Window).toHsl().lightness() < 110;
    }

} // namespace Qv2ray::components::darkmode
