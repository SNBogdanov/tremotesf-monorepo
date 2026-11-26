// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptrunner.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <set>

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include "log/log.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        constexpr auto scriptRunnerSchema = "transgui-script-runner/v1"_L1;
        constexpr auto scriptPayloadSchema = "transgui.script/v1"_L1;

        std::optional<ScriptContext> parseContext(const QString& value) {
            if (value == "torrent"_L1) {
                return ScriptContext::Torrent;
            }
            if (value == "file"_L1) {
                return ScriptContext::File;
            }
            if (value == "system"_L1) {
                return ScriptContext::System;
            }
            return std::nullopt;
        }

        std::optional<ScriptParameterType> parseParameterType(const QString& value) {
            if (value == "string"_L1) {
                return ScriptParameterType::String;
            }
            if (value == "integer"_L1) {
                return ScriptParameterType::Integer;
            }
            if (value == "number"_L1) {
                return ScriptParameterType::Number;
            }
            if (value == "boolean"_L1) {
                return ScriptParameterType::Boolean;
            }
            if (value == "host"_L1) {
                return ScriptParameterType::Host;
            }
            return std::nullopt;
        }

        std::optional<ScriptParameterStorage> parseParameterStorage(const QString& value) {
            if (value == "runtime"_L1) {
                return ScriptParameterStorage::Runtime;
            }
            if (value == "host"_L1) {
                return ScriptParameterStorage::Host;
            }
            return std::nullopt;
        }

        bool isScalarValue(ScriptParameterType type, const QJsonValue& value) {
            switch (type) {
            case ScriptParameterType::String:
            case ScriptParameterType::Host:
                return value.isString();
            case ScriptParameterType::Integer:
                return value.isDouble() && std::floor(value.toDouble()) == value.toDouble();
            case ScriptParameterType::Number:
                return value.isDouble() && std::isfinite(value.toDouble());
            case ScriptParameterType::Boolean:
                return value.isBool();
            }
            return false;
        }

        bool isValidParameterValue(const ScriptParameter& parameter, const QJsonValue& value, bool isDefault) {
            if (value.isNull() || value.isUndefined() || value.isObject()) {
                return false;
            }
            if (parameter.array) {
                if (!value.isArray()) {
                    return false;
                }
                const auto array = value.toArray();
                if (isDefault && array.isEmpty()) {
                    // An empty default is allowed for optional arrays, but not for required ones.
                    return !parameter.required;
                }
                if (array.size() > 256) {
                    return false;
                }
                for (const auto& element : array) {
                    if (element.isArray() || element.isObject() || element.isNull() || element.isUndefined()) {
                        return false;
                    }
                    if (!isScalarValue(parameter.type, element)) {
                        return false;
                    }
                }
            } else {
                if (value.isArray() || !isScalarValue(parameter.type, value)) {
                    return false;
                }
            }

            if (parameter.type == ScriptParameterType::Host) {
                if (parameter.array) {
                    for (const auto& element : value.toArray()) {
                        if (!parameter.hosts.contains(element.toString())) {
                            return false;
                        }
                    }
                } else if (!parameter.hosts.contains(value.toString())) {
                    return false;
                }
            }

            if ((parameter.type == ScriptParameterType::Integer || parameter.type == ScriptParameterType::Number)) {
                const auto checkRange = [&](const QJsonValue& element) {
                    const auto number = element.toDouble();
                    if (parameter.min.has_value() && number < *parameter.min) {
                        return false;
                    }
                    if (parameter.max.has_value() && number > *parameter.max) {
                        return false;
                    }
                    return true;
                };
                if (parameter.array) {
                    for (const auto& element : value.toArray()) {
                        if (!checkRange(element)) {
                            return false;
                        }
                    }
                } else if (!checkRange(value)) {
                    return false;
                }
            }

            return true;
        }

        std::optional<ScriptParameter> parseParameter(const QJsonObject& json, const QStringList& allExternalHostIds) {
            const auto key = json.value("key"_L1).toString();
            const auto name = json.value("name"_L1).toString();
            const auto typeValue = json.value("type"_L1).toString();
            if (key.isEmpty() || name.isEmpty() || typeValue.isEmpty()) {
                warning().log("Script parameter without key, name or type is ignored");
                return std::nullopt;
            }

            const auto type = parseParameterType(typeValue);
            if (!type.has_value()) {
                warning().log("Script parameter '{}' has unknown type '{}'", key, typeValue);
                return std::nullopt;
            }

            ScriptParameter parameter{};
            parameter.key = key;
            parameter.name = name;
            parameter.description = json.value("description"_L1).toString();
            parameter.type = *type;
            if (json.contains("storage"_L1)) {
                const auto storage = parseParameterStorage(json.value("storage"_L1).toString());
                if (!storage.has_value()) {
                    warning().log("Script parameter '{}' has unknown storage", key);
                    return std::nullopt;
                }
                parameter.storage = *storage;
            } else {
                parameter.storage = ScriptParameterStorage::Runtime;
            }
            parameter.array = json.value("array"_L1).toBool(false);
            parameter.required = json.value("required"_L1).toBool(false);

            if (json.contains("min"_L1)) {
                if (parameter.type != ScriptParameterType::Integer && parameter.type != ScriptParameterType::Number) {
                    warning().log("Script parameter '{}' has min for non-numeric type", key);
                    return std::nullopt;
                }
                parameter.min = json.value("min"_L1).toDouble();
            }
            if (json.contains("max"_L1)) {
                if (parameter.type != ScriptParameterType::Integer && parameter.type != ScriptParameterType::Number) {
                    warning().log("Script parameter '{}' has max for non-numeric type", key);
                    return std::nullopt;
                }
                parameter.max = json.value("max"_L1).toDouble();
            }
            if (parameter.min.has_value() && parameter.max.has_value() && *parameter.min > *parameter.max) {
                warning().log("Script parameter '{}' has min greater than max", key);
                return std::nullopt;
            }

            if (parameter.type == ScriptParameterType::Host) {
                if (json.contains("hosts"_L1)) {
                    for (const auto& host : json.value("hosts"_L1).toArray()) {
                        const auto id = host.toString();
                        if (id.isEmpty() || !allExternalHostIds.contains(id)) {
                            warning().log("Script parameter '{}' references unknown external host '{}'", key, id);
                            return std::nullopt;
                        }
                        parameter.hosts.push_back(id);
                    }
                    if (parameter.hosts.isEmpty()) {
                        warning().log("Script parameter '{}' has empty hosts list", key);
                        return std::nullopt;
                    }
                } else {
                    parameter.hosts = allExternalHostIds;
                }
                if (parameter.hosts.isEmpty()) {
                    warning().log("Script parameter '{}' has no available external hosts", key);
                    return std::nullopt;
                }
            } else if (json.contains("hosts"_L1)) {
                warning().log("Script parameter '{}' has hosts for non-host type", key);
                return std::nullopt;
            }

            if (json.contains("default"_L1)) {
                if (parameter.required) {
                    warning().log("Script parameter '{}' is required and has default", key);
                    return std::nullopt;
                }
                const auto value = json.value("default"_L1);
                if (!isValidParameterValue(parameter, value, true)) {
                    warning().log("Script parameter '{}' has invalid default value", key);
                    return std::nullopt;
                }
                parameter.defaultValue = value;
            }

            return parameter;
        }

        std::optional<ScriptInfo> parseScript(const QJsonObject& json, const QList<ScriptExternalHost>& externalHosts) {
            const auto id = json.value("id"_L1).toString();
            const auto name = json.value("name"_L1).toString();
            if (id.isEmpty() || name.isEmpty()) {
                warning().log("Script without id or name is ignored");
                return std::nullopt;
            }
            if (json.value("payload"_L1).toString() != scriptPayloadSchema) {
                warning().log("Script '{}' has unsupported payload", id);
                return std::nullopt;
            }
            if (json.value("stdin"_L1).toString() != "json"_L1) {
                warning().log("Script '{}' has unsupported stdin", id);
                return std::nullopt;
            }

            ScriptInfo script{};
            script.id = id;
            script.name = name;
            script.description = json.value("description"_L1).toString();
            script.group = json.value("group"_L1).toString();
            script.order = json.value("order"_L1).toInt();
            script.platform = json.value("platform"_L1).toString("any"_L1);
            script.requiresSelection = json.value("requires_selection"_L1).toBool(true);
            script.confirm = json.value("confirm"_L1).toBool(false);
            script.timeoutSeconds = json.value("timeout_seconds"_L1).toInt(300);
            script.maxOutputBytes = json.value("max_output_bytes"_L1).toInteger();

            std::set<ScriptContext> contexts{};
            for (const auto& contextValue : json.value("contexts"_L1).toArray()) {
                const auto context = parseContext(contextValue.toString());
                if (!context.has_value()) {
                    warning().log("Script '{}' has unknown context", id);
                    return std::nullopt;
                }
                contexts.insert(*context);
            }
            if (contexts.empty()) {
                warning().log("Script '{}' has no contexts", id);
                return std::nullopt;
            }
            if (contexts.contains(ScriptContext::System) && contexts.size() > 1) {
                warning().log("Script '{}' combines system context with other contexts", id);
                return std::nullopt;
            }
            script.contexts.reserve(contexts.size());
            for (const auto context : contexts) {
                script.contexts.push_back(context);
            }
            if (script.hasContext(ScriptContext::System)) {
                script.requiresSelection = false;
            }

            QStringList allExternalHostIds{};
            allExternalHostIds.reserve(externalHosts.size());
            for (const auto& host : externalHosts) {
                allExternalHostIds.push_back(host.id);
            }

            for (const auto& parameterValue : json.value("parameters"_L1).toArray()) {
                if (!parameterValue.isObject()) {
                    warning().log("Script '{}' has non-object parameter", id);
                    return std::nullopt;
                }
                auto parameter = parseParameter(parameterValue.toObject(), allExternalHostIds);
                if (!parameter.has_value()) {
                    return std::nullopt;
                }
                script.parameters.push_back(std::move(*parameter));
            }

            return script;
        }
    }

    bool ScriptInfo::hasHostParameters() const {
        return std::ranges::any_of(parameters, [](const ScriptParameter& parameter) {
            return parameter.storage == ScriptParameterStorage::Host;
        });
    }

    bool ScriptInfo::hasRuntimeParameters() const {
        return std::ranges::any_of(parameters, [](const ScriptParameter& parameter) {
            return parameter.storage == ScriptParameterStorage::Runtime;
        });
    }

    bool ScriptRunnerData::isValid() const { return schema == scriptRunnerSchema && !revision.isEmpty(); }

    const ScriptInfo* ScriptRunnerData::scriptById(const QString& id) const {
        const auto found = std::ranges::find(scripts, id, &ScriptInfo::id);
        return found == scripts.end() ? nullptr : &*found;
    }

    const ScriptExternalHost* ScriptRunnerData::externalHostById(const QString& id) const {
        const auto found = std::ranges::find(externalHosts, id, &ScriptExternalHost::id);
        return found == externalHosts.end() ? nullptr : &*found;
    }

    std::optional<ScriptRunnerData> parseScriptRunner(const QJsonObject& descriptor) {
        ScriptRunnerData data{};
        data.schema = descriptor.value("schema"_L1).toString();
        data.revision = descriptor.value("revision"_L1).toString();
        if (!data.isValid()) {
            warning().log("Script runner descriptor has invalid schema or revision");
            return std::nullopt;
        }

        QSet<QString> externalHostIds{};
        for (const auto& hostValue : descriptor.value("external_hosts"_L1).toArray()) {
            if (!hostValue.isObject()) {
                warning().log("Script runner external host is not an object");
                continue;
            }
            const auto hostJson = hostValue.toObject();
            const auto id = hostJson.value("id"_L1).toString();
            if (id.isEmpty() || externalHostIds.contains(id)) {
                warning().log("Script runner external host has empty or duplicate id '{}'", id);
                continue;
            }
            externalHostIds.insert(id);
            data.externalHosts.push_back(
                ScriptExternalHost{
                    .id = id,
                    .name = hostJson.value("name"_L1).toString(id),
                    .description = hostJson.value("description"_L1).toString()
                }
            );
        }

        const auto restartJson = descriptor.value("restart_daemon"_L1).toObject();
        data.restartDaemon = ScriptRestartDaemonInfo{
            .id = restartJson.value("id"_L1).toString("restart-daemon"_L1),
            .name = restartJson.value("name"_L1).toString("Restart daemon"_L1),
            .context = restartJson.value("context"_L1).toString("system"_L1),
            .enabled = restartJson.value("enabled"_L1).toBool(false)
        };

        const auto scriptsValue = descriptor.value("scripts"_L1);
        if (!scriptsValue.isArray()) {
            warning().log("Script runner descriptor has no scripts array");
            return std::nullopt;
        }
        for (const auto& scriptValue : scriptsValue.toArray()) {
            if (!scriptValue.isObject()) {
                warning().log("Script runner script is not an object");
                continue;
            }
            auto script = parseScript(scriptValue.toObject(), data.externalHosts);
            if (script.has_value()) {
                data.scripts.push_back(std::move(*script));
            }
        }

        return data;
    }
}
