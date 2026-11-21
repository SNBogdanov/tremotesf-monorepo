// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentsview.h"

#include <algorithm>
#include <set>
#include <QColor>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QPalette>
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
        setEditTriggers(QAbstractItemView::EditKeyPressed);
        setAlternatingRowColors(true);
        updateAlternateRowColor();

        QObject::connect(model, &QAbstractItemModel::rowsInserted, this, [this] { ensureSelection(); });
        QObject::connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { ensureSelection(); });
        QObject::connect(model, &QAbstractItemModel::modelReset, this, [this] { ensureSelection(); });
        QObject::connect(model, &QAbstractItemModel::layoutChanged, this, [this] { ensureSelection(); });
        QObject::connect(
            model,
            &QAbstractItemModel::rowsAboutToBeRemoved,
            this,
            [this](const QModelIndex& parent, int first, int last) {
                if (parent.isValid()) {
                    return;
                }
                const auto current = currentIndex();
                if (current.isValid() && current.row() >= first && current.row() <= last) {
                    mRowToSelectAfterRemoval = first;
                }
            }
        );
        ensureSelection();

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
                Id,
                WantedSize,
            };
            for (int i = 0, max = header->count(); i < max; ++i) {
                if (!defaultColumns.contains(static_cast<TorrentsModel::Column>(i))) {
                    header->hideSection(i);
                }
            }
            header->moveSection(header->visualIndex(static_cast<int>(Id)), 1);
            header->moveSection(
                header->visualIndex(static_cast<int>(WantedSize)),
                header->visualIndex(static_cast<int>(TotalSize)) + 1
            );
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
        header->showSection(static_cast<int>(TorrentsModel::Column::WantedSize));
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

    void TorrentsView::changeEvent(QEvent* event) {
        QTreeView::changeEvent(event);
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
            updateAlternateRowColor();
        }
    }

    void TorrentsView::updateAlternateRowColor() {
        QPalette palette = QTreeView::palette();
        const QColor base = palette.color(QPalette::Base);
        const QColor alternate = base.lightnessF() > 0.5 ? base.darker(103) : base.lighter(105);
        if (palette.color(QPalette::AlternateBase) == alternate) {
            return;
        }
        palette.setColor(QPalette::AlternateBase, alternate);
        setPalette(palette);
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

    void TorrentsView::keyPressEvent(QKeyEvent* event) {
        if (event->key() == Qt::Key_F2) {
            const auto current = currentIndex();
            if (current.isValid()) {
                const auto nameIndex = current.siblingAtColumn(static_cast<int>(TorrentsModel::Column::Name));
                if (nameIndex.isValid()) {
                    edit(nameIndex);
                    event->accept();
                    return;
                }
            }
        }
        QTreeView::keyPressEvent(event);
    }

    void TorrentsView::ensureSelection() {
        auto* const selection = selectionModel();
        if (!model() || !selection || model()->rowCount() == 0) {
            return;
        }
        if (selection->hasSelection()) {
            mRowToSelectAfterRemoval = -1;
            return;
        }
        const int row = mRowToSelectAfterRemoval >= 0
                            ? std::min(mRowToSelectAfterRemoval, model()->rowCount() - 1)
                            : 0;
        mRowToSelectAfterRemoval = -1;
        const auto index = model()->index(row, 0);
        if (!index.isValid()) {
            return;
        }
        selection->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    }

    void TorrentsView::updateIconSize() {
        const int size = desktoputils::iconSizeForDevicePixelRatio(devicePixelRatio());
        setIconSize(QSize(size, size));
    }

    void TorrentsView::saveState() { Settings::instance()->set_torrentsViewHeaderState(header()->saveState()); }

}
