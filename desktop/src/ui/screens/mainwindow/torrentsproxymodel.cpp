// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>

#include "torrentsproxymodel.h"

#include "rpc/torrent.h"
#include "rpc/tracker.h"
#include "settings.h"
#include "torrentsmodel.h"

namespace tremotesf {
    class SortFilterProxyModelHelper final {
    public:
        template<std::invocable Func>
        static void changeRowsFilter(TorrentsProxyModel* model, Func&& func) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            model->beginFilterChange();
#endif
            func();
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
            model->endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
            model->invalidateRowsFilter();
#endif
        }
    };

    TorrentsProxyModel::TorrentsProxyModel(TorrentsModel* sourceModel, QObject* parent)
        : BaseProxyModel(
              sourceModel,
              static_cast<int>(TorrentsModel::Role::Sort),
              static_cast<int>(TorrentsModel::Column::Name),
              parent
          ),
          mStatusFilterEnabled(Settings::instance()->get_torrentsStatusFilterEnabled()),
          mStatusFilter(Settings::instance()->get_torrentsStatusFilter()),
          mLabelFilterEnabled(Settings::instance()->get_torrentsLabelFilterEnabled()),
          mLabelFilter(Settings::instance()->get_torrentsLabelFilter()),
          mTrackerFilterEnabled(Settings::instance()->get_torrentsTrackerFilterEnabled()),
          mTrackerFilter(Settings::instance()->get_torrentsTrackerFilter()),
          mDownloadDirectoryFilterEnabled(Settings::instance()->get_torrentsDownloadDirectoryFilterEnabled()),
          mDownloadDirectoryFilter(Settings::instance()->get_torrentsDownloadDirectoryFilter()) {}

    QString TorrentsProxyModel::searchString() const { return mSearchString; }

    void TorrentsProxyModel::setSearchString(const QString& string) {
        if (string != mSearchString) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mSearchString = string; });
        }
    }

    bool TorrentsProxyModel::isStatusFilterEnabled() const { return mStatusFilterEnabled; }

    void TorrentsProxyModel::setStatusFilterEnabled(bool enabled) {
        if (enabled != mStatusFilterEnabled) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mStatusFilterEnabled = enabled; });
            Settings::instance()->set_torrentsStatusFilterEnabled(mStatusFilterEnabled);
        }
    }

    TorrentsProxyModel::StatusFilter TorrentsProxyModel::statusFilter() const { return mStatusFilter; }

    void TorrentsProxyModel::setStatusFilter(TorrentsProxyModel::StatusFilter filter) {
        if (filter != mStatusFilter) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mStatusFilter = filter; });
            emit statusFilterChanged();
            Settings::instance()->set_torrentsStatusFilter(mStatusFilter);
        }
    }

    bool TorrentsProxyModel::isLabelFilterEnabled() const { return mLabelFilterEnabled; }

    void TorrentsProxyModel::setLabelFilterEnabled(bool enabled) {
        if (enabled != mLabelFilterEnabled) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mLabelFilterEnabled = enabled; });
            Settings::instance()->set_torrentsLabelFilterEnabled(mLabelFilterEnabled);
        }
    }

    QString TorrentsProxyModel::labelFilter() const { return mLabelFilter; }

    void TorrentsProxyModel::setLabelFilter(const QString& filter) {
        if (filter != mLabelFilter) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mLabelFilter = filter; });
            emit labelFilterChanged();
            Settings::instance()->set_torrentsLabelFilter(mLabelFilter);
        }
    }

    bool TorrentsProxyModel::isTrackerFilterEnabled() const { return mTrackerFilterEnabled; }

    void TorrentsProxyModel::setTrackerFilterEnabled(bool enabled) {
        if (enabled != mTrackerFilterEnabled) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mTrackerFilterEnabled = enabled; });
            Settings::instance()->set_torrentsTrackerFilterEnabled(mTrackerFilterEnabled);
        }
    }

    QString TorrentsProxyModel::trackerFilter() const { return mTrackerFilter; }

    void TorrentsProxyModel::setTrackerFilter(const QString& filter) {
        if (filter != mTrackerFilter) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mTrackerFilter = filter; });
            emit trackerFilterChanged();
            Settings::instance()->set_torrentsTrackerFilter(mTrackerFilter);
        }
    }

    bool TorrentsProxyModel::isDownloadDirectoryFilterEnabled() const { return mDownloadDirectoryFilterEnabled; }

    void TorrentsProxyModel::setDownloadDirectoryFilterEnabled(bool enabled) {
        if (enabled != mDownloadDirectoryFilterEnabled) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] {
                mDownloadDirectoryFilterEnabled = enabled;
            });
            Settings::instance()->set_torrentsDownloadDirectoryFilterEnabled(mDownloadDirectoryFilterEnabled);
        }
    }

    QString TorrentsProxyModel::downloadDirectoryFilter() const { return mDownloadDirectoryFilter; }

    void TorrentsProxyModel::setDownloadDirectoryFilter(const QString& filter) {
        if (filter != mDownloadDirectoryFilter) {
            SortFilterProxyModelHelper::changeRowsFilter(this, [&, this] { mDownloadDirectoryFilter = filter; });
            emit downloadDirectoryFilterChanged();
            Settings::instance()->set_torrentsDownloadDirectoryFilter(mDownloadDirectoryFilter);
        }
    }

    bool TorrentsProxyModel::statusFilterAcceptsTorrent(const Torrent* torrent, StatusFilter filter) {
        using enum StatusFilter;
        const auto& data = torrent->data();
        const bool active = (data.downloadSpeed != 0 || data.uploadSpeed != 0);
        const bool stopped = (data.status == TorrentData::Status::Paused);
        const bool finished = (data.status == TorrentData::Status::Seeding
                               || (stopped && data.leftUntilDone == 0 && data.sizeWhenDone != 0));
        // Server does not report tracker/local errors for torrents that are not running
        const bool errorsOnlyForRunningTorrents = (
            data.status == TorrentData::Status::QueuedForDownloading
            || data.status == TorrentData::Status::Downloading
            || data.status == TorrentData::Status::QueuedForSeeding
            || data.status == TorrentData::Status::Seeding
        );
        const bool hasTrackerError = (
            data.error == TorrentData::Error::TrackerWarning || data.error == TorrentData::Error::TrackerError
        );
        // Tracker errors are not shown for queued torrents, they keep their queue icon
        const bool hasShownTrackerError = (
            hasTrackerError
            && (data.status == TorrentData::Status::Downloading || data.status == TorrentData::Status::Seeding)
        );
        switch (filter) {
        case All:
            return true;
        case Downloading:
            return data.status == TorrentData::Status::Downloading;
        case Completed:
            return finished || (data.error == TorrentData::Error::LocalError && errorsOnlyForRunningTorrents);
        case ActiveDownload:
            return data.status == TorrentData::Status::Downloading && data.downloadSpeed != 0;
        case ActiveUpload:
            return data.status == TorrentData::Status::Seeding && active;
        case Inactive:
            return !active && !stopped;
        case Stopped:
            return stopped;
        case Errored:
            return hasShownTrackerError
                   || (data.error == TorrentData::Error::LocalError && errorsOnlyForRunningTorrents);
        case Waiting:
            switch (data.status) {
            case TorrentData::Status::QueuedForChecking:
            case TorrentData::Status::Checking:
            case TorrentData::Status::QueuedForDownloading:
            case TorrentData::Status::QueuedForSeeding:
            case TorrentData::Status::PausedDueToSpace:
            case TorrentData::Status::WaitingForSpaceToMove:
            case TorrentData::Status::RunningScript:
            case TorrentData::Status::Moving:
                return true;
            default:
                return false;
            }
        }
        return true;
    }

    bool TorrentsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex&) const {
        const Torrent* torrent = static_cast<TorrentsModel*>(sourceModel())->torrentAtRow(sourceRow);

        if (!mSearchString.isEmpty() && !torrent->data().name.contains(mSearchString, Qt::CaseInsensitive)) {
            return false;
        }

        if (mStatusFilterEnabled && !statusFilterAcceptsTorrent(torrent, mStatusFilter)) {
            return false;
        }

        if (mLabelFilterEnabled && !mLabelFilter.isEmpty()) {
            if (!std::ranges::contains(torrent->data().labels, mLabelFilter)) {
                return false;
            }
        }

        if (mTrackerFilterEnabled && !mTrackerFilter.isEmpty()) {
            if (!std::ranges::contains(torrent->data().trackers, mTrackerFilter, &Tracker::site)) {
                return false;
            }
        }

        if (mDownloadDirectoryFilterEnabled && !mDownloadDirectoryFilter.isEmpty()) {
            if (torrent->data().downloadDirectory != mDownloadDirectoryFilter) {
                return false;
            }
        }

        return true;
    }
}
