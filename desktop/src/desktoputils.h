// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_DESKTOPUTILS_H
#define TREMOTESF_DESKTOPUTILS_H

#include <QtGlobal>

class QFont;
class QIcon;
class QString;
class QTextDocument;
class QWidget;

namespace tremotesf::desktoputils {
    constexpr int defaultDbusTimeout = 2000; // 2 seconds

    /// Icon size in logical pixels for the given device pixel ratio
    [[nodiscard]] int iconSizeForDevicePixelRatio(qreal devicePixelRatio, bool enlarged = false);

    /// Font scaled proportionally to the enlarged icon size for the given device pixel ratio
    [[nodiscard]] QFont scaledFont(const QFont& font, qreal devicePixelRatio, bool enlarged);

    enum class StatusIcon {
        ActiveDownload,
        ActiveUpload,
        All,
        CheckWaiting,
        Checking,
        CriticalError,
        Done,
        Downloading,
        Inactive,
        Paused,
        QueuedForDownloading,
        QueuedForSeeding,
        Seeding,
        Speedometer,
        TrackerErrorDownloading,
        TrackerErrorSeeding,
        Waiting,
        PausedDueToSpace,
        WaitingForSpaceToMove,
        RunningScript,
        Moving
    };
    const QIcon& statusIcon(StatusIcon icon);

    enum class Priority { High, Normal, Low };
    const QIcon& priorityIcon(Priority priority);

    const QIcon& standardFileIcon();
    const QIcon& standardDirIcon();

    void openFile(const QString& filePath, QWidget* parent = nullptr);

    void findLinksAndAddAnchors(QTextDocument* document);
}

#endif // TREMOTESF_DESKTOPUTILS_H
