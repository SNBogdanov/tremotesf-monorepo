// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptsmenu.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QHash>
#include <QMenu>

#include "rpc/rpc.h"
#include "rpc/scriptjobsmanager.h"
#include "rpc/serversettings.h"
#include "scriptrundialog.h"

using namespace Qt::StringLiterals;

namespace tremotesf::scripts {
    namespace {
        void runScript(
            Rpc* rpc,
            const ScriptInfo& script,
            ScriptContext context,
            const ScriptSelection& selection,
            QJsonObject parameters
        ) {
            rpc->scriptJobsManager()->startJob(script, context, selection, parameters);
        }
    }

    QString scriptErrorMessage(const QString& result) {
        if (result == "script runner is disabled"_L1) {
            return qApp->translate("tremotesf", "Script runner is disabled on the server");
        }
        if (result == "script not found"_L1) {
            return qApp->translate("tremotesf", "Script not found");
        }
        if (result == "script is disabled"_L1) {
            return qApp->translate("tremotesf", "Script is disabled");
        }
        if (result == "script is not available on this platform"_L1) {
            return qApp->translate("tremotesf", "Script is not available on this platform");
        }
        if (result == "script does not support this context"_L1) {
            return qApp->translate("tremotesf", "Script does not support this context");
        }
        if (result == "script requires a selection"_L1) {
            return qApp->translate("tremotesf", "Select torrents or files before running this script");
        }
        if (result == "invalid script payload"_L1) {
            return qApp->translate("tremotesf", "Invalid script payload");
        }
        if (result == "invalid script parameters"_L1) {
            return qApp->translate("tremotesf", "Invalid script parameters");
        }
        if (result == "script settings unavailable"_L1) {
            return qApp->translate("tremotesf", "Script settings are unavailable on the server");
        }
        if (result == "script settings incomplete"_L1) {
            return qApp->translate(
                "tremotesf",
                "Script settings are incomplete. Fill in the script parameters in the server settings."
            );
        }
        if (result == "script selection is too large"_L1) {
            return qApp->translate("tremotesf", "Selected torrents or files are too many");
        }
        if (result == "script payload is too large"_L1) {
            return qApp->translate("tremotesf", "Script payload is too large");
        }
        if (result == "script queue is full"_L1) {
            return qApp->translate("tremotesf", "Script queue is full");
        }
        if (result == "client script queue is full"_L1) {
            return qApp->translate("tremotesf", "Your script queue is full");
        }
        if (result == "client script limit reached"_L1) {
            return qApp->translate("tremotesf", "Too many scripts are already running");
        }
        if (result == "script request id conflict"_L1) {
            return qApp->translate("tremotesf", "Script request conflict");
        }
        if (result == "script execution failed"_L1) {
            return qApp->translate("tremotesf", "Script execution failed");
        }
        if (result == "restart is disabled"_L1) {
            return qApp->translate("tremotesf", "Daemon restart is disabled on the server");
        }
        if (result == "restart already scheduled"_L1) {
            return qApp->translate("tremotesf", "Daemon restart is already scheduled");
        }
        if (result == "restart cooldown active"_L1) {
            return qApp->translate("tremotesf", "Daemon restart is on cooldown, try again later");
        }
        if (result == "restart helper failed"_L1) {
            return qApp->translate("tremotesf", "Failed to schedule daemon restart");
        }
        if (result.isEmpty()) {
            return qApp->translate("tremotesf", "Failed to start script");
        }
        return result;
    }

    void populateScriptsMenu(
        QMenu* menu, Rpc* rpc, ScriptContext context, const ScriptSelection& selection, QWidget* parent
    ) {
        menu->clear();

        const auto& settings = rpc->serverSettings()->data();
        if (!settings.hasScriptRunner() || !settings.scriptRunner.has_value()) {
            menu->setEnabled(false);
            return;
        }
        menu->setEnabled(true);

        QList<const ScriptInfo*> scripts{};
        for (const auto& script : settings.scriptRunner->scripts) {
            if (script.hasContext(context)) {
                scripts.push_back(&script);
            }
        }
        std::ranges::sort(scripts, [](const ScriptInfo* left, const ScriptInfo* right) {
            if (left->group != right->group) {
                return left->group < right->group;
            }
            if (left->order != right->order) {
                return left->order < right->order;
            }
            if (left->name != right->name) {
                return left->name < right->name;
            }
            return left->id < right->id;
        });

        if (scripts.isEmpty()) {
            auto* action = menu->addAction(
                QIcon::fromTheme("tremotesf-no-scripts"_L1),
                qApp->translate("tremotesf", "No scripts")
            );
            action->setEnabled(false);
            return;
        }

        QHash<QString, QMenu*> groups{};
        const ScriptRunnerData runnerData = *settings.scriptRunner;

        for (const auto* script : scripts) {
            QMenu* target = menu;
            if (!script->group.isEmpty()) {
                auto* groupMenu = groups.value(script->group);
                if (!groupMenu) {
                    groupMenu = menu->addMenu(script->group);
                    groups.insert(script->group, groupMenu);
                }
                target = groupMenu;
            }

            const ScriptInfo scriptInfo = *script;
            auto* action = target->addAction(scriptInfo.name);
            if (!scriptInfo.description.isEmpty()) {
                action->setToolTip(scriptInfo.description);
            }
            action->setEnabled(!scriptInfo.requiresSelection || !selection.isEmpty());

            QObject::connect(action, &QAction::triggered, action, [=] {
                if (scriptInfo.confirm || scriptInfo.hasRuntimeParameters()) {
                    ScriptRunDialog dialog(scriptInfo, runnerData, selection, parent);
                    if (dialog.exec() == QDialog::Accepted) {
                        runScript(rpc, scriptInfo, context, selection, dialog.parameters());
                    }
                } else {
                    runScript(rpc, scriptInfo, context, selection, {});
                }
            });
        }
    }
}
