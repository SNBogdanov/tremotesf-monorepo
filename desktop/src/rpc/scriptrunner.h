// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_SCRIPTRUNNER_H
#define TREMOTESF_RPC_SCRIPTRUNNER_H

#include <optional>

#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

class QJsonObject;

namespace tremotesf {
    enum class ScriptContext { Torrent, File, System };

    enum class ScriptParameterType { String, Integer, Number, Boolean, Host };

    enum class ScriptParameterStorage { Runtime, Host };

    struct ScriptExternalHost {
        QString id;
        QString name;
        QString description;
    };

    struct ScriptParameter {
        QString key;
        QString name;
        QString description;
        ScriptParameterType type{};
        ScriptParameterStorage storage = ScriptParameterStorage::Runtime;
        bool array = false;
        bool required = false;
        std::optional<QJsonValue> defaultValue{};
        std::optional<double> min{};
        std::optional<double> max{};
        QStringList hosts{};

        [[nodiscard]] bool hasDefault() const { return defaultValue.has_value(); }
    };

    struct ScriptInfo {
        QString id;
        QString name;
        QString description;
        QString group;
        int order = 0;
        QString platform;
        QList<ScriptContext> contexts;
        bool requiresSelection = true;
        bool confirm = false;
        int timeoutSeconds = 300;
        qint64 maxOutputBytes = 0;
        QList<ScriptParameter> parameters;

        [[nodiscard]] bool hasContext(ScriptContext context) const { return contexts.contains(context); }
        [[nodiscard]] bool hasHostParameters() const;
        [[nodiscard]] bool hasRuntimeParameters() const;
    };

    struct ScriptRestartDaemonInfo {
        QString id;
        QString name;
        QString context;
        bool enabled = false;
    };

    struct ScriptRunnerData {
        QString schema;
        QString revision;
        QList<ScriptExternalHost> externalHosts;
        ScriptRestartDaemonInfo restartDaemon;
        QList<ScriptInfo> scripts;

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] const ScriptInfo* scriptById(const QString& id) const;
        [[nodiscard]] const ScriptExternalHost* externalHostById(const QString& id) const;
    };

    struct ScriptFileSelection {
        QString hash;
        int fileIndex = 0;

        bool operator==(const ScriptFileSelection&) const = default;
    };

    struct ScriptSelection {
        QStringList torrentHashes;
        QList<ScriptFileSelection> files;

        [[nodiscard]] bool isEmpty() const { return torrentHashes.isEmpty() && files.isEmpty(); }
        [[nodiscard]] qsizetype itemCount() const { return torrentHashes.size() + files.size(); }
    };

    [[nodiscard]] std::optional<ScriptRunnerData> parseScriptRunner(const QJsonObject& descriptor);
}

#endif // TREMOTESF_RPC_SCRIPTRUNNER_H
