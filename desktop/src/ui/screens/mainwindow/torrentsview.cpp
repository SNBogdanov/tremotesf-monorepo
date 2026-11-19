// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentsview.h"

#include <set>
#include <QEvent>
#include <QHeaderView>
#include <QShowEvent>
#include <QSize>
#include <QWindow>

#include "desktoputils.h"
#include "ui/widgets/basetreeview.h"
#include "ui/widgets/progressbardelegate.h"
#include "downloaddirectorydelegate.h"
#include "settings.h"
#include "torrentsmodel.h"
#include "torrentsproxymodel.h"

namespace tremotesf {
    TorrentsView::TorrentsView(TorrentsProxyModel* model, QWidget* parent) : QTreeView(parent) {
        setCommonTreeViewProperties(this, true);
        updateIconSize();
        setContextMenuPolicy(Qt::CustomContextMenu);
        setItemDelegate(new TooltipWhenElidedDelegate(this));
        setItemDelegateForColumn(
            static_cast<int>(TorrentsModel::Column::ProgressBar),
            new ProgressBarDelegate(static_cast<int>(TorrentsModel::Role::Sort), this)
        );
        setItemDelegateForColumn(
            static_cast<int>(TorrentsModel::Column::DownloadDirectory),
            new DownloadDirectoryDelegate(this)
        );
        setModel(model);
        setSelectionMode(QAbstractItemView::ExtendedSelection);

        const auto header = this->header();
        if (!header->restoreState(Settings::instance()->get_torrentsViewHeaderState())) {
            using enum TorrentsModel::Column;
            const std::set defaultColumns{
                Name,
                TotalSize,
                ProgressBar,
                Status,
                Seeders,
                Leechers,
                PeersSendingToUs,
                PeersGettingFromUs,
                DownloadSpeed,
                UploadSpeed,
                Eta,
                Ratio,
                AddedDate,
            };
            for (int i = 0, max = header->count(); i < max; ++i) {
                if (!defaultColumns.contains(static_cast<TorrentsModel::Column>(i))) {
                    header->hideSection(i);
                }
            }
            header->moveSection(
                header->visualIndex(static_cast<int>(AddedDate)),
                header->visualIndex(static_cast<int>(Status)) + 1
            );
            header->moveSection(
                header->visualIndex(static_cast<int>(Eta)),
                header->visualIndex(static_cast<int>(AddedDate)) + 1
            );
            sortByColumn(static_cast<int>(AddedDate), Qt::DescendingOrder);
        }
    }

    bool TorrentsView::event(QEvent* event) {
        switch (event->type()) {
        case QEvent::DevicePixelRatioChange:
        case QEvent::ScreenChangeInternal:
            updateIconSize();
            break;
        default:
            break;
        }
        return QTreeView::event(event);
    }

    void TorrentsView::showEvent(QShowEvent* event) {
        updateIconSize();
        if (!mScreenChangeConnected) {
            if (auto* window = windowHandle()) {
                QObject::connect(window, &QWindow::screenChanged, this, [this] { updateIconSize(); });
                mScreenChangeConnected = true;
            }
        }
        QTreeView::showEvent(event);
    }

    void TorrentsView::updateIconSize() {
        const int size = desktoputils::iconSizeForDevicePixelRatio(devicePixelRatio());
        setIconSize(QSize(size, size));
    }

    void TorrentsView::saveState() { Settings::instance()->set_torrentsViewHeaderState(header()->saveState()); }

}
