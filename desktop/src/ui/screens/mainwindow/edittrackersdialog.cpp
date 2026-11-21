// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "edittrackersdialog.h"

#include <array>
#include <set>

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "rpc/rpc.h"
#include "rpc/torrent.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    EditTrackersDialog::EditTrackersDialog(Rpc* rpc, std::vector<Torrent*> torrents, QWidget* parent)
        : QDialog(parent), mRpc(rpc), mTorrents(std::move(torrents)) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Edit Trackers"));
        resize(600, 400);

        auto* const layout = new QVBoxLayout(this);

        mList = new QListWidget(this);
        mList->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(mList);

        auto* const buttonsLayout = new QHBoxLayout();
        mAddButton = new QPushButton(QIcon::fromTheme("list-add"_L1), QString(), this);
        mAddButton->setToolTip(qApp->translate("tremotesf", "Add tracker to all selected torrents"));
        buttonsLayout->addWidget(mAddButton);
        mEditButton = new QPushButton(QIcon::fromTheme("edit-rename"_L1), QString(), this);
        mEditButton->setToolTip(qApp->translate("tremotesf", "Replace tracker in all selected torrents"));
        buttonsLayout->addWidget(mEditButton);
        mRemoveButton = new QPushButton(QIcon::fromTheme("list-remove"_L1), QString(), this);
        mRemoveButton->setToolTip(qApp->translate("tremotesf", "Remove tracker from all selected torrents"));
        buttonsLayout->addWidget(mRemoveButton);
        buttonsLayout->addStretch();
        layout->addLayout(buttonsLayout);

        auto* const buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
        layout->addWidget(buttonBox);

        QObject::connect(mAddButton, &QPushButton::clicked, this, &EditTrackersDialog::addTracker);
        QObject::connect(mEditButton, &QPushButton::clicked, this, &EditTrackersDialog::editTracker);
        QObject::connect(mRemoveButton, &QPushButton::clicked, this, &EditTrackersDialog::removeTracker);
        QObject::connect(mList, &QListWidget::itemSelectionChanged, this, &EditTrackersDialog::updateButtons);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &EditTrackersDialog::reject);

        rebuildList();
        updateButtons();
    }

    void EditTrackersDialog::rebuildList() {
        QStringList announceUrls;
        for (const auto* const torrent : mTorrents) {
            for (const auto& tracker : torrent->data().trackers) {
                if (!announceUrls.contains(tracker.announce())) {
                    announceUrls.push_back(tracker.announce());
                }
            }
        }
        announceUrls.sort(Qt::CaseInsensitive);
        mList->clear();
        mList->addItems(announceUrls);
    }

    void EditTrackersDialog::updateButtons() {
        const bool hasSelection = mList->currentItem() != nullptr;
        mEditButton->setEnabled(hasSelection);
        mRemoveButton->setEnabled(hasSelection);
    }

    void EditTrackersDialog::addTracker() {
        bool ok = false;
        const auto announceUrl = QInputDialog::getText(
                                     this,
                                     qApp->translate("tremotesf", "Add Tracker"),
                                     qApp->translate("tremotesf", "Tracker announce URL:"),
                                     QLineEdit::Normal,
                                     {},
                                     &ok
        )
                                     .trimmed();
        if (!ok || announceUrl.isEmpty()) {
            return;
        }
        const std::array<std::set<QString>, 1> trackers{{std::set<QString>{announceUrl}}};
        for (auto* const torrent : mTorrents) {
            torrent->addTrackers(trackers);
        }
        reannounceAll();
        if (mList->findItems(announceUrl, Qt::MatchExactly).isEmpty()) {
            mList->addItem(announceUrl);
            mList->sortItems(Qt::AscendingOrder);
        }
    }

    void EditTrackersDialog::editTracker() {
        auto* const item = mList->currentItem();
        if (!item) {
            return;
        }
        const auto oldAnnounceUrl = item->text();
        bool ok = false;
        const auto newAnnounceUrl = QInputDialog::getText(
                                        this,
                                        qApp->translate("tremotesf", "Edit Tracker"),
                                        qApp->translate("tremotesf", "Tracker announce URL:"),
                                        QLineEdit::Normal,
                                        oldAnnounceUrl,
                                        &ok
        )
                                        .trimmed();
        if (!ok || newAnnounceUrl.isEmpty() || newAnnounceUrl == oldAnnounceUrl) {
            return;
        }
        for (auto* const torrent : mTorrents) {
            std::vector<int> trackerIds;
            for (const auto& tracker : torrent->data().trackers) {
                if (tracker.announce() == oldAnnounceUrl) {
                    trackerIds.push_back(tracker.id());
                }
            }
            for (const int trackerId : trackerIds) {
                torrent->setTracker(trackerId, newAnnounceUrl);
            }
        }
        reannounceAll();
        item->setText(newAnnounceUrl);
        mList->sortItems(Qt::AscendingOrder);
    }

    void EditTrackersDialog::removeTracker() {
        auto* const item = mList->currentItem();
        if (!item) {
            return;
        }
        const auto announceUrl = item->text();
        for (auto* const torrent : mTorrents) {
            std::vector<int> trackerIds;
            for (const auto& tracker : torrent->data().trackers) {
                if (tracker.announce() == announceUrl) {
                    trackerIds.push_back(tracker.id());
                }
            }
            if (!trackerIds.empty()) {
                torrent->removeTrackers(trackerIds);
            }
        }
        reannounceAll();
        delete item;
        updateButtons();
    }

    void EditTrackersDialog::reannounceAll() {
        if (mTorrents.empty()) {
            return;
        }
        std::vector<int> ids;
        ids.reserve(mTorrents.size());
        for (const auto* const torrent : mTorrents) {
            ids.push_back(torrent->data().id);
        }
        mRpc->reannounceTorrents(ids);
    }
}
