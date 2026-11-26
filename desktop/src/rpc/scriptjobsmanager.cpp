// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptjobsmanager.h"

#include <algorithm>

#include <QDateTime>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QUuid>

#include "rpc.h"
#include "settings.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString clientId() {
            auto* settings = Settings::instance();
            auto id = settings->get_scriptRunnerClientId();
            if (id.isEmpty()) {
                id = "tremotesf-"_L1 + QUuid::createUuid().toString(QUuid::WithoutBraces);
                settings->set_scriptRunnerClientId(id);
            }
            return id;
        }

        QLatin1String contextString(ScriptContext context) {
            switch (context) {
            case ScriptContext::Torrent:
                return "torrent"_L1;
            case ScriptContext::File:
                return "file"_L1;
            case ScriptContext::System:
                return "system"_L1;
            }
            return {};
        }

        QJsonObject selectionJson(const ScriptSelection& selection) {
            QJsonArray hashes{};
            for (const auto& hash : selection.torrentHashes) {
                hashes.append(hash);
            }
            QJsonArray files{};
            for (const auto& file : selection.files) {
                files.append(QJsonObject{{"hash"_L1, file.hash}, {"file_index"_L1, file.fileIndex}});
            }
            return QJsonObject{{"hashes"_L1, hashes}, {"files"_L1, files}};
        }
    }

    ScriptJobsManager::ScriptJobsManager(Rpc* rpc, QObject* parent) : QObject(parent), mRpc(rpc) {
        mPollTimer.setInterval(1000);
        QObject::connect(&mPollTimer, &QTimer::timeout, this, &ScriptJobsManager::poll);
        QObject::connect(mRpc, &Rpc::connectedChanged, this, &ScriptJobsManager::updatePolling);
    }

    void ScriptJobsManager::startJob(
        const ScriptInfo& script, ScriptContext context, const ScriptSelection& selection, const QJsonObject& parameters
    ) {
        QJsonObject payload{
            {"schema"_L1, "transgui.script/v1"_L1},
            {"context"_L1, contextString(context)},
            {"parameters"_L1, parameters},
            {"selection"_L1, selectionJson(selection)}
        };

        QByteArray secret(32, '\0');
        for (char& byte : secret) {
            byte = static_cast<char>(QRandomGenerator::system()->bounded(256));
        }

        QJsonObject arguments{
            {"id"_L1, script.id},
            {"client_id"_L1, clientId()},
            {"client_request_id"_L1, QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"client_secret"_L1, QString::fromLatin1(secret.toHex())},
            {"payload"_L1, payload}
        };

        const auto selectionCount = selection.itemCount();
        mRpc->requestScriptRun(
            arguments,
            this,
            [this, scriptName = script.name, scriptId = script.id, context, selectionCount](ScriptResponse response) {
                if (!response.success) {
                    emit jobStartFailed(scriptName, response.result);
                    return;
                }
                const auto jobId = response.arguments.value("job_id"_L1).toString();
                const auto jobToken = response.arguments.value("job_token"_L1).toString();
                if (jobId.isEmpty() || jobToken.isEmpty()) {
                    emit jobStartFailed(scriptName, response.result);
                    return;
                }
                ScriptJob job{};
                job.id = jobId;
                job.token = jobToken;
                job.scriptId = scriptId;
                job.scriptName = scriptName;
                job.context = context;
                job.state = response.arguments.value("state"_L1).toString("queued"_L1);
                job.queuedAt = QDateTime::currentSecsSinceEpoch();
                job.selectionCount = selectionCount;
                mJobs.push_back(std::move(job));
                emit jobAdded(jobId);
                updatePolling();
            }
        );
    }

    void ScriptJobsManager::cancelJob(const QString& jobId) {
        auto* job = findJob(jobId);
        if (!job || !job->isActive()) {
            return;
        }
        job->cancelRequested = true;

        const QJsonObject arguments{{"job_id"_L1, job->id}, {"job_token"_L1, job->token}};
        mRpc->requestScriptCancel(arguments, this, [this, jobId](ScriptResponse response) {
            auto* job = findJob(jobId);
            if (!job) {
                return;
            }
            if (response.success) {
                const auto state = response.arguments.value("state"_L1).toString();
                if (!state.isEmpty()) {
                    job->state = state;
                }
            }
            emit jobChanged(jobId);
            updatePolling();
        });
    }

    void ScriptJobsManager::clearFinished() {
        for (auto it = mJobs.begin(); it != mJobs.end();) {
            if (!it->isActive()) {
                const auto id = it->id;
                it = mJobs.erase(it);
                emit jobRemoved(id);
            } else {
                ++it;
            }
        }
    }

    const ScriptJob* ScriptJobsManager::job(const QString& jobId) const {
        const auto found = std::ranges::find(mJobs, jobId, &ScriptJob::id);
        return found == mJobs.end() ? nullptr : &*found;
    }

    ScriptJob* ScriptJobsManager::findJob(const QString& jobId) {
        const auto found = std::ranges::find(mJobs, jobId, &ScriptJob::id);
        return found == mJobs.end() ? nullptr : &*found;
    }

    void ScriptJobsManager::poll() {
        for (auto& job : mJobs) {
            if (job.isActive() && !job.polling) {
                requestStatus(job.id);
            }
        }
    }

    void ScriptJobsManager::requestStatus(const QString& jobId) {
        auto* job = findJob(jobId);
        if (!job || job->polling) {
            return;
        }
        job->polling = true;

        QJsonObject arguments{
            {"job_id"_L1, job->id},
            {"job_token"_L1, job->token},
            {"stdout_offset"_L1, job->stdoutOffset},
            {"stdout_limit"_L1, 65536},
            {"stderr_offset"_L1, job->stderrOffset},
            {"stderr_limit"_L1, 65536}
        };

        mRpc->requestScriptStatus(arguments, this, [this, jobId](ScriptResponse response) {
            onStatusResponse(jobId, response);
        });
    }

    void ScriptJobsManager::onStatusResponse(const QString& jobId, const ScriptResponse& response) {
        auto* job = findJob(jobId);
        if (!job) {
            return;
        }
        job->polling = false;

        if (!response.success) {
            if (response.result == "job not found"_L1) {
                job->state = "failed"_L1;
                job->error = response.result;
                job->finishedAt = QDateTime::currentSecsSinceEpoch();
                emit jobChanged(jobId);
                emit jobFinished(jobId);
                updatePolling();
            }
            return;
        }

        const auto& arguments = response.arguments;
        job->state = arguments.value("state"_L1).toString(job->state);
        if (arguments.contains("exit_code"_L1) && !arguments.value("exit_code"_L1).isNull()) {
            job->hasExitCode = true;
            job->exitCode = arguments.value("exit_code"_L1).toInt();
        }
        if (arguments.contains("term_signal"_L1) && !arguments.value("term_signal"_L1).isNull()) {
            job->hasTermSignal = true;
            job->termSignal = arguments.value("term_signal"_L1).toInt();
        }
        job->error = arguments.value("error"_L1).toString();
        job->queuedAt = arguments.value("queued_at"_L1).toInteger(job->queuedAt);
        job->startedAt = arguments.value("started_at"_L1).toInteger(job->startedAt);
        job->finishedAt = arguments.value("finished_at"_L1).toInteger(job->finishedAt);

        const auto stdoutChunk = arguments.value("stdout"_L1).toString();
        if (!stdoutChunk.isEmpty()) {
            job->stdoutText += stdoutChunk;
        }
        job->stdoutOffset =
            arguments.value("stdout_offset"_L1).toInteger(job->stdoutOffset) + stdoutChunk.toUtf8().size();
        job->stdoutTotalBytes = arguments.value("stdout_total_bytes"_L1).toInteger(job->stdoutTotalBytes);
        job->stdoutStoredBytes = arguments.value("stdout_stored_bytes"_L1).toInteger(job->stdoutStoredBytes);
        job->stdoutTruncated = arguments.value("stdout_truncated"_L1).toBool(job->stdoutTruncated);

        const auto stderrChunk = arguments.value("stderr"_L1).toString();
        if (!stderrChunk.isEmpty()) {
            job->stderrText += stderrChunk;
        }
        job->stderrOffset =
            arguments.value("stderr_offset"_L1).toInteger(job->stderrOffset) + stderrChunk.toUtf8().size();
        job->stderrTotalBytes = arguments.value("stderr_total_bytes"_L1).toInteger(job->stderrTotalBytes);
        job->stderrStoredBytes = arguments.value("stderr_stored_bytes"_L1).toInteger(job->stderrStoredBytes);
        job->stderrTruncated = arguments.value("stderr_truncated"_L1).toBool(job->stderrTruncated);

        job->timedOut = job->state == "timed_out"_L1;

        const bool active = job->isActive();
        emit jobChanged(jobId);
        if (!active) {
            emit jobFinished(jobId);
        }
        updatePolling();
    }

    void ScriptJobsManager::updatePolling() {
        const bool hasActive = std::ranges::any_of(mJobs, [](const ScriptJob& job) { return job.isActive(); });
        if (hasActive && mRpc->isConnected()) {
            if (!mPollTimer.isActive()) {
                mPollTimer.start();
            }
        } else {
            mPollTimer.stop();
        }
    }
}
