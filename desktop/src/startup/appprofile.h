// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_APPPROFILE_H
#define TREMOTESF_APPPROFILE_H

#include <QString>

namespace tremotesf {
    /// Returns true if the name can be used as a profile name
    [[nodiscard]] bool isValidProfileName(const QString& name);

    /// Returns the profile name passed on the command line, or an empty string for the default profile
    [[nodiscard]] const QString& profileName();

    /// Must be called before any settings are created
    void setProfileName(QString name);
}

#endif // TREMOTESF_APPPROFILE_H
