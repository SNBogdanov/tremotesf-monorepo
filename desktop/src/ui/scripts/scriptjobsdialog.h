// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_SCRIPTS_SCRIPTJOBSDIALOG_H
#define TREMOTESF_UI_SCRIPTS_SCRIPTJOBSDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;

namespace tremotesf {
    class ScriptJobsManager;

    class ScriptJobsDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ScriptJobsDialog(ScriptJobsManager* manager, QWidget* parent = nullptr);

        void showJob(const QString& jobId);

    private:
        void refresh();
        void updateDetails();
        [[nodiscard]] QString selectedJobId() const;

        ScriptJobsManager* mManager;
        QTableWidget* mTable{};
        QLabel* mStatusLabel{};
        QTextEdit* mStdoutText{};
        QTextEdit* mStderrText{};
        QPushButton* mCancelButton{};
    };
}

#endif // TREMOTESF_UI_SCRIPTS_SCRIPTJOBSDIALOG_H
