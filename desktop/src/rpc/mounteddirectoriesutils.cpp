// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mounteddirectoriesutils.h"

#include "rpc.h"
#include "serversettings.h"
#include "servers.h"

#include <QFileInfo>
#include <QStringBuilder>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        // Data can stay in the incomplete (temporary) directory even when everything is downloaded:
        // the server keeps it there until it has enough free space to move it to the download directory.
        bool isTorrentDataIncomplete(const Torrent* torrent) {
            if (torrent->data().leftUntilDone > 0) {
                return true;
            }
            switch (torrent->data().status) {
            case TorrentData::Status::PausedDueToSpace:
            case TorrentData::Status::WaitingForSpaceToMove:
                return true;
            default:
                return false;
            }
        }
    }

    bool isServerLocalOrTorrentIsMounted(const Rpc* rpc, const Torrent* torrent) {
        if (rpc->isLocal()) {
            return true;
        }
        if (!Servers::instance()->currentServerHasMountedDirectories()) {
            return false;
        }
        return !localTorrentDownloadDirectoryPath(rpc, torrent).isEmpty()
               || !localTorrentIncompleteDirectoryPath(rpc, torrent).isEmpty();
    }

    QString localTorrentDownloadDirectoryPath(const Rpc* rpc, const Torrent* torrent) {
        const auto serverSettings = rpc->serverSettings();
        QString directory = torrent->data().downloadDirectory;
        if (!rpc->isLocal() && !directory.isEmpty() && Servers::instance()->currentServerHasMountedDirectories()) {
            directory = Servers::instance()->fromRemoteToLocalDirectory(directory, serverSettings);
        }
        return directory;
    }

    QString localTorrentIncompleteDirectoryPath(const Rpc* rpc, const Torrent* torrent) {
        if (!isTorrentDataIncomplete(torrent)) {
            return {};
        }
        const auto serverSettings = rpc->serverSettings();
        QString directory = torrent->data().incompleteDirectory;
        if (directory.isEmpty() && serverSettings->data().incompleteDirectoryEnabled) {
            directory = serverSettings->data().incompleteDirectory;
        }
        if (directory.isEmpty()) {
            return {};
        }
        if (!rpc->isLocal() && Servers::instance()->currentServerHasMountedDirectories()) {
            directory = Servers::instance()->fromRemoteToLocalDirectory(directory, serverSettings);
        }
        return directory;
    }

    QString localTorrentRootFilePath(const Rpc* rpc, const Torrent* torrent) {
        const QString downloadDirectoryPath = localTorrentDownloadDirectoryPath(rpc, torrent);
        const QString incompleteDirectoryPath = localTorrentIncompleteDirectoryPath(rpc, torrent);

        const bool renameIncompleteFiles = rpc->serverSettings()->data().renameIncompleteFiles;
        const bool incomplete = isTorrentDataIncomplete(torrent);
        const auto& torrentName = torrent->data().name;

        QStringList candidates{};
        const auto addCandidates = [&](const QString& directory) {
            if (directory.isEmpty()) {
                return;
            }
            const QString rootFilePath = directory % '/' % torrentName;
            if (renameIncompleteFiles && incomplete) {
                candidates.push_back(rootFilePath % ".part"_L1);
            }
            candidates.push_back(rootFilePath);
        };
        addCandidates(downloadDirectoryPath);
        addCandidates(incompleteDirectoryPath);

        for (const QString& candidate : candidates) {
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
        if (!downloadDirectoryPath.isEmpty()) {
            return downloadDirectoryPath % '/' % torrentName;
        }
        if (!incompleteDirectoryPath.isEmpty()) {
            return incompleteDirectoryPath % '/' % torrentName;
        }
        return {};
    }
}
