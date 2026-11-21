// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ARCHIVERESTOREDIALOG_H
#define TREMOTESF_ARCHIVERESTOREDIALOG_H

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;
class QPushButton;

namespace tremotesf {
    class ArchiveRestoreDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ArchiveRestoreDialog(QWidget* parent = nullptr);
        ~ArchiveRestoreDialog() override = default;
        Q_DISABLE_COPY_MOVE(ArchiveRestoreDialog)

        [[nodiscard]] bool changeDownloadDirectory() const;
        [[nodiscard]] QString downloadDirectory() const;
        [[nodiscard]] bool removeFromArchive() const;

    private:
        QCheckBox* mChangePathCheckBox{};
        QLineEdit* mPathEdit{};
        QPushButton* mBrowseButton{};
        QCheckBox* mRemoveCheckBox{};
    };
}

#endif // TREMOTESF_ARCHIVERESTOREDIALOG_H
