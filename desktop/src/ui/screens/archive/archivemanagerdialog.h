// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ARCHIVEMANAGERDIALOG_H
#define TREMOTESF_ARCHIVEMANAGERDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace tremotesf {
    class Rpc;

    class ArchiveManagerDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ArchiveManagerDialog(Rpc* rpc, QWidget* parent = nullptr);
        ~ArchiveManagerDialog() override = default;
        Q_DISABLE_COPY_MOVE(ArchiveManagerDialog)

    private:
        void loadArchives();
        void loadPage();
        void refresh();
        void createArchive();
        void deleteArchive();
        void restoreSelected();
        void viewSelected();
        void deleteSelected();
        void
        deleteEntries(const QList<QPair<QString, QString>>& entries, int index, int deleted, const QStringList& errors);
        void updateControls();

        [[nodiscard]] QString currentArchive() const;
        [[nodiscard]] QList<QPair<QString, QString>> selectedEntries() const;
        void showError(const QString& message);

        Rpc* mRpc;
        QComboBox* mArchiveComboBox{};
        QCheckBox* mVerifyCheckBox{};
        QTableWidget* mTable{};
        QPushButton* mDeleteArchiveButton{};
        QPushButton* mRestoreButton{};
        QPushButton* mViewButton{};
        QPushButton* mDeleteEntryButton{};
        QPushButton* mPreviousPageButton{};
        QPushButton* mNextPageButton{};
        QLabel* mCountLabel{};
        QLabel* mPageLabel{};

        QString mPendingArchiveSelection{};
        int mOffset{};
        int mTotal{};

        static constexpr int pageSize = 100;
    };
}

#endif // TREMOTESF_ARCHIVEMANAGERDIALOG_H
