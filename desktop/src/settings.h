// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
// SPDX-FileCopyrightText: 2021 LuK1337
// SPDX-FileCopyrightText: 2022 Alex <tabell@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_SETTINGS_H
#define TREMOTESF_SETTINGS_H

#include <QObject>

#include "rpc/torrent.h"
#include "ui/screens/mainwindow/torrentsproxymodel.h"

class QSettings;

#define SETTINGS_PROPERTY(type, name) \
public:                               \
    type get_##name() const;          \
    void set_##name(type value);      \
Q_SIGNALS:                            \
    void name##Changed();

namespace tremotesf {
    class Settings final : public QObject {
        Q_OBJECT

        SETTINGS_PROPERTY(bool, connectOnStartup)
        SETTINGS_PROPERTY(bool, notificationOnDisconnecting)
        SETTINGS_PROPERTY(bool, notificationOnAddingTorrent)
        SETTINGS_PROPERTY(bool, notificationOfFinishedTorrents)
        SETTINGS_PROPERTY(bool, notificationsOnAddedTorrentsSinceLastConnection)
        SETTINGS_PROPERTY(bool, notificationsOnFinishedTorrentsSinceLastConnection)

        SETTINGS_PROPERTY(bool, rememberOpenTorrentDir)
        SETTINGS_PROPERTY(QString, lastOpenTorrentDirectory)
        SETTINGS_PROPERTY(bool, rememberAddTorrentParameters)
        SETTINGS_PROPERTY(TorrentData::Priority, lastAddTorrentPriority)
        SETTINGS_PROPERTY(bool, lastAddTorrentStartAfterAdding)
        SETTINGS_PROPERTY(bool, lastAddTorrentSequentialDownload)
        SETTINGS_PROPERTY(bool, lastAddTorrentMetadataOnly)
        SETTINGS_PROPERTY(bool, lastAddTorrentDeleteTorrentFile)
        SETTINGS_PROPERTY(bool, lastAddTorrentMoveTorrentFileToTrash)

        SETTINGS_PROPERTY(bool, fillTorrentLinkFromClipboard)

        SETTINGS_PROPERTY(bool, showMainWindowWhenAddingTorrent)
        SETTINGS_PROPERTY(bool, showAddTorrentDialog)

        SETTINGS_PROPERTY(bool, mergeTrackersWhenAddingExistingTorrent)
        SETTINGS_PROPERTY(bool, askForMergingTrackersWhenAddingExistingTorrent)

        SETTINGS_PROPERTY(bool, torrentsStatusFilterEnabled)
        SETTINGS_PROPERTY(TorrentsProxyModel::StatusFilter, torrentsStatusFilter)

        SETTINGS_PROPERTY(bool, torrentsLabelFilterEnabled)
        SETTINGS_PROPERTY(QString, torrentsLabelFilter)

        SETTINGS_PROPERTY(bool, torrentsTrackerFilterEnabled)
        SETTINGS_PROPERTY(QString, torrentsTrackerFilter)

        SETTINGS_PROPERTY(bool, torrentsDownloadDirectoryFilterEnabled)
        SETTINGS_PROPERTY(QString, torrentsDownloadDirectoryFilter)

        SETTINGS_PROPERTY(bool, showTrayIcon)
        SETTINGS_PROPERTY(Qt::ToolButtonStyle, toolButtonStyle)
        SETTINGS_PROPERTY(bool, toolBarLocked)
        SETTINGS_PROPERTY(bool, enlargedIcons)
        SETTINGS_PROPERTY(QString, scriptRunnerClientId)
        SETTINGS_PROPERTY(QString, scriptRunParameters)
        SETTINGS_PROPERTY(QString, whisparrUrl)
        SETTINGS_PROPERTY(QString, whisparrApiKey)
        SETTINGS_PROPERTY(bool, whisparrIgnoreSslErrors)
        SETTINGS_PROPERTY(bool, checkForUpdates)
        SETTINGS_PROPERTY(bool, sideBarVisible)
        SETTINGS_PROPERTY(bool, statusBarVisible)
        SETTINGS_PROPERTY(bool, showTorrentPropertiesInMainWindow)

        SETTINGS_PROPERTY(QByteArray, mainWindowGeometry)
        SETTINGS_PROPERTY(QByteArray, mainWindowState)
        SETTINGS_PROPERTY(QByteArray, horizontalSplitterState)
        SETTINGS_PROPERTY(QByteArray, verticalSplitterState)

        SETTINGS_PROPERTY(QByteArray, torrentsViewHeaderState)
        SETTINGS_PROPERTY(QByteArray, torrentPropertiesDialogGeometry)
        SETTINGS_PROPERTY(QByteArray, torrentFilesViewHeaderState)
        SETTINGS_PROPERTY(QByteArray, trackersViewHeaderState)
        SETTINGS_PROPERTY(QByteArray, peersViewHeaderState)
        SETTINGS_PROPERTY(QByteArray, localTorrentFilesViewHeaderState)
        SETTINGS_PROPERTY(QByteArray, serverSettingsDialogGeometry)
        SETTINGS_PROPERTY(QByteArray, minFreeSpaceTableHeaderState)
        SETTINGS_PROPERTY(QByteArray, archiveViewDialogGeometry)
        SETTINGS_PROPERTY(QByteArray, archiveViewHeaderState)
        SETTINGS_PROPERTY(QByteArray, whisparrRenameDialogHeaderState)
        SETTINGS_PROPERTY(QByteArray, whisparrRenameDialogMultiHeaderState)

        SETTINGS_PROPERTY(bool, displayRelativeTime)

        SETTINGS_PROPERTY(QString, language)

        SETTINGS_PROPERTY(bool, displayFullDownloadDirectoryPath)

    public:
        enum class DarkThemeMode { FollowSystem, On, Off };
        Q_ENUM(DarkThemeMode)

        enum class CloseBehavior { HideToTray, Quit };
        Q_ENUM(CloseBehavior)

        enum class TorrentDoubleClickAction { OpenPropertiesDialog, OpenTorrentFile, OpenDownloadDirectory };
        Q_ENUM(TorrentDoubleClickAction)

        SETTINGS_PROPERTY(Settings::DarkThemeMode, darkThemeMode)
        SETTINGS_PROPERTY(Settings::CloseBehavior, closeBehavior)
        SETTINGS_PROPERTY(bool, useSystemAccentColor)
        SETTINGS_PROPERTY(Settings::TorrentDoubleClickAction, torrentDoubleClickAction)

    public:
        static Settings* instance();
        void sync();

    private:
        explicit Settings(QObject* parent = nullptr);

        QSettings* mSettings{};
    };
}

#endif // TREMOTESF_SETTINGS_H
