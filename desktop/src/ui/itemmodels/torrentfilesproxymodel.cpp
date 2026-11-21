// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfilesproxymodel.h"

#include <ranges>

#include "ui/itemmodels/basetorrentfilesmodel.h"

namespace tremotesf {
    TorrentFilesProxyModel::TorrentFilesProxyModel(
        BaseTorrentFilesModel* sourceModel, int sortRole, int fallbackColumn, QObject* parent
    )
        : BaseProxyModel(sourceModel, sortRole, fallbackColumn, parent) {}

    void TorrentFilesProxyModel::setFilterMode(FilterMode mode) {
        if (mode != mFilterMode) {
            mFilterMode = mode;
            invalidateRowsFilter();
        }
    }

    void TorrentFilesProxyModel::setShowUnwanted(bool show) {
        if (show != mShowUnwanted) {
            mShowUnwanted = show;
            invalidateRowsFilter();
        }
    }

    void TorrentFilesProxyModel::setPriorityFilter(int priorityMask) {
        if (priorityMask != mPriorityFilter) {
            mPriorityFilter = priorityMask;
            invalidateRowsFilter();
        }
    }

    bool TorrentFilesProxyModel::acceptsEntry(const TorrentFilesModelEntry* entry) const {
        if (!mShowUnwanted && entry->wantedState() == TorrentFilesModelEntry::WantedState::Unwanted) {
            return false;
        }
        if (entry->isDirectory()) {
            return std::ranges::any_of(entry->children(), [this](const auto& child) { return acceptsEntry(&child); });
        }
        switch (mFilterMode) {
        case FilterMode::All:
            break;
        case FilterMode::Downloaded:
            if (entry->size() != 0 && entry->progress() < 1.0) {
                return false;
            }
            break;
        case FilterMode::NotDownloaded:
            if (entry->size() == 0 || entry->progress() >= 1.0) {
                return false;
            }
            break;
        }
        int priorityFlag = 0;
        switch (entry->priority()) {
        case TorrentFilesModelEntry::Priority::High:
            priorityFlag = PriorityHigh;
            break;
        case TorrentFilesModelEntry::Priority::Normal:
            priorityFlag = PriorityNormal;
            break;
        case TorrentFilesModelEntry::Priority::Low:
            priorityFlag = PriorityLow;
            break;
        case TorrentFilesModelEntry::Priority::Mixed:
            return false;
        }
        return (mPriorityFilter & priorityFlag) != 0;
    }

    bool TorrentFilesProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
        const auto* const source = static_cast<const BaseTorrentFilesModel*>(sourceModel());
        if (!source) {
            return true;
        }
        const auto index = source->index(sourceRow, 0, sourceParent);
        const auto* const entry = static_cast<const TorrentFilesModelEntry*>(index.internalPointer());
        return !entry || acceptsEntry(entry);
    }

    bool TorrentFilesProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
        const bool leftIsDirectory = static_cast<TorrentFilesModelEntry*>(left.internalPointer())->isDirectory();
        const bool rightIsDirectory = static_cast<TorrentFilesModelEntry*>(right.internalPointer())->isDirectory();

        if (leftIsDirectory != rightIsDirectory) {
            if (sortOrder() == Qt::AscendingOrder) {
                return leftIsDirectory;
            }
            return rightIsDirectory;
        }

        return BaseProxyModel::lessThan(left, right);
    }
}
