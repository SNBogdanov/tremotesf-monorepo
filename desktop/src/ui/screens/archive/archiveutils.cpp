// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archiveutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    bool isValidArchiveName(const QString& name) {
        if (name.isEmpty() || name.size() > 128 || name == QStringLiteral(".") || name == QStringLiteral("..")) {
            return false;
        }
        for (const QChar c : name) {
            if (c.unicode() < 32
                || c == u'/'
                || c == u'\\'
                || c == u':'
                || c == u'*'
                || c == u'?'
                || c == u'"'
                || c == u'<'
                || c == u'>'
                || c == u'|') {
                return false;
            }
        }
        return true;
    }
}
