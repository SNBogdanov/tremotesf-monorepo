// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptparameterspage.h"

#include <ranges>

#include <QApplication>
#include <QFrame>
#include <QGroupBox>
#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "ui/scripts/scriptparametersform.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString settingsErrorMessage(const QString& result) {
            if (result == "script settings revision conflict"_L1) {
                return qApp->translate(
                    "tremotesf",
                    "Script settings were changed by another client. Reload the page and try again."
                );
            }
            if (result == "script settings unavailable"_L1) {
                return qApp->translate("tremotesf", "Script settings are unavailable on the server");
            }
            if (result == "script settings incomplete"_L1) {
                return qApp->translate("tremotesf", "Fill in all required script parameters");
            }
            if (result == "script runner is disabled"_L1) {
                return qApp->translate("tremotesf", "Script runner is disabled on the server");
            }
            if (result.isEmpty()) {
                return qApp->translate("tremotesf", "Failed to save script settings");
            }
            return result;
        }
    }

    ScriptParametersPage::ScriptParametersPage(Rpc* rpc, QWidget* parent) : QWidget(parent), mRpc(rpc) {
        auto* layout = new QVBoxLayout(this);
        mStatusLabel = new QLabel(this);
        mStatusLabel->setWordWrap(true);
        layout->addWidget(mStatusLabel);

        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(scrollArea);
        mFormsLayout = new QVBoxLayout(content);
        scrollArea->setWidget(content);
        layout->addWidget(scrollArea);
    }

    bool ScriptParametersPage::isAvailable() const {
        const auto& data = mRpc->serverSettings()->data();
        if (!data.hasScriptRunner() || !data.hasScriptRunnerSettings() || !data.scriptRunner.has_value()) {
            return false;
        }
        return std::ranges::any_of(data.scriptRunner->scripts, [](const ScriptInfo& script) {
            return script.hasHostParameters();
        });
    }

    void ScriptParametersPage::load() {
        clearForms();
        mRevision.clear();

        if (!isAvailable()) {
            setStatus(qApp->translate("tremotesf", "Script parameters are not available"));
            return;
        }

        setStatus(qApp->translate("tremotesf", "Loading..."));
        mRpc->requestScriptSettingsGet({}, this, [this](ScriptResponse response) {
            if (!response.success) {
                setStatus(settingsErrorMessage(response.result));
                return;
            }

            const auto arguments = response.arguments;
            mRevision = arguments.value("revision"_L1).toString();

            QHash<QString, QJsonObject> settingsById{};
            for (const auto& scriptValue : arguments.value("scripts"_L1).toArray()) {
                if (!scriptValue.isObject()) {
                    continue;
                }
                const auto scriptJson = scriptValue.toObject();
                settingsById.insert(scriptJson.value("id"_L1).toString(), scriptJson);
            }

            const auto runnerData = *mRpc->serverSettings()->data().scriptRunner;
            for (const auto& script : runnerData.scripts) {
                if (!script.hasHostParameters()) {
                    continue;
                }

                const auto settingsJson = settingsById.value(script.id);
                const auto parameters = settingsJson.value("parameters"_L1).toObject();

                auto* groupBox = new QGroupBox(script.name, this);
                auto* groupLayout = new QVBoxLayout(groupBox);

                auto* form = new ScriptParametersForm(script, runnerData, groupBox, ScriptParameterStorage::Host);
                form->setValues(parameters);
                groupLayout->addWidget(form);

                const auto missing = settingsJson.value("missing"_L1).toArray();
                if (!settingsJson.value("ready"_L1).toBool(true) || !missing.isEmpty()) {
                    QStringList missingNames{};
                    for (const auto& missingValue : missing) {
                        const auto key = missingValue.toString();
                        const auto parameter = std::ranges::find(script.parameters, key, &ScriptParameter::key);
                        missingNames.push_back(parameter == script.parameters.end() ? key : parameter->name);
                    }
                    auto* warning = new QLabel(
                        missingNames.isEmpty() ? qApp->translate("tremotesf", "Required parameters are missing")
                                               : qApp->translate("tremotesf", "Required parameters are missing: %1")
                                                     .arg(missingNames.join(u", "_s)),
                        groupBox
                    );
                    warning->setWordWrap(true);
                    groupLayout->addWidget(warning);
                    mForms.push_back(
                        {.id = script.id, .name = script.name, .form = form, .warning = warning, .initial = parameters}
                    );
                } else {
                    mForms.push_back({.id = script.id, .name = script.name, .form = form, .initial = parameters});
                }

                mFormsLayout->addWidget(groupBox);
            }

            mFormsLayout->addStretch();
            setStatus(mForms.isEmpty() ? qApp->translate("tremotesf", "No script parameters") : QString());
        });
    }

    bool ScriptParametersPage::hasChanges() const {
        for (const auto& scriptForm : mForms) {
            QString error{};
            const auto parameters = scriptForm.form->parameters(&error);
            if (!parameters.has_value() || *parameters != scriptForm.initial) {
                return true;
            }
        }
        return false;
    }

    void ScriptParametersPage::save(std::function<void(bool success, const QString& error)> callback) {
        QJsonArray scripts{};
        for (const auto& scriptForm : mForms) {
            QString error{};
            const auto parameters = scriptForm.form->parameters(&error);
            if (!parameters.has_value()) {
                callback(false, error.isEmpty() ? qApp->translate("tremotesf", "Invalid script parameters") : error);
                return;
            }
            if (*parameters != scriptForm.initial) {
                scripts.append(QJsonObject{{"id"_L1, scriptForm.id}, {"parameters"_L1, *parameters}});
            }
        }

        if (scripts.isEmpty()) {
            callback(true, {});
            return;
        }

        QJsonObject arguments{{"scripts"_L1, scripts}};
        if (!mRevision.isEmpty()) {
            arguments.insert("expected_revision"_L1, mRevision);
        }

        mRpc->requestScriptSettingsSet(
            arguments,
            this,
            [this, callback = std::move(callback)](ScriptResponse response) {
                if (!response.success) {
                    callback(false, settingsErrorMessage(response.result));
                    return;
                }

                mRevision = response.arguments.value("revision"_L1).toString();
                QHash<QString, QJsonObject> settingsById{};
                for (const auto& scriptValue : response.arguments.value("scripts"_L1).toArray()) {
                    if (scriptValue.isObject()) {
                        const auto scriptJson = scriptValue.toObject();
                        settingsById.insert(scriptJson.value("id"_L1).toString(), scriptJson);
                    }
                }
                for (auto& scriptForm : mForms) {
                    const auto settingsJson = settingsById.value(scriptForm.id);
                    const auto parameters = settingsJson.value("parameters"_L1).toObject();
                    scriptForm.initial = parameters;
                    scriptForm.form->setValues(parameters);
                    const auto missing = settingsJson.value("missing"_L1).toArray();
                    if (scriptForm.warning) {
                        scriptForm.warning->setVisible(
                            !settingsJson.value("ready"_L1).toBool(true) || !missing.isEmpty()
                        );
                    }
                }
                callback(true, {});
            }
        );
    }

    void ScriptParametersPage::clearForms() {
        mForms.clear();
        while (auto* item = mFormsLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
    }

    void ScriptParametersPage::setStatus(const QString& text) {
        mStatusLabel->setText(text);
        mStatusLabel->setVisible(!text.isEmpty());
    }
}
