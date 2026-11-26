// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_SCRIPTS_SCRIPTPARAMETERSFORM_H
#define TREMOTESF_UI_SCRIPTS_SCRIPTPARAMETERSFORM_H

#include <optional>

#include <QJsonObject>
#include <QWidget>

#include "rpc/scriptrunner.h"

class QLineEdit;
class QListWidget;

namespace tremotesf {
    class ScriptParametersForm final : public QWidget {
        Q_OBJECT

    public:
        explicit ScriptParametersForm(
            const ScriptInfo& script,
            const ScriptRunnerData& runnerData,
            QWidget* parent = nullptr,
            ScriptParameterStorage storage = ScriptParameterStorage::Runtime
        );

        void setValues(const QJsonObject& values);

        [[nodiscard]] std::optional<QJsonObject> parameters(QString* error) const;

    private:
        struct Input {
            ScriptParameter parameter;
            QWidget* editor{};
            QListWidget* list{};
            QLineEdit* lineEdit{};
        };

        ScriptParameterStorage mStorage;
        QList<Input> mInputs;
    };
}

#endif // TREMOTESF_UI_SCRIPTS_SCRIPTPARAMETERSFORM_H
