// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTFILESVIEW_H
#define TREMOTESF_TORRENTFILESVIEW_H

#include <functional>

#include <QFont>
#include <QTreeView>

class QEvent;
class QKeyEvent;

namespace tremotesf {
    class BaseTorrentFilesModel;
    class LocalTorrentFilesModel;
    class Rpc;
    class TorrentFilesModel;
    class TorrentFilesProxyModel;

    class TorrentFilesView final : public QTreeView {
        Q_OBJECT

    public:
        explicit TorrentFilesView(LocalTorrentFilesModel* model, Rpc* rpc, QWidget* parent = nullptr);
        explicit TorrentFilesView(TorrentFilesModel* model, Rpc* rpc, QWidget* parent = nullptr);
        ~TorrentFilesView() override = default;
        Q_DISABLE_COPY_MOVE(TorrentFilesView)

        static void showFileRenameDialog(
            const QString& fileName, QWidget* parent, const std::function<void(const QString&)>& onAccepted
        );

        void saveState();

        [[nodiscard]] TorrentFilesProxyModel* proxyModel() const;

    protected:
        bool event(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        void init();
        void updateFontSize();
        void onModelReset();
        void showContextMenu(QPoint pos);

        bool mLocalFile;
        BaseTorrentFilesModel* mModel;
        TorrentFilesProxyModel* mProxyModel;
        Rpc* mRpc;
        QFont mBaseFont{};
    };
}

#endif // TREMOTESF_TORRENTFILESVIEW_H
