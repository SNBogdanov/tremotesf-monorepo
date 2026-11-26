// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serversettings.h"

#include <QJsonArray>
#include <QJsonObject>

#include "jsonutils.h"
#include "pathutils.h"
#include "rpc.h"
#include "stdutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    using namespace impl;

    namespace {
        constexpr auto downloadDirectoryKey = "download-dir"_L1;
        constexpr auto trashTorrentFilesKey = "trash-original-torrent-files"_L1;
        constexpr auto startAddedTorrentsKey = "start-added-torrents"_L1;
        constexpr auto renameIncompleteFilesKey = "rename-partial-files"_L1;
        constexpr auto sequentialDownloadKey = "sequential_download"_L1;
        constexpr auto incompleteDirectoryEnabledKey = "incomplete-dir-enabled"_L1;
        constexpr auto incompleteDirectoryKey = "incomplete-dir"_L1;

        constexpr auto ratioLimitedKey = "seedRatioLimited"_L1;
        constexpr auto ratioLimitKey = "seedRatioLimit"_L1;
        constexpr auto idleSeedingLimitedKey = "idle-seeding-limit-enabled"_L1;
        constexpr auto idleSeedingLimitKey = "idle-seeding-limit"_L1;

        constexpr auto downloadQueueEnabledKey = "download-queue-enabled"_L1;
        constexpr auto downloadQueueSizeKey = "download-queue-size"_L1;
        constexpr auto seedQueueEnabledKey = "seed-queue-enabled"_L1;
        constexpr auto seedQueueSizeKey = "seed-queue-size"_L1;
        constexpr auto idleQueueLimitedKey = "queue-stalled-enabled"_L1;
        constexpr auto idleQueueLimitKey = "queue-stalled-minutes"_L1;

        constexpr auto autoPriorityEnabledKey = "auto_priority_enabled"_L1;
        constexpr auto autoPriorityMaxHighKey = "auto_priority_max_high"_L1;
        constexpr auto autoPrioritySortKey = "auto_priority_sort"_L1;
        constexpr auto queueMoveToEndEnabledKey = "queue_move_to_end_enabled"_L1;
        constexpr auto queueMoveToEndMinutesKey = "queue_move_to_end_minutes"_L1;

        constexpr auto minFreeSpaceKey = "min-free-space"_L1;
        constexpr auto minFreeSpaceResumeMarginKey = "min-free-space-resume-margin"_L1;
        constexpr auto moveCompletedTorrentsEnabledKey = "move_completed_torrents_enabled"_L1;
        constexpr auto movePartialFilesEnabledKey = "move_partial_files_enabled"_L1;
        constexpr auto forceFinishPartialFilesEnabledKey = "force_finish_partial_files_enabled"_L1;
        constexpr auto scriptTorrentDoneRemoveTorrentEnabledKey = "script_torrent_done_remove_torrent_enabled"_L1;
        constexpr auto scriptTorrentDoneRemoveTempFilesEnabledKey = "script_torrent_done_remove_temp_files_enabled"_L1;
        constexpr auto scriptTorrentDoneRemoveAllFilesEnabledKey = "script_torrent_done_remove_all_files_enabled"_L1;

        constexpr auto downloadSpeedLimitedKey = "speed-limit-down-enabled"_L1;
        constexpr auto downloadSpeedLimitKey = "speed-limit-down"_L1;
        constexpr auto uploadSpeedLimitedKey = "speed-limit-up-enabled"_L1;
        constexpr auto uploadSpeedLimitKey = "speed-limit-up"_L1;
        constexpr auto alternativeSpeedLimitsEnabledKey = "alt-speed-enabled"_L1;
        constexpr auto alternativeDownloadSpeedLimitKey = "alt-speed-down"_L1;
        constexpr auto alternativeUploadSpeedLimitKey = "alt-speed-up"_L1;
        constexpr auto alternativeSpeedLimitsScheduledKey = "alt-speed-time-enabled"_L1;
        constexpr auto alternativeSpeedLimitsBeginTimeKey = "alt-speed-time-begin"_L1;
        constexpr auto alternativeSpeedLimitsEndTimeKey = "alt-speed-time-end"_L1;
        constexpr auto alternativeSpeedLimitsDaysKey = "alt-speed-time-day"_L1;

        constexpr auto peerPortKey = "peer-port"_L1;
        constexpr auto randomPortEnabledKey = "peer-port-random-on-start"_L1;
        constexpr auto portForwardingEnabledKey = "port-forwarding-enabled"_L1;

        constexpr auto encryptionModeKey = "encryption"_L1;

        constexpr auto utpEnabledKey = "utp-enabled"_L1;
        constexpr auto pexEnabledKey = "pex-enabled"_L1;
        constexpr auto dhtEnabledKey = "dht-enabled"_L1;
        constexpr auto lpdEnabledKey = "lpd-enabled"_L1;
        constexpr auto maximumPeersPerTorrentKey = "peer-limit-per-torrent"_L1;
        constexpr auto maximumPeersGloballyKey = "peer-limit-global"_L1;

        constexpr auto alternativeSpeedLimitsDaysMapper = EnumMapper(
            std::array{
                // (1 << 0)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Sunday, 1),
                // (1 << 1)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Monday, 2),
                // (1 << 2)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Tuesday, 4),
                // (1 << 3)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Wednesday, 8),
                // (1 << 4)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Thursday, 16),
                // (1 << 5)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Friday, 32),
                // (1 << 6)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Saturday, 64),
                // (Monday | Tuesday | Wednesday | Thursday | Friday)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Weekdays, 62),
                // (Sunday | Saturday)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::Weekends, 65),
                // (Weekdays | Weekends)
                EnumMapping(ServerSettingsData::AlternativeSpeedLimitsDays::All, 127),
            }
        );

        constexpr auto encryptionModeMapper = EnumMapper(
            std::array{
                EnumMapping(ServerSettingsData::EncryptionMode::Allowed, "tolerated"_L1),
                EnumMapping(ServerSettingsData::EncryptionMode::Preferred, "preferred"_L1),
                EnumMapping(ServerSettingsData::EncryptionMode::Required, "required"_L1)
            }
        );

        constexpr auto autoPrioritySortMapper = EnumMapper(
            std::array{
                EnumMapping(ServerSettingsData::AutoPrioritySort::Id, "id"_L1),
                EnumMapping(ServerSettingsData::AutoPrioritySort::Name, "name"_L1),
                EnumMapping(ServerSettingsData::AutoPrioritySort::Size, "size"_L1)
            }
        );
    }

    bool ServerSettingsData::canRenameFiles() const { return (rpcVersion >= 15); }

    bool ServerSettingsData::canShowFreeSpaceForPath() const { return (rpcVersion >= 15); }

    bool ServerSettingsData::hasSessionIdFile() const { return (rpcVersion >= 16); }

    bool ServerSettingsData::hasTableMode() const { return (rpcVersion >= 16); }

    bool ServerSettingsData::hasTrackerListProperty() const { return (rpcVersion >= 17); }

    bool ServerSettingsData::hasLabelsProperty() const { return (rpcVersion >= 16); }

    bool ServerSettingsData::hasForkFeature(QLatin1String feature) const {
        const QString name = feature.toString();
        for (const auto& forkFeature : forkFeatures) {
            if (forkFeature == name || forkFeature.startsWith(name + u'/')) {
                return true;
            }
        }
        return false;
    }

    bool ServerSettingsData::hasScriptRunner() const {
        return scriptRunner.has_value() && hasForkFeature("script_runner/v1"_L1);
    }

    bool ServerSettingsData::hasScriptRunnerSettings() const {
        return scriptRunner.has_value() && hasForkFeature("script_runner_settings/v1"_L1);
    }

    bool ServerSettingsData::hasRestartDaemon() const {
        return scriptRunner.has_value()
               && scriptRunner->restartDaemon.enabled
               && hasForkFeature("script_restart_daemon/v1"_L1);
    }

    ServerSettings::ServerSettings(Rpc* rpc, QObject* parent) : QObject(parent), mRpc(rpc), mSaveOnSet(true) {}

    void ServerSettings::setDownloadDirectory(const QString& directory) {
        if (directory != mData.downloadDirectory) {
            mData.downloadDirectory = directory;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(downloadDirectoryKey, mData.downloadDirectory);
            }
        }
    }

    void ServerSettings::setStartAddedTorrents(bool start) {
        mData.startAddedTorrents = start;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(startAddedTorrentsKey, mData.startAddedTorrents);
        }
    }

    void ServerSettings::setTrashTorrentFiles(bool trash) {
        mData.trashTorrentFiles = trash;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(trashTorrentFilesKey, mData.trashTorrentFiles);
        }
    }

    void ServerSettings::setRenameIncompleteFiles(bool rename) {
        mData.renameIncompleteFiles = rename;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(renameIncompleteFilesKey, mData.renameIncompleteFiles);
        }
    }

    void ServerSettings::setSequentialDownload(bool enabled) {
        mData.sequentialDownload = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(sequentialDownloadKey, mData.sequentialDownload);
        }
    }

    void ServerSettings::setIncompleteDirectoryEnabled(bool enabled) {
        mData.incompleteDirectoryEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(incompleteDirectoryEnabledKey, mData.incompleteDirectoryEnabled);
        }
    }

    void ServerSettings::setIncompleteDirectory(const QString& directory) {
        if (directory != mData.incompleteDirectory) {
            mData.incompleteDirectory = directory;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(incompleteDirectoryKey, mData.incompleteDirectory);
            }
        }
    }

    void ServerSettings::setRatioLimited(bool limited) {
        mData.ratioLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(ratioLimitedKey, mData.ratioLimited);
        }
    }

    void ServerSettings::setRatioLimit(double limit) {
        mData.ratioLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(ratioLimitKey, mData.ratioLimit);
        }
    }

    void ServerSettings::setIdleSeedingLimited(bool limited) {
        mData.idleSeedingLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleSeedingLimitedKey, mData.idleSeedingLimited);
        }
    }

    void ServerSettings::setIdleSeedingLimit(int limit) {
        mData.idleSeedingLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleSeedingLimitKey, mData.idleSeedingLimit);
        }
    }

    void ServerSettings::setDownloadQueueEnabled(bool enabled) {
        mData.downloadQueueEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadQueueEnabledKey, mData.downloadQueueEnabled);
        }
    }

    void ServerSettings::setDownloadQueueSize(int size) {
        mData.downloadQueueSize = size;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadQueueSizeKey, mData.downloadQueueSize);
        }
    }

    void ServerSettings::setSeedQueueEnabled(bool enabled) {
        mData.seedQueueEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(seedQueueEnabledKey, mData.seedQueueEnabled);
        }
    }

    void ServerSettings::setSeedQueueSize(int size) {
        mData.seedQueueSize = size;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(seedQueueSizeKey, mData.seedQueueSize);
        }
    }

    void ServerSettings::setIdleQueueLimited(bool limited) {
        mData.idleQueueLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleQueueLimitedKey, mData.idleQueueLimited);
        }
    }

    void ServerSettings::setIdleQueueLimit(int limit) {
        mData.idleQueueLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleQueueLimitKey, mData.idleQueueLimit);
        }
    }

    void ServerSettings::setAutoPriorityEnabled(bool enabled) {
        mData.autoPriorityEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(autoPriorityEnabledKey, mData.autoPriorityEnabled);
        }
    }

    void ServerSettings::setAutoPriorityMaxHigh(int count) {
        mData.autoPriorityMaxHigh = count;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(autoPriorityMaxHighKey, mData.autoPriorityMaxHigh);
        }
    }

    void ServerSettings::setAutoPrioritySort(ServerSettingsData::AutoPrioritySort sort) {
        mData.autoPrioritySort = sort;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(autoPrioritySortKey, autoPrioritySortMapper.toJsonConstant(sort));
        }
    }

    void ServerSettings::setQueueMoveToEndEnabled(bool enabled) {
        mData.queueMoveToEndEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(queueMoveToEndEnabledKey, mData.queueMoveToEndEnabled);
        }
    }

    void ServerSettings::setQueueMoveToEndMinutes(int minutes) {
        mData.queueMoveToEndMinutes = minutes;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(queueMoveToEndMinutesKey, mData.queueMoveToEndMinutes);
        }
    }

    void ServerSettings::setMinFreeSpace(const QStringList& entries) {
        mData.minFreeSpace = entries;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(minFreeSpaceKey, QJsonArray::fromStringList(mData.minFreeSpace));
        }
    }

    void ServerSettings::setMinFreeSpaceResumeMargin(const QString& margin) {
        mData.minFreeSpaceResumeMargin = margin;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(minFreeSpaceResumeMarginKey, mData.minFreeSpaceResumeMargin);
        }
    }

    void ServerSettings::setMoveCompletedTorrentsEnabled(bool enabled) {
        mData.moveCompletedTorrentsEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(moveCompletedTorrentsEnabledKey, mData.moveCompletedTorrentsEnabled);
        }
    }

    void ServerSettings::setMovePartialFilesEnabled(bool enabled) {
        mData.movePartialFilesEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(movePartialFilesEnabledKey, mData.movePartialFilesEnabled);
        }
    }

    void ServerSettings::setForceFinishPartialFilesEnabled(bool enabled) {
        mData.forceFinishPartialFilesEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(forceFinishPartialFilesEnabledKey, mData.forceFinishPartialFilesEnabled);
        }
    }

    void ServerSettings::setScriptTorrentDoneRemoveTorrentEnabled(bool enabled) {
        mData.scriptTorrentDoneRemoveTorrentEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(scriptTorrentDoneRemoveTorrentEnabledKey, mData.scriptTorrentDoneRemoveTorrentEnabled);
        }
    }

    void ServerSettings::setScriptTorrentDoneRemoveTempFilesEnabled(bool enabled) {
        mData.scriptTorrentDoneRemoveTempFilesEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(
                scriptTorrentDoneRemoveTempFilesEnabledKey,
                mData.scriptTorrentDoneRemoveTempFilesEnabled
            );
        }
    }

    void ServerSettings::setScriptTorrentDoneRemoveAllFilesEnabled(bool enabled) {
        mData.scriptTorrentDoneRemoveAllFilesEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(scriptTorrentDoneRemoveAllFilesEnabledKey, mData.scriptTorrentDoneRemoveAllFilesEnabled);
        }
    }

    void ServerSettings::setDownloadSpeedLimited(bool limited) {
        mData.downloadSpeedLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadSpeedLimitedKey, mData.downloadSpeedLimited);
        }
    }

    void ServerSettings::setDownloadSpeedLimit(int limit) {
        mData.downloadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadSpeedLimitKey, mData.downloadSpeedLimit);
        }
    }

    void ServerSettings::setUploadSpeedLimited(bool limited) {
        mData.uploadSpeedLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(uploadSpeedLimitedKey, mData.uploadSpeedLimited);
        }
    }

    void ServerSettings::setUploadSpeedLimit(int limit) {
        mData.uploadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(uploadSpeedLimitKey, mData.uploadSpeedLimit);
        }
    }

    void ServerSettings::setAlternativeSpeedLimitsEnabled(bool enabled) {
        mData.alternativeSpeedLimitsEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsEnabledKey, mData.alternativeSpeedLimitsEnabled);
        }
    }

    void ServerSettings::setAlternativeDownloadSpeedLimit(int limit) {
        mData.alternativeDownloadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeDownloadSpeedLimitKey, mData.alternativeDownloadSpeedLimit);
        }
    }

    void ServerSettings::setAlternativeUploadSpeedLimit(int limit) {
        mData.alternativeUploadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeUploadSpeedLimitKey, mData.alternativeUploadSpeedLimit);
        }
    }

    void ServerSettings::setAlternativeSpeedLimitsScheduled(bool scheduled) {
        mData.alternativeSpeedLimitsScheduled = scheduled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsScheduledKey, mData.alternativeSpeedLimitsScheduled);
        }
    }

    void ServerSettings::setAlternativeSpeedLimitsBeginTime(QTime time) {
        mData.alternativeSpeedLimitsBeginTime = time;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(
                alternativeSpeedLimitsBeginTimeKey,
                mData.alternativeSpeedLimitsBeginTime.msecsSinceStartOfDay() / 60000
            );
        }
    }

    void ServerSettings::setAlternativeSpeedLimitsEndTime(QTime time) {
        mData.alternativeSpeedLimitsEndTime = time;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(
                alternativeSpeedLimitsEndTimeKey,
                mData.alternativeSpeedLimitsEndTime.msecsSinceStartOfDay() / 60000
            );
        }
    }

    void ServerSettings::setAlternativeSpeedLimitsDays(ServerSettingsData::AlternativeSpeedLimitsDays days) {
        if (days != mData.alternativeSpeedLimitsDays) {
            mData.alternativeSpeedLimitsDays = days;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(
                    alternativeSpeedLimitsDaysKey,
                    alternativeSpeedLimitsDaysMapper.toJsonConstant(days)
                );
            }
        }
    }

    void ServerSettings::setPeerPort(int port) {
        mData.peerPort = port;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(peerPortKey, mData.peerPort);
        }
    }

    void ServerSettings::setRandomPortEnabled(bool enabled) {
        mData.randomPortEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(randomPortEnabledKey, mData.randomPortEnabled);
        }
    }

    void ServerSettings::setPortForwardingEnabled(bool enabled) {
        mData.portForwardingEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(portForwardingEnabledKey, mData.portForwardingEnabled);
        }
    }

    void ServerSettings::setEncryptionMode(ServerSettingsData::EncryptionMode mode) {
        mData.encryptionMode = mode;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(encryptionModeKey, encryptionModeMapper.toJsonConstant(mode));
        }
    }

    void ServerSettings::setUtpEnabled(bool enabled) {
        mData.utpEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(utpEnabledKey, mData.utpEnabled);
        }
    }

    void ServerSettings::setPexEnabled(bool enabled) {
        mData.pexEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(pexEnabledKey, mData.pexEnabled);
        }
    }

    void ServerSettings::setDhtEnabled(bool enabled) {
        mData.dhtEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(dhtEnabledKey, mData.dhtEnabled);
        }
    }

    void ServerSettings::setLpdEnabled(bool enabled) {
        mData.lpdEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(lpdEnabledKey, mData.lpdEnabled);
        }
    }

    void ServerSettings::setMaximumPeersPerTorrent(int peers) {
        mData.maximumPeersPerTorrent = peers;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(maximumPeersPerTorrentKey, mData.maximumPeersPerTorrent);
        }
    }

    void ServerSettings::setMaximumPeersGlobally(int peers) {
        mData.maximumPeersGlobally = peers;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(maximumPeersGloballyKey, mData.maximumPeersGlobally);
        }
    }

    bool ServerSettings::saveOnSet() const { return mSaveOnSet; }

    void ServerSettings::setSaveOnSet(bool save) { mSaveOnSet = save; }

    void ServerSettings::update(const QJsonObject& serverSettings) {
        bool changed = false;

        mData.rpcVersion = serverSettings.value("rpc-version"_L1).toInt();
        mData.minimumRpcVersion = serverSettings.value("rpc-version-minimum"_L1).toInt();

        QStringList forkFeatures;
        for (const auto& feature : serverSettings.value("fork-features"_L1).toArray()) {
            forkFeatures.append(feature.toString());
        }
        setChanged(mData.forkFeatures, std::move(forkFeatures), changed);

        const auto scriptRunnerValue = serverSettings.value("script_runner"_L1);
        if (mData.hasForkFeature("script_runner/v1"_L1) && scriptRunnerValue.isObject()) {
            const auto revision = scriptRunnerValue.toObject().value("revision"_L1).toString();
            if (!mData.scriptRunner.has_value() || mData.scriptRunner->revision != revision) {
                auto parsed = parseScriptRunner(scriptRunnerValue.toObject());
                if (parsed.has_value()) {
                    mData.scriptRunner = std::move(parsed);
                } else {
                    mData.scriptRunner.reset();
                }
                changed = true;
            }
        } else if (mData.scriptRunner.has_value()) {
            mData.scriptRunner.reset();
            changed = true;
        }

        if (const auto newConfigDir = serverSettings.value("config-dir"_L1).toString();
            newConfigDir != mData.configDirectory) {
            mData.configDirectory = newConfigDir;
            // Transmission's config directory is likely located under 'C:\Users' on Windows
            // If config-dir somehow is an UNC path then we are out of luck - we can't reliably distinguish
            // between Unix path and Windows UNC path unless we already know the OS. And that's what we are trying to determine here
            mData.pathOs = isAbsoluteWindowsDOSFilePath(mData.configDirectory) ? PathOs::Windows : PathOs::Unix;
        }

        setChanged(
            mData.downloadDirectory,
            normalizePath(serverSettings.value(downloadDirectoryKey).toString(), mData.pathOs),
            changed
        );
        setChanged(mData.trashTorrentFiles, serverSettings.value(trashTorrentFilesKey).toBool(), changed);
        setChanged(mData.startAddedTorrents, serverSettings.value(startAddedTorrentsKey).toBool(), changed);
        setChanged(mData.renameIncompleteFiles, serverSettings.value(renameIncompleteFilesKey).toBool(), changed);
        mData.hasSequentialDownload = serverSettings.contains(sequentialDownloadKey);
        setChanged(
            mData.sequentialDownload,
            mData.hasSequentialDownload && serverSettings.value(sequentialDownloadKey).toBool(),
            changed
        );
        setChanged(
            mData.incompleteDirectoryEnabled,
            serverSettings.value(incompleteDirectoryEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.incompleteDirectory,
            normalizePath(serverSettings.value(incompleteDirectoryKey).toString(), mData.pathOs),
            changed
        );

        setChanged(mData.ratioLimited, serverSettings.value(ratioLimitedKey).toBool(), changed);
        setChanged(mData.ratioLimit, serverSettings.value(ratioLimitKey).toDouble(), changed);
        setChanged(mData.idleSeedingLimited, serverSettings.value(idleSeedingLimitedKey).toBool(), changed);
        setChanged(mData.idleSeedingLimit, serverSettings.value(idleSeedingLimitKey).toInt(), changed);

        setChanged(mData.downloadQueueEnabled, serverSettings.value(downloadQueueEnabledKey).toBool(), changed);
        setChanged(mData.downloadQueueSize, serverSettings.value(downloadQueueSizeKey).toInt(), changed);
        setChanged(mData.seedQueueEnabled, serverSettings.value(seedQueueEnabledKey).toBool(), changed);
        setChanged(mData.seedQueueSize, serverSettings.value(seedQueueSizeKey).toInt(), changed);
        setChanged(mData.idleQueueLimited, serverSettings.value(idleQueueLimitedKey).toBool(), changed);
        setChanged(mData.idleQueueLimit, serverSettings.value(idleQueueLimitKey).toInt(), changed);

        setChanged(mData.autoPriorityEnabled, serverSettings.value(autoPriorityEnabledKey).toBool(), changed);
        setChanged(mData.autoPriorityMaxHigh, serverSettings.value(autoPriorityMaxHighKey).toInt(), changed);
        setChanged(
            mData.autoPrioritySort,
            autoPrioritySortMapper.fromJsonValue(serverSettings.value(autoPrioritySortKey), autoPrioritySortKey),
            changed
        );
        setChanged(mData.queueMoveToEndEnabled, serverSettings.value(queueMoveToEndEnabledKey).toBool(), changed);
        setChanged(mData.queueMoveToEndMinutes, serverSettings.value(queueMoveToEndMinutesKey).toInt(), changed);

        setChanged(
            mData.minFreeSpace,
            serverSettings.value(minFreeSpaceKey).toVariant().toStringList(),
            changed
        );
        setChanged(
            mData.minFreeSpaceResumeMargin,
            serverSettings.value(minFreeSpaceResumeMarginKey).toString(),
            changed
        );
        setChanged(
            mData.moveCompletedTorrentsEnabled,
            serverSettings.value(moveCompletedTorrentsEnabledKey).toBool(),
            changed
        );
        setChanged(mData.movePartialFilesEnabled, serverSettings.value(movePartialFilesEnabledKey).toBool(), changed);
        setChanged(
            mData.forceFinishPartialFilesEnabled,
            serverSettings.value(forceFinishPartialFilesEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.scriptTorrentDoneRemoveTorrentEnabled,
            serverSettings.value(scriptTorrentDoneRemoveTorrentEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.scriptTorrentDoneRemoveTempFilesEnabled,
            serverSettings.value(scriptTorrentDoneRemoveTempFilesEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.scriptTorrentDoneRemoveAllFilesEnabled,
            serverSettings.value(scriptTorrentDoneRemoveAllFilesEnabledKey).toBool(),
            changed
        );

        setChanged(mData.downloadSpeedLimited, serverSettings.value(downloadSpeedLimitedKey).toBool(), changed);
        setChanged(mData.downloadSpeedLimit, serverSettings.value(downloadSpeedLimitKey).toInt(), changed);
        setChanged(mData.uploadSpeedLimited, serverSettings.value(uploadSpeedLimitedKey).toBool(), changed);
        setChanged(mData.uploadSpeedLimit, serverSettings.value(uploadSpeedLimitKey).toInt(), changed);
        setChanged(
            mData.alternativeSpeedLimitsEnabled,
            serverSettings.value(alternativeSpeedLimitsEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.alternativeDownloadSpeedLimit,
            serverSettings.value(alternativeDownloadSpeedLimitKey).toInt(),
            changed
        );
        setChanged(
            mData.alternativeUploadSpeedLimit,
            serverSettings.value(alternativeUploadSpeedLimitKey).toInt(),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsScheduled,
            serverSettings.value(alternativeSpeedLimitsScheduledKey).toBool(),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsBeginTime,
            QTime::fromMSecsSinceStartOfDay(serverSettings.value(alternativeSpeedLimitsBeginTimeKey).toInt() * 60000),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsEndTime,
            QTime::fromMSecsSinceStartOfDay(serverSettings.value(alternativeSpeedLimitsEndTimeKey).toInt() * 60000),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsDays,
            alternativeSpeedLimitsDaysMapper
                .fromJsonValue(serverSettings.value(alternativeSpeedLimitsDaysKey), alternativeDownloadSpeedLimitKey),
            changed
        );

        setChanged(mData.peerPort, serverSettings.value(peerPortKey).toInt(), changed);
        setChanged(mData.randomPortEnabled, serverSettings.value(randomPortEnabledKey).toBool(), changed);
        setChanged(mData.portForwardingEnabled, serverSettings.value(portForwardingEnabledKey).toBool(), changed);
        setChanged(
            mData.encryptionMode,
            encryptionModeMapper.fromJsonValue(serverSettings.value(encryptionModeKey), encryptionModeKey),
            changed
        );
        setChanged(mData.utpEnabled, serverSettings.value(utpEnabledKey).toBool(), changed);
        setChanged(mData.pexEnabled, serverSettings.value(pexEnabledKey).toBool(), changed);
        setChanged(mData.dhtEnabled, serverSettings.value(dhtEnabledKey).toBool(), changed);
        setChanged(mData.lpdEnabled, serverSettings.value(lpdEnabledKey).toBool(), changed);
        setChanged(mData.maximumPeersPerTorrent, serverSettings.value(maximumPeersPerTorrentKey).toInt(), changed);
        setChanged(mData.maximumPeersGlobally, serverSettings.value(maximumPeersGloballyKey).toInt(), changed);

        if (changed) {
            emit this->changed();
        }
    }

    void ServerSettings::save() const {
        mRpc->setSessionProperties(
            {{downloadDirectoryKey, mData.downloadDirectory},
             {trashTorrentFilesKey, mData.trashTorrentFiles},
             {startAddedTorrentsKey, mData.startAddedTorrents},
             {renameIncompleteFilesKey, mData.renameIncompleteFiles},
             {sequentialDownloadKey, mData.sequentialDownload},
             {incompleteDirectoryEnabledKey, mData.incompleteDirectoryEnabled},
             {incompleteDirectoryKey, mData.incompleteDirectory},

             {ratioLimitedKey, mData.ratioLimited},
             {ratioLimitKey, mData.ratioLimit},
             {idleSeedingLimitedKey, mData.idleSeedingLimit},
             {idleSeedingLimitKey, mData.idleSeedingLimit},

             {downloadQueueEnabledKey, mData.downloadQueueEnabled},
             {downloadQueueSizeKey, mData.downloadQueueSize},
             {seedQueueEnabledKey, mData.seedQueueEnabled},
             {seedQueueSizeKey, mData.seedQueueSize},
             {idleQueueLimitedKey, mData.idleQueueLimited},
             {idleQueueLimitKey, mData.idleQueueLimit},

             {downloadSpeedLimitedKey, mData.downloadSpeedLimited},
             {downloadSpeedLimitKey, mData.downloadSpeedLimit},
             {uploadSpeedLimitedKey, mData.uploadSpeedLimited},
             {uploadSpeedLimitKey, mData.uploadSpeedLimit},
             {alternativeSpeedLimitsEnabledKey, mData.alternativeSpeedLimitsEnabled},
             {alternativeDownloadSpeedLimitKey, mData.alternativeDownloadSpeedLimit},
             {alternativeUploadSpeedLimitKey, mData.alternativeUploadSpeedLimit},
             {alternativeSpeedLimitsScheduledKey, mData.alternativeSpeedLimitsScheduled},
             {alternativeSpeedLimitsBeginTimeKey, mData.alternativeSpeedLimitsBeginTime.msecsSinceStartOfDay() / 60000},
             {alternativeSpeedLimitsEndTimeKey, mData.alternativeSpeedLimitsEndTime.msecsSinceStartOfDay() / 60000},
             {alternativeSpeedLimitsDaysKey,
              alternativeSpeedLimitsDaysMapper.toJsonConstant(mData.alternativeSpeedLimitsDays)},

             {peerPortKey, mData.peerPort},
             {randomPortEnabledKey, mData.randomPortEnabled},
             {portForwardingEnabledKey, mData.portForwardingEnabled},
             {encryptionModeKey, encryptionModeMapper.toJsonConstant(mData.encryptionMode)},
             {utpEnabledKey, mData.utpEnabled},
             {pexEnabledKey, mData.pexEnabled},
             {dhtEnabledKey, mData.dhtEnabled},
             {lpdEnabledKey, mData.lpdEnabled},
             {maximumPeersPerTorrentKey, mData.maximumPeersPerTorrent},
             {maximumPeersGloballyKey, mData.maximumPeersGlobally}}
        );
    }
}
