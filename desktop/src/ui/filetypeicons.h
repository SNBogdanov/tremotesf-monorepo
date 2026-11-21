// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_FILETYPEICONS_H
#define TREMOTESF_FILETYPEICONS_H

#include <QIcon>
#include <QString>

namespace tremotesf::filetypeicons {
    /// Returns icon for a file based on its extension, with fallback to MIME type database
    QIcon fileIcon(const QString& fileName);

    /// Returns folder icon from the application icon theme
    const QIcon& folderIcon();
}

#endif // TREMOTESF_FILETYPEICONS_H
