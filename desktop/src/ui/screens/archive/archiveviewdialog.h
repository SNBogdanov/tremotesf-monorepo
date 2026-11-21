// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ARCHIVEVIEWDIALOG_H
#define TREMOTESF_ARCHIVEVIEWDIALOG_H

#include <vector>

#include <QDialog>
#include <QString>

class QTreeWidget;

namespace tremotesf {
    struct ArchiveViewFile {
        QString path;
        qint64 size{};
        bool wanted{true};
    };

    class ArchiveViewDialog final : public QDialog {
        Q_OBJECT

    public:
        ArchiveViewDialog(
            QString torrentName,
            QString downloadDirectory,
            std::vector<ArchiveViewFile> files,
            QWidget* parent = nullptr
        );
        ~ArchiveViewDialog() override = default;
        Q_DISABLE_COPY_MOVE(ArchiveViewDialog)

    private:
        QTreeWidget* mTree{};
    };
}

#endif // TREMOTESF_ARCHIVEVIEWDIALOG_H
