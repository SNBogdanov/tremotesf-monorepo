// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTFILESPROXYMODEL_H
#define TREMOTESF_TORRENTFILESPROXYMODEL_H

#include "ui/itemmodels/baseproxymodel.h"

namespace tremotesf {
    class BaseTorrentFilesModel;
    class TorrentFilesModelEntry;

    class TorrentFilesProxyModel final : public BaseProxyModel {
        Q_OBJECT

    public:
        enum class FilterMode { All, Downloaded, NotDownloaded };

        static constexpr int PriorityHigh = 0x1;
        static constexpr int PriorityNormal = 0x2;
        static constexpr int PriorityLow = 0x4;
        static constexpr int PriorityAll = PriorityHigh | PriorityNormal | PriorityLow;

        explicit TorrentFilesProxyModel(
            BaseTorrentFilesModel* sourceModel, int sortRole, int fallbackColumn, QObject* parent = nullptr
        );

        void setFilterMode(FilterMode mode);
        [[nodiscard]] FilterMode filterMode() const { return mFilterMode; }

        void setPriorityFilter(int priorityMask);
        [[nodiscard]] int priorityFilter() const { return mPriorityFilter; }

        void setShowUnwanted(bool show);
        [[nodiscard]] bool showUnwanted() const { return mShowUnwanted; }

    protected:
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    private:
        [[nodiscard]] bool acceptsEntry(const TorrentFilesModelEntry* entry) const;

        FilterMode mFilterMode = FilterMode::All;
        int mPriorityFilter = PriorityAll;
        bool mShowUnwanted = true;
    };
}

#endif // TREMOTESF_TORRENTFILESPROXYMODEL_H
