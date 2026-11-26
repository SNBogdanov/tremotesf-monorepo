// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTPROPERTIESDIALOG_H
#define TREMOTESF_TORRENTPROPERTIESDIALOG_H

#include <QDialog>
#include <QFont>

#include "ui/savewindowstatedispatcher.h"

class QEvent;

namespace tremotesf {
    class Rpc;
    class Torrent;
    class TorrentPropertiesWidget;

    class TorrentPropertiesDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit TorrentPropertiesDialog(Torrent* torrent, Rpc* rpc, QWidget* parent = nullptr);
        ~TorrentPropertiesDialog() override = default;
        Q_DISABLE_COPY_MOVE(TorrentPropertiesDialog)

    private:
        void saveState();
        void updateFontSize();

    protected:
        bool event(QEvent* event) override;

    private:
        TorrentPropertiesWidget* mTorrentPropertiesWidget;
        QFont mBaseFont{};

        SaveWindowStateHandler mSaveStateHandler{this, [this] { saveState(); }};
    };
}

#endif // TREMOTESF_TORRENTPROPERTIESDIALOG_H
