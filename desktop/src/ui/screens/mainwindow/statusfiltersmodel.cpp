// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusfiltersmodel.h"

#include <map>

#include <QCoreApplication>
#include <QIcon>

#include "rpc/rpc.h"
#include "ui/itemmodels/modelutils.h"
#include "desktoputils.h"
#include "torrentsproxymodel.h"

namespace tremotesf {
    QVariant StatusFiltersModel::data(const QModelIndex& index, int role) const {
        using StatusFilter = TorrentsProxyModel::StatusFilter;

        if (!index.isValid()) {
            return {};
        }
        const Item& item = mItems.at(static_cast<size_t>(index.row()));
        switch (role) {
        case FilterRole:
            return QVariant::fromValue(item.filter);
        case Qt::DecorationRole: {
            using enum desktoputils::StatusIcon;

            switch (item.filter) {
            case StatusFilter::All: {
                return desktoputils::standardDirIcon();
            }
            case StatusFilter::Downloading:
            case StatusFilter::ActiveDownload:
                return desktoputils::statusIcon(Downloading);
            case StatusFilter::Completed:
            case StatusFilter::ActiveUpload:
                return desktoputils::statusIcon(Seeding);
            case StatusFilter::Inactive:
                return desktoputils::statusIcon(Queued);
            case StatusFilter::Stopped:
                return desktoputils::statusIcon(Paused);
            case StatusFilter::Errored:
                return desktoputils::statusIcon(Errored);
            case StatusFilter::Waiting:
                return desktoputils::statusIcon(Checking);
            }
            break;
        }
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            switch (item.filter) {
            case StatusFilter::All:
                //: Filter option of torrents list's status filter. %L1 is total number of torrents
                return qApp->translate("tremotesf", "All (%L1)").arg(item.torrents);
            case StatusFilter::Downloading:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Downloading (%L1)").arg(item.torrents);
            case StatusFilter::Completed:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Completed (%L1)").arg(item.torrents);
            case StatusFilter::ActiveDownload:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Active download (%L1)").arg(item.torrents);
            case StatusFilter::ActiveUpload:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Active upload (%L1)").arg(item.torrents);
            case StatusFilter::Inactive:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Inactive (%L1)").arg(item.torrents);
            case StatusFilter::Stopped:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Stopped (%L1)").arg(item.torrents);
            case StatusFilter::Errored:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Error (%L1)").arg(item.torrents);
            case StatusFilter::Waiting:
                //: Filter option of torrents list's status filter. %L1 is a number of torrents with that status
                return qApp->translate("tremotesf", "Waiting (%L1)").arg(item.torrents);
            default:
                break;
            }
            break;
        default:
            break;
        }
        return {};
    }

    int StatusFiltersModel::rowCount(const QModelIndex&) const { return static_cast<int>(mItems.size()); }

    QModelIndex StatusFiltersModel::indexForTorrentsProxyModelFilter() const {
        if (!torrentsProxyModel()) {
            return {};
        }
        const auto filter = torrentsProxyModel()->statusFilter();
        for (size_t i = 0, max = mItems.size(); i < max; ++i) {
            const auto& item = mItems[i];
            if (item.filter == filter) {
                return index(static_cast<int>(i));
            }
        }
        return {};
    }

    void StatusFiltersModel::resetTorrentsProxyModelFilter() const {
        if (torrentsProxyModel()) {
            torrentsProxyModel()->setStatusFilter(TorrentsProxyModel::StatusFilter::All);
        }
    }

    class StatusFiltersModelUpdater final : public ModelListUpdater<
                                                StatusFiltersModel,
                                                StatusFiltersModel::Item,
                                                std::map<TorrentsProxyModel::StatusFilter, int>> {
    public:
        inline explicit StatusFiltersModelUpdater(StatusFiltersModel& model) : ModelListUpdater(model) {}

    protected:
        std::map<TorrentsProxyModel::StatusFilter, int>::iterator findNewItemForItem(
            std::map<TorrentsProxyModel::StatusFilter, int>& newItems, const StatusFiltersModel::Item& item
        ) override {
            return newItems.find(item.filter);
        }

        bool updateItem(
            StatusFiltersModel::Item& item,
            // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
            std::pair<const TorrentsProxyModel::StatusFilter, int>&& newItem
        ) override {
            const auto& [filter, torrents] = newItem;
            if (item.torrents != torrents) {
                item.torrents = torrents;
                return true;
            }
            return false;
        }

        StatusFiltersModel::Item createItemFromNewItem(
            // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
            std::pair<const TorrentsProxyModel::StatusFilter, int>&& newTracker
        ) override {
            return StatusFiltersModel::Item{.filter = newTracker.first, .torrents = newTracker.second};
        }
    };

    void StatusFiltersModel::update() {
        using enum TorrentsProxyModel::StatusFilter;
        std::map<TorrentsProxyModel::StatusFilter, int> items{
            {All, rpc()->torrentsCount()},
            {Downloading, 0},
            {Completed, 0},
            {ActiveDownload, 0},
            {ActiveUpload, 0},
            {Inactive, 0},
            {Stopped, 0},
            {Errored, 0},
            {Waiting, 0}
        };

        const auto processFilter = [&](Torrent* torrent, TorrentsProxyModel::StatusFilter filter) {
            if (TorrentsProxyModel::statusFilterAcceptsTorrent(torrent, filter)) {
                ++(items.find(filter)->second);
            }
        };

        for (const auto& torrent : rpc()->torrents()) {
            processFilter(torrent.get(), Downloading);
            processFilter(torrent.get(), Completed);
            processFilter(torrent.get(), ActiveDownload);
            processFilter(torrent.get(), ActiveUpload);
            processFilter(torrent.get(), Inactive);
            processFilter(torrent.get(), Stopped);
            processFilter(torrent.get(), Errored);
            processFilter(torrent.get(), Waiting);
        }

        StatusFiltersModelUpdater updater(*this);
        updater.update(mItems, std::move(items));
    }
}
