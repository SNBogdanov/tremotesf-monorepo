// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "scriptrunner.h"
#include "serversettings.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QJsonObject parseObject(const char* json) { return QJsonDocument::fromJson(QByteArray(json)).object(); }
    }

    class ScriptRunnerTest final : public QObject {
        Q_OBJECT

    private slots:
        void parseFullDescriptor() {
            const auto data = parseScriptRunner(parseObject(R"json(
                {
                    "schema": "transgui-script-runner/v1",
                    "revision": "sha256:abc",
                    "external_hosts": [
                        {"id": "qnap", "name": "QNAP", "description": "QNAP Transmission"},
                        {"id": "samsung-pc", "description": ""}
                    ],
                    "restart_daemon": {
                        "id": "restart-daemon",
                        "name": "Restart daemon",
                        "context": "system",
                        "enabled": true
                    },
                    "scripts": [
                        {
                            "id": "copy",
                            "name": "Copy",
                            "description": "Copy torrents",
                            "group": "Orchestration",
                            "order": 30,
                            "platform": "any",
                            "contexts": ["torrent", "file"],
                            "requires_selection": true,
                            "confirm": true,
                            "parameters": [
                                {
                                    "key": "target",
                                    "name": "Target",
                                    "type": "host",
                                    "storage": "runtime",
                                    "required": true,
                                    "hosts": ["qnap"]
                                },
                                {
                                    "key": "retries",
                                    "name": "Retries",
                                    "type": "integer",
                                    "storage": "host",
                                    "default": 3,
                                    "min": 0,
                                    "max": 10
                                }
                            ],
                            "payload": "transgui.script/v1",
                            "stdin": "json",
                            "timeout_seconds": 60,
                            "max_output_bytes": 1024
                        },
                        {
                            "id": "cleanup",
                            "name": "Cleanup",
                            "contexts": ["system"],
                            "requires_selection": false,
                            "parameters": [
                                {
                                    "key": "dirs",
                                    "name": "Dirs",
                                    "type": "string",
                                    "array": true,
                                    "default": ["a", "b"]
                                }
                            ],
                            "payload": "transgui.script/v1",
                            "stdin": "json"
                        }
                    ]
                }
            )json"));

            QVERIFY(data.has_value());
            QVERIFY(data->isValid());
            QCOMPARE(data->revision, "sha256:abc"_L1);
            QCOMPARE(data->externalHosts.size(), 2);
            QCOMPARE(data->externalHosts.at(0).id, "qnap"_L1);
            QCOMPARE(data->externalHosts.at(0).name, "QNAP"_L1);
            QCOMPARE(data->externalHosts.at(1).name, "samsung-pc"_L1);
            QVERIFY(data->restartDaemon.enabled);

            QCOMPARE(data->scripts.size(), 2);
            const auto* copy = data->scriptById("copy"_L1);
            QVERIFY(copy);
            QCOMPARE(copy->group, "Orchestration"_L1);
            QCOMPARE(copy->order, 30);
            QCOMPARE(copy->timeoutSeconds, 60);
            QCOMPARE(copy->maxOutputBytes, qint64(1024));
            QVERIFY(copy->hasContext(ScriptContext::Torrent));
            QVERIFY(copy->hasContext(ScriptContext::File));
            QVERIFY(!copy->hasContext(ScriptContext::System));
            QVERIFY(copy->hasHostParameters());
            QVERIFY(copy->hasRuntimeParameters());
            QCOMPARE(copy->parameters.size(), 2);

            const auto& target = copy->parameters.at(0);
            QVERIFY(target.type == ScriptParameterType::Host);
            QVERIFY(target.storage == ScriptParameterStorage::Runtime);
            QVERIFY(target.required);
            QVERIFY(!target.hasDefault());
            QCOMPARE(target.hosts, QStringList{"qnap"_L1});

            const auto& retries = copy->parameters.at(1);
            QVERIFY(retries.type == ScriptParameterType::Integer);
            QVERIFY(retries.storage == ScriptParameterStorage::Host);
            QVERIFY(retries.hasDefault());
            QCOMPARE(retries.defaultValue->toInt(), 3);
            QCOMPARE(retries.min.value(), 0.0);
            QCOMPARE(retries.max.value(), 10.0);

            const auto* cleanup = data->scriptById("cleanup"_L1);
            QVERIFY(cleanup);
            QVERIFY(cleanup->hasContext(ScriptContext::System));
            QVERIFY(!cleanup->requiresSelection);
            QVERIFY(!cleanup->hasHostParameters());
            QCOMPARE(cleanup->parameters.size(), 1);
            QVERIFY(cleanup->parameters.at(0).array);
            QCOMPARE(cleanup->parameters.at(0).defaultValue->toArray().size(), 2);
        }

        void invalidTopLevelDescriptor() {
            QVERIFY(
                !parseScriptRunner(parseObject(R"json({"revision": "sha256:abc", "scripts": []})json")).has_value()
            );
            QVERIFY(!parseScriptRunner(parseObject(R"json({"schema": "transgui-script-runner/v1", "scripts": []})json"))
                         .has_value());
            QVERIFY(!parseScriptRunner(parseObject(R"json(
                {"schema": "transgui-script-runner/v1", "revision": "sha256:abc"}
            )json"))
                         .has_value());
        }

        void invalidParameterSkipsScript() {
            const auto data = parseScriptRunner(parseObject(R"json(
                {
                    "schema": "transgui-script-runner/v1",
                    "revision": "sha256:abc",
                    "scripts": [
                        {
                            "id": "bad",
                            "name": "Bad",
                            "contexts": ["torrent"],
                            "parameters": [
                                {"key": "value", "name": "Value", "type": "unknown"}
                            ],
                            "payload": "transgui.script/v1",
                            "stdin": "json"
                        }
                    ]
                }
            )json"));
            QVERIFY(data.has_value());
            QVERIFY(data->scripts.isEmpty());
        }

        void requiredParameterWithDefaultSkipsScript() {
            const auto data = parseScriptRunner(parseObject(R"json(
                {
                    "schema": "transgui-script-runner/v1",
                    "revision": "sha256:abc",
                    "scripts": [
                        {
                            "id": "bad",
                            "name": "Bad",
                            "contexts": ["torrent"],
                            "parameters": [
                                {"key": "value", "name": "Value", "type": "string", "required": true, "default": "x"}
                            ],
                            "payload": "transgui.script/v1",
                            "stdin": "json"
                        }
                    ]
                }
            )json"));
            QVERIFY(data.has_value());
            QVERIFY(data->scripts.isEmpty());
        }

        void unknownHostSkipsScript() {
            const auto data = parseScriptRunner(parseObject(R"json(
                {
                    "schema": "transgui-script-runner/v1",
                    "revision": "sha256:abc",
                    "external_hosts": [{"id": "qnap"}],
                    "scripts": [
                        {
                            "id": "bad",
                            "name": "Bad",
                            "contexts": ["torrent"],
                            "parameters": [
                                {
                                    "key": "target",
                                    "name": "Target",
                                    "type": "host",
                                    "hosts": ["unknown"]
                                }
                            ],
                            "payload": "transgui.script/v1",
                            "stdin": "json"
                        }
                    ]
                }
            )json"));
            QVERIFY(data.has_value());
            QVERIFY(data->scripts.isEmpty());
        }

        void featureGating() {
            const auto parsed = parseScriptRunner(parseObject(R"json(
                {
                    "schema": "transgui-script-runner/v1",
                    "revision": "sha256:abc",
                    "restart_daemon": {"enabled": true},
                    "scripts": []
                }
            )json"));
            QVERIFY(parsed.has_value());

            ServerSettingsData settings{};
            QVERIFY(!settings.hasScriptRunner());
            QVERIFY(!settings.hasScriptRunnerSettings());
            QVERIFY(!settings.hasRestartDaemon());

            settings
                .forkFeatures = {"script_runner/v1"_L1, "script_runner_settings/v1"_L1, "script_restart_daemon/v1"_L1};
            settings.scriptRunner = *parsed;
            QVERIFY(settings.hasScriptRunner());
            QVERIFY(settings.hasScriptRunnerSettings());
            QVERIFY(settings.hasRestartDaemon());

            settings.forkFeatures.removeAll("script_runner/v1"_L1);
            QVERIFY(!settings.hasScriptRunner());
            QVERIFY(settings.hasScriptRunnerSettings());
            QVERIFY(settings.hasRestartDaemon());
        }
    };
}

QTEST_GUILESS_MAIN(tremotesf::ScriptRunnerTest)

#include "scriptrunner_test.moc"
