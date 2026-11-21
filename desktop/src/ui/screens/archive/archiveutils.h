// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ARCHIVEUTILS_H
#define TREMOTESF_ARCHIVEUTILS_H

#include <QString>

namespace tremotesf {
    /// Checks that archive name can be safely passed to torrent_archive RPC method
    [[nodiscard]] bool isValidArchiveName(const QString& name);
}

#endif // TREMOTESF_ARCHIVEUTILS_H
