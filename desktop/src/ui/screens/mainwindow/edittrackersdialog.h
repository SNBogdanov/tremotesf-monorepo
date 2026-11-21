// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_EDITTRACKERSDIALOG_H
#define TREMOTESF_EDITTRACKERSDIALOG_H

#include <vector>

#include <QDialog>

class QListWidget;
class QPushButton;

namespace tremotesf {
    class Rpc;
    class Torrent;

    class EditTrackersDialog final : public QDialog {
        Q_OBJECT

    public:
        EditTrackersDialog(Rpc* rpc, std::vector<Torrent*> torrents, QWidget* parent = nullptr);
        ~EditTrackersDialog() override = default;
        Q_DISABLE_COPY_MOVE(EditTrackersDialog)

    private:
        void rebuildList();
        void addTracker();
        void editTracker();
        void removeTracker();
        void updateButtons();
        void reannounceAll();

        Rpc* mRpc;
        std::vector<Torrent*> mTorrents;
        QListWidget* mList{};
        QPushButton* mAddButton{};
        QPushButton* mEditButton{};
        QPushButton* mRemoveButton{};
    };
}

#endif // TREMOTESF_EDITTRACKERSDIALOG_H
