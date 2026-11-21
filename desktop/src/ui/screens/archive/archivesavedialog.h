// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ARCHIVESAVEDIALOG_H
#define TREMOTESF_ARCHIVESAVEDIALOG_H

#include <vector>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;

namespace tremotesf {
    class Rpc;

    class ArchiveSaveDialog final : public QDialog {
        Q_OBJECT

    public:
        ArchiveSaveDialog(Rpc* rpc, std::vector<int> torrentIds, QWidget* parent = nullptr);
        ~ArchiveSaveDialog() override = default;
        Q_DISABLE_COPY_MOVE(ArchiveSaveDialog)

    private:
        void loadArchives();
        void save();

        Rpc* mRpc;
        std::vector<int> mTorrentIds;
        QComboBox* mArchiveComboBox{};
        QCheckBox* mOverwriteCheckBox{};
        QCheckBox* mDeleteLocalDataCheckBox{};
        QDialogButtonBox* mButtonBox{};
    };
}

#endif // TREMOTESF_ARCHIVESAVEDIALOG_H
