// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ipcserver_dbus.h"

#include "startup/appprofile.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    QString IpcServerDbus::serviceName() {
        QString name("org.equeim.Tremotesf"_L1);
        const auto& profile = profileName();
        if (!profile.isEmpty()) {
            // D-Bus object name elements must not start with a digit
            QString sanitizedProfile(profile);
            sanitizedProfile.replace(QLatin1Char('.'), QLatin1Char('_'));
            name += ".Profile_"_L1;
            name += sanitizedProfile;
        }
        return name;
    }

    IpcServer* IpcServer::createInstance(QObject* parent) { return new IpcServerDbus(parent); }
}
