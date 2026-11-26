// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_WHISPARR_WHISPARRRENAMEDIALOG_H
#define TREMOTESF_UI_WHISPARR_WHISPARRRENAMEDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QString>
#include <QVector>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QUrl;

namespace tremotesf {
    class Torrent;

    class WhisparrRenameDialog final : public QDialog {
        Q_OBJECT

    public:
        WhisparrRenameDialog(QVector<Torrent*> torrents, QString baseUrl, QString apiKey, QWidget* parent = nullptr);
        ~WhisparrRenameDialog() override;
        Q_DISABLE_COPY_MOVE(WhisparrRenameDialog)

        void accept() override;

    private:
        struct Entry {
            QPointer<Torrent> torrent{};
            QString torrentName{};
            QString oldPath{};
            QString fullPath{};
            QString newPath{};
            QString error{};
            bool parsed{};
            bool changed{};

            [[nodiscard]] QString targetFullPath() const {
                return fullPath.left(fullPath.size() - oldPath.size()) + newPath;
            }
        };

        void collectFiles();
        void startNextRequest();
        void finishParsing();
        void updateTableRow(int row);
        void updateStatus();
        [[nodiscard]] QUrl parseUrl(const Entry& entry) const;

        QVector<Torrent*> mTorrents{};
        QString mBaseUrl{};
        QString mApiKey{};
        QVector<Entry> mEntries{};
        QNetworkAccessManager* mNetwork{};
        QNetworkReply* mReply{};
        qsizetype mCurrentIndex{};
        int mNewNameColumn{1};
        QTableWidget* mTable{};
        QLabel* mStatusLabel{};
        QProgressBar* mProgress{};
        QPushButton* mRenameButton{};
    };
}

#endif // TREMOTESF_UI_WHISPARR_WHISPARRRENAMEDIALOG_H
