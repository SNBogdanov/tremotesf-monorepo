// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_SCRIPTS_SCRIPTRUNDIALOG_H
#define TREMOTESF_UI_SCRIPTS_SCRIPTRUNDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "rpc/scriptrunner.h"

class QLabel;

namespace tremotesf {
    class ScriptParametersForm;

    class ScriptRunDialog final : public QDialog {
        Q_OBJECT

    public:
        ScriptRunDialog(
            const ScriptInfo& script,
            const ScriptRunnerData& runnerData,
            const ScriptSelection& selection,
            QWidget* parent = nullptr
        );

        [[nodiscard]] QJsonObject parameters() const { return mParameters; }

        void accept() override;

    private:
        ScriptParametersForm* mForm{};
        QJsonObject mParameters{};
        QString mScriptId{};
    };
}

#endif // TREMOTESF_UI_SCRIPTS_SCRIPTRUNDIALOG_H
