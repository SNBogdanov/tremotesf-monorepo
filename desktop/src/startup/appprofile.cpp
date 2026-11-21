// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appprofile.h"

#include <utility>

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString currentProfileName{};
    }

    bool isValidProfileName(const QString& name) {
        static const QRegularExpression allowedCharacters("^[A-Za-z0-9._-]{1,64}$"_L1);
        return allowedCharacters.match(name).hasMatch() && name != "."_L1 && name != ".."_L1;
    }

    const QString& profileName() { return currentProfileName; }

    void setProfileName(QString name) { currentProfileName = std::move(name); }
}
