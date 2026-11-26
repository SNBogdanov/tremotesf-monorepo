// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ARCHIVESAVEDIALOG_H
#define TREMOTESF_ARCHIVESAVEDIALOG_H

#include <vector>

#include <QDialog>
#include <QPair>
#include <QPointer>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;

namespace tremotesf {
    class Rpc;
    class Torrent;

    class ArchiveSaveDialog final : public QDialog {
        Q_OBJECT

    public:
        ArchiveSaveDialog(Rpc* rpc, QVector<Torrent*> torrents, QWidget* parent = nullptr);
        ~ArchiveSaveDialog() override;
        Q_DISABLE_COPY_MOVE(ArchiveSaveDialog)

    private:
        void loadArchives();
        void save();
        void saveManual(const QString& archiveName);
        void startWhisparrAnalysis();
        void analyzeNextTorrent();
        void finishAnalysis();
        void confirmAndSave(const QVector<QPair<QString, QVector<int>>>& groups);
        void performSave(const QVector<QPair<QString, QVector<int>>>& groups);
        void updateUiEnabled(bool enabled);
        [[nodiscard]] QString analysisTitle(const Torrent* torrent) const;

        Rpc* mRpc;
        QVector<QPointer<Torrent>> mTorrents;
        std::vector<int> mTorrentIds;
        QComboBox* mArchiveComboBox{};
        QCheckBox* mOverwriteCheckBox{};
        QCheckBox* mDeleteLocalDataCheckBox{};
        QCheckBox* mUseWhisparrCheckBox{};
        QLabel* mStatusLabel{};
        QProgressBar* mProgress{};
        QDialogButtonBox* mButtonBox{};
        QNetworkAccessManager* mNetwork{};
        QNetworkReply* mReply{};
        qsizetype mAnalysisIndex{};
        QStringList mArchiveNames{};
        QStringList mAnalysisErrors{};
        bool mWhisparrConfigured{};
    };
}

#endif // TREMOTESF_ARCHIVESAVEDIALOG_H
