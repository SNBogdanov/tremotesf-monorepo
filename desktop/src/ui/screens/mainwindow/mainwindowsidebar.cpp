// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindowsidebar.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWindow>
#include <QShortcut>

#include <algorithm>
#include <vector>

#include "desktoputils.h"
#include "settings.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "rpc/torrent.h"
#include "rpc/tracker.h"
#include "ui/itemmodels/baseproxymodel.h"
#include "ui/stylehelpers.h"
#include "ui/widgets/tooltipwhenelideddelegate.h"
#include "alltrackersmodel.h"
#include "downloaddirectoriesmodel.h"
#include "downloaddirectorydelegate.h"
#include "labelsmodel.h"
#include "statusfiltersmodel.h"
#include "torrentsproxymodel.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        class BaseListView : public QListView {
            Q_OBJECT

        public:
            explicit BaseListView(
                TorrentsProxyModel* torrentsProxyModel,
                BaseTorrentsFiltersSettingsModel* model,
                QAbstractItemDelegate* delegate,
                QWidget* parent
            )
                : QListView(parent), mTorrentsProxyModel(torrentsProxyModel), mModel(model) {
                mModel->setParent(this);
                setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                mBaseFont = font();
                updateSizes();
                QObject::connect(Settings::instance(), &Settings::enlargedIconsChanged, this, &BaseListView::updateSizes);
                if (delegate) {
                    delegate->setParent(this);
                    setItemDelegate(delegate);
                } else {
                    setItemDelegate(new TooltipWhenElidedDelegate(this));
                }
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                setTextElideMode(Qt::ElideMiddle);
                setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                makeScrollAreaTransparent(this);
            }

            [[nodiscard]] QSize minimumSizeHint() const override { return {8, 0}; }

            [[nodiscard]] QSize sizeHint() const override {
                int height = 0;
                for (int i = 0, max = model()->rowCount(); i < max; ++i) {
                    height += sizeHintForRow(i);
                    height += spacing();
                }
                height += spacing();

                return {sizeHintForColumn(0), height};
            }

            void updateSizes() {
                const bool enlarged = Settings::instance()->get_enlargedIcons();
                const int size = desktoputils::iconSizeForDevicePixelRatio(devicePixelRatio(), enlarged);
                setIconSize(QSize(size, size));
                setFont(desktoputils::scaledFont(mBaseFont, devicePixelRatioF(), enlarged));
                updateGeometry();
                if (model()) {
                    doItemsLayout();
                }
            }

            void setFilterEnabled(bool enabled) {
                setVisible(enabled);
                setTorrentsModelFilterEnabled(enabled);
            }

        protected:
            bool event(QEvent* event) override {
                switch (event->type()) {
                case QEvent::DevicePixelRatioChange:
                case QEvent::ScreenChangeInternal:
                    updateSizes();
                    break;
                default:
                    break;
                }
                return QListView::event(event);
            }

            void showEvent(QShowEvent* event) override {
                updateSizes();
                if (!mScreenChangeConnected) {
                    if (auto* window = windowHandle()) {
                        QObject::connect(window, &QWindow::screenChanged, this, [this] { updateSizes(); });
                        mScreenChangeConnected = true;
                    }
                }
                QListView::showEvent(event);
            }

            void init(Rpc* rpc, std::optional<int> sortByRole = {}) {
                mRpc = rpc;
                setContextMenuPolicy(Qt::CustomContextMenu);
                QObject::connect(this, &QWidget::customContextMenuRequested, this, &BaseListView::showContextMenu);
                setSelectionMode(QAbstractItemView::ExtendedSelection);
                QAbstractItemModel* actualModel{};
                if (sortByRole.has_value()) {
                    mProxyModel = new BaseProxyModel(mModel, *sortByRole, std::nullopt, this);
                    mProxyModel->sort(0);
                    actualModel = mProxyModel;
                } else {
                    actualModel = mModel;
                }

                setModel(actualModel);

                QObject::connect(
                    mModel,
                    &BaseTorrentsFiltersSettingsModel::populatedChanged,
                    this,
                    &BaseListView::updateCurrentIndex
                );

                QObject::connect(
                    selectionModel(),
                    &QItemSelectionModel::currentChanged,
                    this,
                    [this](const QModelIndex& current) {
                        if (mModel->isPopulated()) {
                            setTorrentsModelFilterFromIndex(current);
                        }
                    }
                );

                QObject::connect(
                    actualModel,
                    &BaseTorrentsFiltersSettingsModel::rowsInserted,
                    this,
                    &BaseListView::updateGeometry
                );
                QObject::connect(
                    actualModel,
                    &BaseTorrentsFiltersSettingsModel::rowsRemoved,
                    this,
                    &BaseListView::updateGeometry
                );

                mModel->setTorrentsProxyModel(mTorrentsProxyModel);
                mModel->setRpc(rpc);

                if (mModel->isPopulated()) {
                    updateCurrentIndex();
                }
            }

            void updateCurrentIndex() {
                auto index = mModel->indexForTorrentsProxyModelFilter();
                if (index.isValid() && mProxyModel) {
                    index = mProxyModel->mapFromSource(index);
                }
                if (!index.isValid() && model()->rowCount() > 0) {
                    index = model()->index(0, 0);
                }
                if (index.isValid()) {
                    setCurrentIndex(index);
                }
            }

            void showContextMenu(const QPoint& pos) {
                const QModelIndex clickedViewIndex = indexAt(pos);
                if (!clickedViewIndex.isValid()) {
                    return;
                }
                QModelIndexList viewIndexes = selectionModel()->selectedIndexes();
                if (!viewIndexes.contains(clickedViewIndex)) {
                    viewIndexes = {clickedViewIndex};
                }

                std::vector<int> torrentIds{};
                for (const QModelIndex& viewIndex : viewIndexes) {
                    const QModelIndex index = mProxyModel ? mProxyModel->mapToSource(viewIndex) : viewIndex;
                    const auto ids = torrentIdsForFilterIndex(index);
                    torrentIds.insert(torrentIds.end(), ids.begin(), ids.end());
                }
                std::ranges::sort(torrentIds);
                torrentIds.erase(std::ranges::unique(torrentIds).begin(), torrentIds.end());
                if (torrentIds.empty()) {
                    return;
                }

                QMenu menu;
                QAction* const startAction = menu.addAction(
                    QIcon::fromTheme("media-playback-start"_L1),
                    //: Context menu item to start torrents that match the selected filter
                    qApp->translate("tremotesf", "&Start")
                );
                QAction* const startNowAction = menu.addAction(
                    QIcon::fromTheme("media-playback-start-now"_L1, QIcon::fromTheme("media-playback-start"_L1)),
                    //: Context menu item to start torrents that match the selected filter immediately
                    qApp->translate("tremotesf", "Start &Now")
                );
                QAction* const pauseAction = menu.addAction(
                    QIcon::fromTheme("media-playback-pause"_L1),
                    //: Context menu item to pause torrents that match the selected filter
                    qApp->translate("tremotesf", "P&ause")
                );

                const QAction* const selectedAction = menu.exec(mapToGlobal(pos));
                if (selectedAction == startAction) {
                    mRpc->startTorrents(torrentIds);
                } else if (selectedAction == startNowAction) {
                    mRpc->startTorrentsNow(torrentIds);
                } else if (selectedAction == pauseAction) {
                    mRpc->pauseTorrents(torrentIds);
                }
            }

            [[nodiscard]] virtual std::vector<int> torrentIdsForFilterIndex(const QModelIndex& index) const = 0;

            virtual void setTorrentsModelFilterFromIndex(const QModelIndex& index) = 0;
            virtual void setTorrentsModelFilterEnabled(bool enabled) = 0;

            Rpc* mRpc{};
            TorrentsProxyModel* mTorrentsProxyModel;
            BaseTorrentsFiltersSettingsModel* mModel;
            BaseProxyModel* mProxyModel{};
            bool mScreenChangeConnected = false;
            QFont mBaseFont{};
        };

        class StatusFiltersListView final : public BaseListView {
            Q_OBJECT

        public:
            StatusFiltersListView(Rpc* rpc, TorrentsProxyModel* torrentsModel, QWidget* parent = nullptr)
                : BaseListView(torrentsModel, new StatusFiltersModel(), nullptr, parent) {
                init(rpc);
                QObject::connect(torrentsModel, &TorrentsProxyModel::statusFilterChanged, this, [=, this] {
                    updateCurrentIndex();
                });
            }

        protected:
            void setTorrentsModelFilterFromIndex(const QModelIndex& index) override {
                mTorrentsProxyModel->setStatusFilter(
                    index.data(StatusFiltersModel::FilterRole).value<TorrentsProxyModel::StatusFilter>()
                );
            }

            void setTorrentsModelFilterEnabled(bool enabled) override {
                mTorrentsProxyModel->setStatusFilterEnabled(enabled);
            }

            [[nodiscard]] std::vector<int> torrentIdsForFilterIndex(const QModelIndex& index) const override {
                const auto filter =
                    index.data(StatusFiltersModel::FilterRole).value<TorrentsProxyModel::StatusFilter>();
                std::vector<int> ids{};
                for (const auto& torrent : mRpc->torrents()) {
                    if (TorrentsProxyModel::statusFilterAcceptsTorrent(torrent.get(), filter)) {
                        ids.push_back(torrent->data().id);
                    }
                }
                return ids;
            }
        };

        class LabelsListView final : public BaseListView {
            Q_OBJECT

        public:
            LabelsListView(Rpc* rpc, TorrentsProxyModel* torrentsModel, QWidget* parent = nullptr)
                : BaseListView(torrentsModel, new LabelsModel(), nullptr, parent) {
                init(rpc, LabelsModel::LabelRole);
                QObject::connect(torrentsModel, &TorrentsProxyModel::labelFilterChanged, this, [=, this] {
                    updateCurrentIndex();
                });
            }

        protected:
            void setTorrentsModelFilterFromIndex(const QModelIndex& index) override {
                mTorrentsProxyModel->setLabelFilter(index.data(LabelsModel::LabelRole).toString());
            }

            void setTorrentsModelFilterEnabled(bool enabled) override {
                mTorrentsProxyModel->setLabelFilterEnabled(enabled);
            }

            [[nodiscard]] std::vector<int> torrentIdsForFilterIndex(const QModelIndex& index) const override {
                const QString label = index.data(LabelsModel::LabelRole).toString();
                std::vector<int> ids{};
                for (const auto& torrent : mRpc->torrents()) {
                    if (std::ranges::contains(torrent->data().labels, label)) {
                        ids.push_back(torrent->data().id);
                    }
                }
                return ids;
            }
        };

        class TrackersListView final : public BaseListView {
            Q_OBJECT

        public:
            TrackersListView(Rpc* rpc, TorrentsProxyModel* torrentsModel, QWidget* parent = nullptr)
                : BaseListView(torrentsModel, new AllTrackersModel(), nullptr, parent) {
                init(rpc, AllTrackersModel::TrackerRole);
                QObject::connect(torrentsModel, &TorrentsProxyModel::trackerFilterChanged, this, [=, this] {
                    updateCurrentIndex();
                });
            }

        protected:
            void setTorrentsModelFilterFromIndex(const QModelIndex& index) override {
                mTorrentsProxyModel->setTrackerFilter(index.data(AllTrackersModel::TrackerRole).toString());
            }

            void setTorrentsModelFilterEnabled(bool enabled) override {
                mTorrentsProxyModel->setTrackerFilterEnabled(enabled);
            }

            [[nodiscard]] std::vector<int> torrentIdsForFilterIndex(const QModelIndex& index) const override {
                const QString site = index.data(AllTrackersModel::TrackerRole).toString();
                std::vector<int> ids{};
                for (const auto& torrent : mRpc->torrents()) {
                    if (std::ranges::contains(torrent->data().trackers, site, &Tracker::site)) {
                        ids.push_back(torrent->data().id);
                    }
                }
                return ids;
            }
        };

        class DirectoriesListView final : public BaseListView {
            Q_OBJECT

        public:
            DirectoriesListView(Rpc* rpc, TorrentsProxyModel* torrentsModel, QWidget* parent = nullptr)
                : BaseListView(torrentsModel, new DownloadDirectoriesModel(), new DownloadDirectoryDelegate(), parent) {
                init(rpc, static_cast<int>(DownloadDirectoriesModel::Role::Directory));
                QObject::connect(torrentsModel, &TorrentsProxyModel::downloadDirectoryFilterChanged, this, [=, this] {
                    updateCurrentIndex();
                });
            }

        protected:
            void setTorrentsModelFilterFromIndex(const QModelIndex& index) override {
                mTorrentsProxyModel->setDownloadDirectoryFilter(
                    index.data(static_cast<int>(DownloadDirectoriesModel::Role::Directory)).toString()
                );
            }

            void setTorrentsModelFilterEnabled(bool enabled) override {
                mTorrentsProxyModel->setDownloadDirectoryFilterEnabled(enabled);
            }

            [[nodiscard]] std::vector<int> torrentIdsForFilterIndex(const QModelIndex& index) const override {
                const QString directory =
                    index.data(static_cast<int>(DownloadDirectoriesModel::Role::Directory)).toString();
                std::vector<int> ids{};
                for (const auto& torrent : mRpc->torrents()) {
                    if (torrent->data().downloadDirectory == directory) {
                        ids.push_back(torrent->data().id);
                    }
                }
                return ids;
            }
        };
    }

    MainWindowSideBar::MainWindowSideBar(Rpc* rpc, TorrentsProxyModel* proxyModel, QWidget* parent)
        : QScrollArea(parent) {
        mBaseFont = font();
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setWidgetResizable(true);

        auto contentWidget = new QWidget(this);
        auto layout = new QVBoxLayout(contentWidget);

        auto searchLineEdit = new QLineEdit(this);
        searchLineEdit->setClearButtonEnabled(true);
        //: Search field placeholder
        searchLineEdit->setPlaceholderText(qApp->translate("tremotesf", "Search..."));
        QObject::connect(searchLineEdit, &QLineEdit::textChanged, this, [proxyModel](const auto& text) {
            proxyModel->setSearchString(text);
        });
        layout->addWidget(searchLineEdit);

        auto* const searchShortcut = new QShortcut(QKeyCombination(Qt::ControlModifier, Qt::Key_F), this);
        QObject::connect(searchShortcut, &QShortcut::activated, this, [searchLineEdit] {
            searchLineEdit->selectAll();
            searchLineEdit->setFocus();
        });

        //: Title of torrents status filters list
        auto statusCheckBox = new QCheckBox(qApp->translate("tremotesf", "Status"), this);
        statusCheckBox->setChecked(true);
        layout->addWidget(statusCheckBox);

        auto statusFiltersListView = new StatusFiltersListView(rpc, proxyModel, this);
        QObject::connect(
            statusCheckBox,
            &QCheckBox::toggled,
            statusFiltersListView,
            &StatusFiltersListView::setFilterEnabled
        );
        layout->addWidget(statusFiltersListView);
        statusCheckBox->setChecked(proxyModel->isStatusFilterEnabled());

        //: Title of torrents label filters list
        auto labelsCheckBox = new QCheckBox(qApp->translate("tremotesf", "Labels"), this);
        labelsCheckBox->setChecked(proxyModel->isLabelFilterEnabled());
        layout->addWidget(labelsCheckBox);

        auto labelsListView = new LabelsListView(rpc, proxyModel, this);
        QObject::connect(labelsCheckBox, &QCheckBox::toggled, labelsListView, &LabelsListView::setFilterEnabled);
        layout->addWidget(labelsListView);

        QObject::connect(rpc, &Rpc::connectedChanged, this, [=] {
            const bool serverSupportsLabels =
                rpc->isConnected() ? rpc->serverSettings()->data().hasLabelsProperty() : true;
            if (serverSupportsLabels) {
                labelsCheckBox->setVisible(true);
                labelsListView->setVisible(labelsCheckBox->isChecked());
            } else {
                labelsCheckBox->setVisible(false);
                labelsListView->setVisible(false);
            }
        });

        //: Title of torrents download directory filters list
        auto directoriesCheckBox = new QCheckBox(qApp->translate("tremotesf", "Directories"), this);
        directoriesCheckBox->setChecked(true);
        layout->addWidget(directoriesCheckBox);

        auto directoriesListView = new DirectoriesListView(rpc, proxyModel, this);
        QObject::connect(
            directoriesCheckBox,
            &QCheckBox::toggled,
            directoriesListView,
            &DirectoriesListView::setFilterEnabled
        );
        layout->addWidget(directoriesListView);
        directoriesCheckBox->setChecked(proxyModel->isDownloadDirectoryFilterEnabled());

        //: Title of torrents tracker filters list
        auto trackersCheckBox = new QCheckBox(qApp->translate("tremotesf", "Trackers"), this);
        trackersCheckBox->setChecked(true);
        layout->addWidget(trackersCheckBox);

        auto trackersListView = new TrackersListView(rpc, proxyModel, this);
        QObject::connect(trackersCheckBox, &QCheckBox::toggled, trackersListView, &TrackersListView::setFilterEnabled);
        layout->addWidget(trackersListView);
        trackersCheckBox->setChecked(proxyModel->isTrackerFilterEnabled());

        layout->addStretch();

        setWidget(contentWidget);

        updateFonts();
        QObject::connect(Settings::instance(), &Settings::enlargedIconsChanged, this, &MainWindowSideBar::updateFonts);
    }

    bool MainWindowSideBar::event(QEvent* event) {
        switch (event->type()) {
        case QEvent::DevicePixelRatioChange:
        case QEvent::ScreenChangeInternal:
            updateFonts();
            break;
        default:
            break;
        }
        return QScrollArea::event(event);
    }

    void MainWindowSideBar::updateFonts() {
        const bool enlarged = Settings::instance()->get_enlargedIcons();
        setFont(desktoputils::scaledFont(mBaseFont, devicePixelRatioF(), enlarged));
        QFont checkBoxFont = desktoputils::scaledFont(mBaseFont, devicePixelRatioF(), enlarged);
        checkBoxFont.setBold(true);
        checkBoxFont.setCapitalization(QFont::AllUppercase);
        for (auto* checkBox : findChildren<QCheckBox*>()) {
            checkBox->setFont(checkBoxFont);
        }
    }
}

#include "mainwindowsidebar.moc"
