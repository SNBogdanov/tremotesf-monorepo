// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptrundialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "settings.h"
#include "scriptparametersform.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QJsonObject loadScriptParametersHistory() {
            const auto document = QJsonDocument::fromJson(Settings::instance()->get_scriptRunParameters().toUtf8());
            return document.isObject() ? document.object() : QJsonObject{};
        }

        void saveScriptParametersHistory(const QString& scriptId, const QJsonObject& parameters) {
            auto history = loadScriptParametersHistory();
            history.insert(scriptId, parameters);
            Settings::instance()->set_scriptRunParameters(
                QString::fromUtf8(QJsonDocument(history).toJson(QJsonDocument::Compact))
            );
        }
    }

    ScriptRunDialog::ScriptRunDialog(
        const ScriptInfo& script, const ScriptRunnerData& runnerData, const ScriptSelection& selection, QWidget* parent
    )
        : QDialog(parent), mScriptId(script.id) {
        setWindowTitle(script.name);

        auto* layout = new QVBoxLayout(this);

        if (!script.description.isEmpty()) {
            auto* descriptionLabel = new QLabel(script.description, this);
            descriptionLabel->setWordWrap(true);
            layout->addWidget(descriptionLabel);
        }

        QString selectionText{};
        if (script.hasContext(ScriptContext::System)) {
            selectionText = qApp->translate("tremotesf", "No torrents or files are selected");
        } else if (selection.files.isEmpty()) {
            selectionText = qApp->translate("tremotesf", "Selected torrents: %1").arg(selection.torrentHashes.size());
        } else {
            selectionText = qApp->translate("tremotesf", "Selected files: %1").arg(selection.files.size());
        }
        auto* selectionLabel = new QLabel(selectionText, this);
        selectionLabel->setWordWrap(true);
        layout->addWidget(selectionLabel);

        if (script.confirm) {
            auto* confirmLabel =
                new QLabel(qApp->translate("tremotesf", "This script will be executed on the server. Continue?"), this);
            confirmLabel->setWordWrap(true);
            layout->addWidget(confirmLabel);
        }

        mForm = new ScriptParametersForm(script, runnerData, this);
        layout->addWidget(mForm);

        if (const auto saved = loadScriptParametersHistory().value(mScriptId); saved.isObject()) {
            mForm->setValues(saved.toObject());
        }

        auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText(qApp->translate("tremotesf", "Run"));
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &ScriptRunDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &ScriptRunDialog::reject);
        layout->addWidget(buttonBox);
    }

    void ScriptRunDialog::accept() {
        QString error{};
        auto parameters = mForm->parameters(&error);
        if (!parameters.has_value()) {
            QMessageBox::warning(
                this,
                qApp->translate("tremotesf", "Invalid script parameters"),
                error.isEmpty() ? qApp->translate("tremotesf", "Invalid script parameters") : error
            );
            return;
        }
        mParameters = std::move(*parameters);
        saveScriptParametersHistory(mScriptId, mParameters);
        QDialog::accept();
    }
}
