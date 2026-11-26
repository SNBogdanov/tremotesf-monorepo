// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_SCRIPTJOBSMANAGER_H
#define TREMOTESF_RPC_SCRIPTJOBSMANAGER_H

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>

#include "scriptrunner.h"

namespace tremotesf {
    class Rpc;
    struct ScriptResponse;

    struct ScriptJob {
        QString id;
        QString token;
        QString scriptId;
        QString scriptName;
        ScriptContext context{};
        QString state;
        QString error;
        bool hasExitCode = false;
        int exitCode = 0;
        bool hasTermSignal = false;
        int termSignal = 0;
        qint64 queuedAt = 0;
        qint64 startedAt = 0;
        qint64 finishedAt = 0;
        QString stdoutText;
        QString stderrText;
        qint64 stdoutOffset = 0;
        qint64 stderrOffset = 0;
        qint64 stdoutTotalBytes = 0;
        qint64 stdoutStoredBytes = 0;
        qint64 stderrTotalBytes = 0;
        qint64 stderrStoredBytes = 0;
        bool stdoutTruncated = false;
        bool stderrTruncated = false;
        bool timedOut = false;
        bool cancelRequested = false;
        bool polling = false;
        qsizetype selectionCount = 0;

        [[nodiscard]] bool isActive() const {
            return state == QStringLiteral("queued") || state == QStringLiteral("running");
        }
    };

    class ScriptJobsManager final : public QObject {
        Q_OBJECT

    public:
        explicit ScriptJobsManager(Rpc* rpc, QObject* parent = nullptr);

        void startJob(
            const ScriptInfo& script,
            ScriptContext context,
            const ScriptSelection& selection,
            const QJsonObject& parameters
        );
        void cancelJob(const QString& jobId);
        void clearFinished();

        [[nodiscard]] const QList<ScriptJob>& jobs() const { return mJobs; }
        [[nodiscard]] const ScriptJob* job(const QString& jobId) const;

    signals:
        void jobAdded(const QString& jobId);
        void jobChanged(const QString& jobId);
        void jobFinished(const QString& jobId);
        void jobRemoved(const QString& jobId);
        void jobStartFailed(const QString& scriptName, const QString& result);

    private:
        void poll();
        void requestStatus(const QString& jobId);
        void onStatusResponse(const QString& jobId, const ScriptResponse& response);
        void updatePolling();
        ScriptJob* findJob(const QString& jobId);

        Rpc* mRpc;
        QList<ScriptJob> mJobs;
        QTimer mPollTimer;
    };
}

#endif // TREMOTESF_RPC_SCRIPTJOBSMANAGER_H
