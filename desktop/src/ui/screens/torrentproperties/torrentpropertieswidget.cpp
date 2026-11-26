// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentpropertieswidget.h"

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <KMessageWidget>
#include <KColumnResizer>

#include "desktoputils.h"
#include "formatutils.h"
#include "peersmodel.h"
#include "settings.h"
#include "stdutils.h"
#include "torrentfilesmodel.h"
#include "trackersviewwidget.h"
#include "rpc/numbervaluerange.h"
#include "rpc/pathutils.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "rpc/torrent.h"
#include "ui/itemmodels/baseproxymodel.h"
#include "ui/itemmodels/stringlistmodel.h"
#include "ui/itemmodels/torrentfilesproxymodel.h"
#include "ui/stylehelpers.h"
#include "ui/widgets/basetreeview.h"
#include "ui/widgets/progressbardelegate.h"
#include "ui/widgets/tooltipwhenelideddelegate.h"
#include "ui/widgets/torrentfilesview.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        constexpr std::array priorityComboBoxItems{
            TorrentData::Priority::High, TorrentData::Priority::Normal, TorrentData::Priority::Low
        };

        constexpr std::array ratioLimitComboBoxItems{
            TorrentData::RatioLimitMode::Global,
            TorrentData::RatioLimitMode::Unlimited,
            TorrentData::RatioLimitMode::Single
        };

        constexpr std::array idleSeedingLimitComboBoxItems{
            TorrentData::IdleSeedingLimitMode::Global,
            TorrentData::IdleSeedingLimitMode::Unlimited,
            TorrentData::IdleSeedingLimitMode::Single
        };
    }

    TorrentPropertiesWidget::TorrentPropertiesWidget(Rpc* rpc, bool horizontalDetails, QWidget* parent)
        : QTabWidget(parent),
          mRpc(rpc),
          mFilesModel(new TorrentFilesModel(mRpc, this)),
          mFilesView(new TorrentFilesView(mFilesModel, mRpc, this)),
          mTrackersViewWidget(new TrackersViewWidget(mRpc, this)) {
        mBaseFont = font();
        setEnabled(false);

        setupDetailsTab(horizontalDetails);

        auto filesTab = new QWidget(this);
        auto filesTabLayout = new QVBoxLayout(filesTab);

        auto filesFilterLayout = new QHBoxLayout();
        filesFilterLayout->addWidget(new QLabel(qApp->translate("tremotesf", "Show:"), this));
        auto filesFilterComboBox = new QComboBox(this);
        using FilterMode = TorrentFilesProxyModel::FilterMode;
        filesFilterComboBox->addItem(qApp->translate("tremotesf", "All files"), static_cast<int>(FilterMode::All));
        filesFilterComboBox->addItem(
            qApp->translate("tremotesf", "Downloaded files"),
            static_cast<int>(FilterMode::Downloaded)
        );
        filesFilterComboBox->addItem(
            qApp->translate("tremotesf", "Not downloaded files"),
            static_cast<int>(FilterMode::NotDownloaded)
        );
        filesFilterLayout->addWidget(filesFilterComboBox);

        filesFilterLayout->addWidget(new QLabel(qApp->translate("tremotesf", "Priority:"), this));
        auto highPriorityCheckBox = new QCheckBox(qApp->translate("tremotesf", "High"), this);
        auto normalPriorityCheckBox = new QCheckBox(qApp->translate("tremotesf", "Normal"), this);
        auto lowPriorityCheckBox = new QCheckBox(qApp->translate("tremotesf", "Low"), this);
        for (auto* checkBox : {highPriorityCheckBox, normalPriorityCheckBox, lowPriorityCheckBox}) {
            checkBox->setChecked(true);
            filesFilterLayout->addWidget(checkBox);
        }
        const auto updatePriorityFilter = [=, this] {
            int mask = 0;
            if (highPriorityCheckBox->isChecked()) {
                mask |= TorrentFilesProxyModel::PriorityHigh;
            }
            if (normalPriorityCheckBox->isChecked()) {
                mask |= TorrentFilesProxyModel::PriorityNormal;
            }
            if (lowPriorityCheckBox->isChecked()) {
                mask |= TorrentFilesProxyModel::PriorityLow;
            }
            mFilesView->proxyModel()->setPriorityFilter(mask);
        };
        for (auto* checkBox : {highPriorityCheckBox, normalPriorityCheckBox, lowPriorityCheckBox}) {
            QObject::connect(checkBox, &QCheckBox::toggled, this, updatePriorityFilter);
        }

        auto showUnwantedFilesCheckBox = new QCheckBox(qApp->translate("tremotesf", "Show unwanted files"), this);
        showUnwantedFilesCheckBox->setChecked(true);
        filesFilterLayout->addWidget(showUnwantedFilesCheckBox);
        filesFilterLayout->addStretch();
        filesTabLayout->addLayout(filesFilterLayout);

        QObject::connect(filesFilterComboBox, &QComboBox::currentIndexChanged, this, [=, this](int index) {
            mFilesView->proxyModel()->setFilterMode(
                static_cast<FilterMode>(filesFilterComboBox->itemData(index).toInt())
            );
        });
        QObject::connect(
            showUnwantedFilesCheckBox,
            &QCheckBox::toggled,
            mFilesView->proxyModel(),
            &TorrentFilesProxyModel::setShowUnwanted
        );

        filesTabLayout->addWidget(mFilesView);
        overrideBreezeFramelessScrollAreaHeuristic(mFilesView, true);
        //: Torrent properties dialog tab
        addTab(filesTab, qApp->translate("tremotesf", "Files"));

        //: Torrent properties dialog tab
        addTab(mTrackersViewWidget, qApp->translate("tremotesf", "Trackers"));

        setupPeersTab();
        setupWebSeedersTab();
        setupLimitsTab();

        updateFontSize();
        QObject::connect(
            Settings::instance(),
            &Settings::enlargedIconsChanged,
            this,
            &TorrentPropertiesWidget::updateFontSize
        );
    }

    bool TorrentPropertiesWidget::event(QEvent* event) {
        switch (event->type()) {
        case QEvent::DevicePixelRatioChange:
        case QEvent::ScreenChangeInternal:
            updateFontSize();
            break;
        default:
            break;
        }
        return QTabWidget::event(event);
    }

    void TorrentPropertiesWidget::updateFontSize() {
        setFont(desktoputils::scaledFont(mBaseFont, devicePixelRatioF(), Settings::instance()->get_enlargedIcons()));
    }

    void TorrentPropertiesWidget::saveState() {
        Settings::instance()->set_peersViewHeaderState(mPeersView->header()->saveState());
        mFilesView->saveState();
        mTrackersViewWidget->saveState();
    }

    void TorrentPropertiesWidget::setupDetailsTab(bool horizontal) {
        auto detailsTab = new QScrollArea(this);
        detailsTab->setWidgetResizable(true);
        makeScrollAreaTransparent(detailsTab);

        if (!horizontal) {
            detailsTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
        //: Torrent's properties dialog tab
        addTab(detailsTab, qApp->translate("tremotesf", "Details"));

        auto detailsTabScrollContent = new QWidget(detailsTab);
        detailsTab->setWidget(detailsTabScrollContent);

        QBoxLayout* detailsTabLayout{};
        if (horizontal) {
            detailsTabLayout = new QHBoxLayout(detailsTabScrollContent);
        } else {
            detailsTabLayout = new QVBoxLayout(detailsTabScrollContent);
        }

        //: Torrent's details tab section
        auto activityGroupBox = new QGroupBox(qApp->translate("tremotesf", "Activity"), this);
        auto activityGroupBoxLayout = new QFormLayout(activityGroupBox);
        activityGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto completedLabel = new QLabel(this);
        //: Torrent's completed size
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Completed:"), completedLabel);
        auto downloadedLabel = new QLabel(this);
        //: Torrent's downloaded size
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Downloaded:"), downloadedLabel);
        auto uploadedLabel = new QLabel(this);
        //: Torrent's uploaded size
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Uploaded:"), uploadedLabel);
        auto ratioLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Ratio:"), ratioLabel);
        auto downloadSpeedLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Download speed:"), downloadSpeedLabel);
        auto uploadSpeedLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Upload speed:"), uploadSpeedLabel);
        auto etaLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "ETA:"), etaLabel);
        auto seedersLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Seeders:"), seedersLabel);
        auto leechersLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Leechers:"), leechersLabel);
        auto peersSendingToUsLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Peers we are downloading from:"),
            peersSendingToUsLabel
        );
        auto webSeedersSendingToUsLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Web seeders we are downloading from:"),
            webSeedersSendingToUsLabel
        );
        auto peersGettingFromUsLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Peers we are uploading to:"),
            peersGettingFromUsLabel
        );
        auto lastActivityLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Last activity:"), lastActivityLabel);
        auto errorLabel = new QLabel(this);
        errorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        errorLabel->setWordWrap(true);
        //: Torrent's error
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Error:"), errorLabel);
        auto errorLabelLabel = activityGroupBoxLayout->labelForField(errorLabel);
        detailsTabLayout->addWidget(activityGroupBox);

        //: Torrent's details tab section
        auto infoGroupBox = new QGroupBox(qApp->translate("tremotesf", "Information"), this);
        auto infoGroupBoxLayout = new QFormLayout(infoGroupBox);
        infoGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto totalSizeLabel = new QLabel(this);
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Total size:"), totalSizeLabel);
        auto wantedSizeLabel = new QLabel(this);
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Selected size:"), wantedSizeLabel);
        auto completedSizeLabel = new QLabel(this);
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Completed files:"), completedSizeLabel);
        auto locationLabel = new QLabel(this);
        locationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        //: Torrent's download directory
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Location:"), locationLabel);
        auto incompleteDirectoryLabel = new QLabel(this);
        incompleteDirectoryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        //: Torrent's temporary directory for incomplete data
        infoGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Incomplete directory:"),
            incompleteDirectoryLabel
        );
        auto incompleteDirectoryLabelLabel = infoGroupBoxLayout->labelForField(incompleteDirectoryLabel);
        auto hashLabel = new QLabel(this);
        hashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        //: Torrent's hash string
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Hash:"), hashLabel);
        auto creatorLabel = new QLabel(this);
        //: Program that created torrent file
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Created by:"), creatorLabel);
        auto creationDateLabel = new QLabel(this);
        //: Date/time when torrent was created
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Created on:"), creationDateLabel);
        auto commentTextEdit = new QTextBrowser(this);
        commentTextEdit->setOpenExternalLinks(true);
        commentTextEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        // Show a few lines by default, longer comments are scrollable
        commentTextEdit->setFixedHeight(commentTextEdit->fontMetrics().lineSpacing() * 4 + 8);
        //: Torrent's comment text
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Comment:"), commentTextEdit);

        auto topicLabel = new QLabel(this);
        topicLabel->setOpenExternalLinks(true);
        topicLabel->setTextFormat(Qt::RichText);
        topicLabel->setWordWrap(true);
        topicLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        // Allow the field to shrink below its content size so that long topic names wrap instead of expanding the form
        topicLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        //: Torrent's topic name (metadata provided by server's fork)
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Topic:"), topicLabel);
        auto topicLabelLabel = infoGroupBoxLayout->labelForField(topicLabel);

        auto labelsModel = new StringListModel({}, QIcon::fromTheme("tag"_L1), this);
        auto labelsProxyModel = new BaseProxyModel(labelsModel, Qt::DisplayRole, std::nullopt, this);
        auto labelsView = new QListView(this);
        labelsView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        labelsView->setFlow(QListView::LeftToRight);
        labelsView->setWrapping(true);
        labelsView->setResizeMode(QListView::Adjust);
        labelsView->setIconSize(QSize(16, 16));
        labelsView->setModel(labelsProxyModel);
        // Show at most two rows of labels, more labels are scrollable
        const int labelsRowHeight =
            std::max(labelsView->fontMetrics().lineSpacing(), labelsView->iconSize().height()) + 4;
        labelsView->setFixedHeight(labelsRowHeight * 2 + labelsView->frameWidth() * 2 + labelsView->spacing());
        //: Torrent's labels
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Labels:"), labelsView);
        auto labelsLabel = infoGroupBoxLayout->labelForField(labelsView);

        detailsTabLayout->addWidget(infoGroupBox);

        if (horizontal) {
            // Give extra width to the information group; activity group keeps its natural width
            detailsTabLayout->setStretch(0, 0);
            detailsTabLayout->setStretch(1, 1);
        }

        if (!horizontal) {
            auto resizer = new KColumnResizer(this);
            resizer->addWidgetsFromLayout(activityGroupBoxLayout);
            resizer->addWidgetsFromLayout(infoGroupBoxLayout);
        }

        mUpdateDetailsTab = [=, this] {
            if (mTorrent) {
                //: Torrent's completion size, e.g. 100 MiB of 200 MiB (50%). %1 is completed size, %2 is size, %3 is progress in percents
                completedLabel->setText(qApp->translate("tremotesf", "%1 of %2 (%3)")
                                            .arg(
                                                formatutils::formatByteSize(mTorrent->data().completedSize),
                                                formatutils::formatByteSize(mTorrent->data().sizeWhenDone),
                                                formatutils::formatProgress(mTorrent->data().percentDone)
                                            ));
                downloadedLabel->setText(formatutils::formatByteSize(mTorrent->data().totalDownloaded));
                uploadedLabel->setText(formatutils::formatByteSize(mTorrent->data().totalUploaded));
                ratioLabel->setText(formatutils::formatRatio(mTorrent->data().ratio));
                downloadSpeedLabel->setText(formatutils::formatByteSpeed(mTorrent->data().downloadSpeed));
                uploadSpeedLabel->setText(formatutils::formatByteSpeed(mTorrent->data().uploadSpeed));
                etaLabel->setText(formatutils::formatEta(mTorrent->data().eta));

                const QLocale locale{};
                seedersLabel->setText(locale.toString(mTorrent->data().totalSeedersFromTrackersCount));
                leechersLabel->setText(locale.toString(mTorrent->data().totalLeechersFromTrackersCount));
                peersSendingToUsLabel->setText(locale.toString(mTorrent->data().peersSendingToUsCount));
                webSeedersSendingToUsLabel->setText(locale.toString(mTorrent->data().webSeedersSendingToUsCount));
                peersGettingFromUsLabel->setText(locale.toString(mTorrent->data().peersGettingFromUsCount));

                lastActivityLabel->setText(
                    formatutils::formatDateTime(mTorrent->data().activityDate.toLocalTime(), QLocale::LongFormat)
                );
                errorLabel->setText(mTorrent->data().errorString);

                totalSizeLabel->setText(formatutils::formatByteSize(mTorrent->data().totalSize));
                wantedSizeLabel->setText(formatutils::formatByteSize(mTorrent->data().sizeWhenDone));
                qint64 completedFilesSize = 0;
                for (const auto& file : mTorrent->files()) {
                    if (file.size > 0 && file.completedSize >= file.size) {
                        completedFilesSize += file.size;
                    }
                }
                completedSizeLabel->setText(formatutils::formatByteSize(completedFilesSize));
                locationLabel->setText(
                    toNativeSeparators(mTorrent->data().downloadDirectory, mRpc->serverSettings()->data().pathOs)
                );
                incompleteDirectoryLabel->setText(
                    toNativeSeparators(mTorrent->data().incompleteDirectory, mRpc->serverSettings()->data().pathOs)
                );
                hashLabel->setText(mTorrent->data().hashString);
                creatorLabel->setText(mTorrent->data().creator);
                creationDateLabel->setText(
                    formatutils::formatDateTime(mTorrent->data().creationDate.toLocalTime(), QLocale::LongFormat)
                );
                if (mTorrent->data().comment != commentTextEdit->toPlainText()) {
                    commentTextEdit->document()->setPlainText(mTorrent->data().comment);
                    desktoputils::findLinksAndAddAnchors(commentTextEdit->document());
                }
                const auto& topicName = mTorrent->data().topicName;
                const auto& topicUrl = mTorrent->data().topicUrl;
                const auto topicText = topicName.isEmpty() ? topicUrl : topicName;
                if (!topicUrl.isEmpty()) {
                    topicLabel->setText(
                        QStringLiteral("<a href=\"%1\">%2</a>")
                            .arg(topicUrl.toHtmlEscaped(), topicText.toHtmlEscaped())
                    );
                } else {
                    topicLabel->setText(topicText.toHtmlEscaped());
                }
                labelsModel->setStringList(mTorrent->data().labels);
            } else {
                completedLabel->clear();
                downloadedLabel->clear();
                uploadedLabel->clear();
                ratioLabel->clear();
                downloadSpeedLabel->clear();
                uploadSpeedLabel->clear();
                etaLabel->clear();
                seedersLabel->clear();
                leechersLabel->clear();
                peersSendingToUsLabel->clear();
                webSeedersSendingToUsLabel->clear();
                peersGettingFromUsLabel->clear();
                lastActivityLabel->clear();
                errorLabel->clear();
                totalSizeLabel->clear();
                wantedSizeLabel->clear();
                completedSizeLabel->clear();
                locationLabel->clear();
                incompleteDirectoryLabel->clear();
                hashLabel->clear();
                creatorLabel->clear();
                creationDateLabel->clear();
                commentTextEdit->clear();
                topicLabel->clear();
                labelsModel->setStringList({});
            }
            const bool incompleteDirectoryVisible = mTorrent && !mTorrent->data().incompleteDirectory.isEmpty();
            incompleteDirectoryLabel->setVisible(incompleteDirectoryVisible);
            incompleteDirectoryLabelLabel->setVisible(incompleteDirectoryVisible);
            const bool errorVisible = mTorrent && mTorrent->data().hasError() && !mTorrent->data().errorString.isEmpty();
            errorLabel->setVisible(errorVisible);
            errorLabelLabel->setVisible(errorVisible);
            const bool topicVisible = mTorrent && (!mTorrent->data().topicName.isEmpty() || !mTorrent->data().topicUrl.isEmpty());
            topicLabel->setVisible(topicVisible);
            topicLabelLabel->setVisible(topicVisible);
            const bool labelsViewVisible = labelsModel->rowCount() > 0;
            labelsView->setVisible(labelsViewVisible);
            labelsLabel->setVisible(labelsViewVisible);
        };

        QObject::connect(Settings::instance(), &Settings::displayRelativeTimeChanged, this, mUpdateDetailsTab);
    }

    void TorrentPropertiesWidget::setupPeersTab() {
        mPeersModel = new PeersModel(this);
        auto peersProxyModel =
            new BaseProxyModel(mPeersModel, PeersModel::SortRole, static_cast<int>(PeersModel::Column::Address), this);

        auto peersTab = new QWidget(this);
        auto peersTabLayout = new QVBoxLayout(peersTab);

        mPeersView = new QTreeView(this);
        setCommonTreeViewProperties(mPeersView, true);
        mPeersView->setItemDelegate(new TooltipWhenElidedDelegate(this));
        mPeersView->setItemDelegateForColumn(
            static_cast<int>(PeersModel::Column::ProgressBar),
            new ProgressBarDelegate(PeersModel::SortRole, this)
        );
        mPeersView->setModel(peersProxyModel);
        mPeersView->header()->restoreState(Settings::instance()->get_peersViewHeaderState());
        overrideBreezeFramelessScrollAreaHeuristic(mPeersView, true);

        peersTabLayout->addWidget(mPeersView);
        //: Torrent's properties dialog tab
        addTab(peersTab, qApp->translate("tremotesf", "Peers"));
    }

    void TorrentPropertiesWidget::setupWebSeedersTab() {
        //: Web seeders list column title
        mWebSeedersModel = new StringListModel(qApp->translate("tremotesf", "Web seeder"), {}, this);
        auto webSeedersProxyModel = new BaseProxyModel(mWebSeedersModel, Qt::DisplayRole, std::nullopt, this);

        auto webSeedersTab = new QWidget(this);
        auto webSeedersTabLayout = new QVBoxLayout(webSeedersTab);

        auto webSeedersView = new QTreeView(this);
        setCommonTreeViewProperties(webSeedersView, true);
        webSeedersView->header()->setContextMenuPolicy(Qt::DefaultContextMenu);
        webSeedersView->setModel(webSeedersProxyModel);
        overrideBreezeFramelessScrollAreaHeuristic(webSeedersView, true);

        webSeedersTabLayout->addWidget(webSeedersView);

        //: Torrent's properties dialog tab
        addTab(webSeedersTab, qApp->translate("tremotesf", "Web seeders"));
    }

    void TorrentPropertiesWidget::setupLimitsTab() {
        auto limitsTab = new QScrollArea(this);
        limitsTab->setWidgetResizable(true);
        limitsTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        makeScrollAreaTransparent(limitsTab);
        //: Torrent's properties dialog tab
        addTab(limitsTab, qApp->translate("tremotesf", "Limits"));

        auto limitsTabScrollContent = new QWidget(limitsTab);
        limitsTab->setWidget(limitsTabScrollContent);

        auto limitsTabLayout = new QVBoxLayout(limitsTabScrollContent);

        //
        // Speed group box
        //
        //: Torrent's limits tab section
        auto speedGroupBox = new QGroupBox(qApp->translate("tremotesf", "Speed"), this);
        auto speedGroupBoxLayout = new QFormLayout(speedGroupBox);
        speedGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        speedGroupBoxLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        //: Check box label
        auto globalLimitsCheckBox = new QCheckBox(qApp->translate("tremotesf", "Honor global limits"), this);
        speedGroupBoxLayout->addRow(globalLimitsCheckBox);

        //: Download speed limit input field label
        auto downloadSpeedCheckBox = new QCheckBox(qApp->translate("tremotesf", "Download:"), this);
        speedGroupBoxLayout->addRow(downloadSpeedCheckBox);

        auto downloadSpeedSpinBoxLayout = new QHBoxLayout();
        speedGroupBoxLayout->addRow(downloadSpeedSpinBoxLayout);
        auto downloadSpeedSpinBox = new QSpinBox(this);
        downloadSpeedSpinBox->setEnabled(false);
        setSpinBoxLimits(downloadSpeedSpinBox, speedLimitKbpsRange);
        //: Suffix that is added to input field with download/upload speed limit, e.g. "5000 kB/s". 'k' prefix means SI prefix, i.e kB = 1000 bytes
        downloadSpeedSpinBox->setSuffix(qApp->translate("tremotesf", " kB/s"));

        QObject::connect(downloadSpeedCheckBox, &QCheckBox::toggled, downloadSpeedSpinBox, &QSpinBox::setEnabled);

        downloadSpeedSpinBoxLayout->addSpacing(28);
        downloadSpeedSpinBoxLayout->addWidget(downloadSpeedSpinBox);

        //: Upload speed limit input field label
        auto uploadSpeedCheckBox = new QCheckBox(qApp->translate("tremotesf", "Upload:"), this);
        speedGroupBoxLayout->addRow(uploadSpeedCheckBox);

        auto uploadSpeedSpinBoxLayout = new QHBoxLayout();
        speedGroupBoxLayout->addRow(uploadSpeedSpinBoxLayout);
        auto uploadSpeedSpinBox = new QSpinBox(this);
        uploadSpeedSpinBox->setEnabled(false);
        setSpinBoxLimits(uploadSpeedSpinBox, speedLimitKbpsRange);
        //: Suffix that is added to input field with download/upload speed limit, e.g. "5000 kB/s". 'k' prefix means SI prefix, i.e kB = 1000 bytes
        uploadSpeedSpinBox->setSuffix(qApp->translate("tremotesf", " kB/s"));

        QObject::connect(uploadSpeedCheckBox, &QCheckBox::toggled, uploadSpeedSpinBox, &QSpinBox::setEnabled);

        uploadSpeedSpinBoxLayout->addSpacing(28);
        uploadSpeedSpinBoxLayout->addWidget(uploadSpeedSpinBox);

        auto priorityComboBox = new QComboBox(this);
        for (const TorrentData::Priority priority : priorityComboBoxItems) {
            switch (priority) {
            case TorrentData::Priority::High:
                //: Torrent's loading priority
                priorityComboBox->addItem(qApp->translate("tremotesf", "High"));
                break;
            case TorrentData::Priority::Normal:
                //: Torrent's loading priority
                priorityComboBox->addItem(qApp->translate("tremotesf", "Normal"));
                break;
            case TorrentData::Priority::Low:
                //: Torrent's loading priority
                priorityComboBox->addItem(qApp->translate("tremotesf", "Low"));
                break;
            }
        }
        speedGroupBoxLayout->addRow(qApp->translate("tremotesf", "Torrent priority:"), priorityComboBox);

        limitsTabLayout->addWidget(speedGroupBox);

        //
        // Seeding group box
        //
        //: Torrent's limits tab section
        auto seedingGroupBox = new QGroupBox(qApp->translate("tremotesf", "Seeding", "Options section"), this);
        auto seedingGroupBoxLayout = new QFormLayout(seedingGroupBox);
        seedingGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        auto ratioLimitLayout = new QHBoxLayout();
        seedingGroupBoxLayout->addRow(qApp->translate("tremotesf", "Ratio limit mode:"), ratioLimitLayout);
        ratioLimitLayout->setContentsMargins(0, 0, 0, 0);
        auto ratioLimitComboBox = new QComboBox(this);
        for (const TorrentData::RatioLimitMode mode : ratioLimitComboBoxItems) {
            switch (mode) {
            case TorrentData::RatioLimitMode::Global:
                //: Seeding ratio limit mode (global settings/stop at ratio/unlimited)
                ratioLimitComboBox->addItem(qApp->translate("tremotesf", "Use global settings"));
                break;
            case TorrentData::RatioLimitMode::Single:
                //: Seeding ratio limit mode (global settings/stop at ratio/unlimited)
                ratioLimitComboBox->addItem(qApp->translate("tremotesf", "Stop seeding at ratio:"));
                break;
            case TorrentData::RatioLimitMode::Unlimited:
                //: Seeding ratio limit mode (global settings/stop at ratio/unlimited)
                ratioLimitComboBox->addItem(qApp->translate("tremotesf", "Seed regardless of ratio"));
                break;
            }
        }

        ratioLimitLayout->addWidget(ratioLimitComboBox);
        auto ratioLimitSpinBox = new QDoubleSpinBox(this);
        setSpinBoxLimits(ratioLimitSpinBox, nonNegativeDecimalsRange);
        ratioLimitSpinBox->setSingleStep(0.1);
        ratioLimitSpinBox->setVisible(false);
        QObject::connect(
            ratioLimitComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [ratioLimitSpinBox](int index) {
                if (index == indexOfCasted<int>(ratioLimitComboBoxItems, TorrentData::RatioLimitMode::Single)) {
                    ratioLimitSpinBox->show();
                } else {
                    ratioLimitSpinBox->hide();
                }
            }
        );
        ratioLimitLayout->addWidget(ratioLimitSpinBox);

        auto idleSeedingLimitLayout = new QHBoxLayout();
        seedingGroupBoxLayout->addRow(qApp->translate("tremotesf", "Idle seeding mode:"), idleSeedingLimitLayout);
        idleSeedingLimitLayout->setContentsMargins(0, 0, 0, 0);
        auto idleSeedingLimitComboBox = new QComboBox(this);
        for (const TorrentData::IdleSeedingLimitMode mode : idleSeedingLimitComboBoxItems) {
            switch (mode) {
            case TorrentData::IdleSeedingLimitMode::Global:
                //: Seeding idle limit mode (global settings/stop if idle for/unlimited)
                idleSeedingLimitComboBox->addItem(qApp->translate("tremotesf", "Use global settings"));
                break;
            case TorrentData::IdleSeedingLimitMode::Single:
                //: Seeding idle limit mode (global settings/stop if idle for/unlimited)
                idleSeedingLimitComboBox->addItem(qApp->translate("tremotesf", "Stop seeding if idle for:"));
                break;
            case TorrentData::IdleSeedingLimitMode::Unlimited:
                //: Seeding idle limit mode (global settings/stop if idle for/unlimited)
                idleSeedingLimitComboBox->addItem(qApp->translate("tremotesf", "Seed regardless of activity"));
                break;
            }
        }

        idleSeedingLimitLayout->addWidget(idleSeedingLimitComboBox);
        auto idleSeedingLimitSpinBox = new QSpinBox(this);
        setSpinBoxLimits(idleSeedingLimitSpinBox, minutesRange);
        //: Suffix that is added to input field with number of minuts, e.g. "5 min"
        idleSeedingLimitSpinBox->setSuffix(qApp->translate("tremotesf", " min"));

        idleSeedingLimitSpinBox->setVisible(false);
        QObject::connect(
            idleSeedingLimitComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [idleSeedingLimitSpinBox](int index) {
                if (index
                    == indexOfCasted<int>(idleSeedingLimitComboBoxItems, TorrentData::IdleSeedingLimitMode::Single)) {
                    idleSeedingLimitSpinBox->show();
                } else {
                    idleSeedingLimitSpinBox->hide();
                }
            }
        );
        idleSeedingLimitLayout->addWidget(idleSeedingLimitSpinBox);

        limitsTabLayout->addWidget(seedingGroupBox);

        //
        // Peers group box
        //
        //: Torrent's limits tab section
        auto peersGroupBox = new QGroupBox(qApp->translate("tremotesf", "Peers"), this);
        auto peersGroupBoxLayout = new QFormLayout(peersGroupBox);
        peersGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        auto peersLimitsSpinBox = new QSpinBox(this);
        setSpinBoxLimits(peersLimitsSpinBox, peersLimitRange);
        peersGroupBoxLayout->addRow(qApp->translate("tremotesf", "Maximum peers:"), peersLimitsSpinBox);

        limitsTabLayout->addWidget(peersGroupBox);

        limitsTabLayout->addStretch();

        auto resizer = new KColumnResizer(this);
        resizer->addWidgetsFromLayout(speedGroupBoxLayout);
        resizer->addWidgetsFromLayout(seedingGroupBoxLayout);
        resizer->addWidgetsFromLayout(peersGroupBoxLayout);

        QObject::connect(globalLimitsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            if (!mUpdatingLimits && mTorrent) {
                mTorrent->setHonorSessionLimits(checked);
            }
        });
        QObject::connect(downloadSpeedCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            if (!mUpdatingLimits && mTorrent) {
                mTorrent->setDownloadSpeedLimited(checked);
            }
        });
        QObject::connect(
            downloadSpeedSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int limit) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setDownloadSpeedLimit(limit);
                }
            }
        );
        QObject::connect(uploadSpeedCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            if (!mUpdatingLimits && mTorrent) {
                mTorrent->setUploadSpeedLimited(checked);
            }
        });
        QObject::connect(
            uploadSpeedSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int limit) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setUploadSpeedLimit(limit);
                }
            }
        );
        QObject::connect(
            priorityComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setBandwidthPriority(priorityComboBoxItems.at(static_cast<size_t>(index)));
                }
            }
        );
        QObject::connect(
            ratioLimitComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setRatioLimitMode(ratioLimitComboBoxItems.at(static_cast<size_t>(index)));
                }
            }
        );
        QObject::connect(
            ratioLimitSpinBox,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double limit) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setRatioLimit(limit);
                }
            }
        );
        QObject::connect(
            idleSeedingLimitComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setIdleSeedingLimitMode(idleSeedingLimitComboBoxItems.at(static_cast<size_t>(index)));
                }
            }
        );
        QObject::connect(
            idleSeedingLimitSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int limit) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setIdleSeedingLimit(limit);
                }
            }
        );
        QObject::connect(
            peersLimitsSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int limit) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setPeersLimit(limit);
                }
            }
        );

        mUpdateLimitsTab = [=, this] {
            mUpdatingLimits = true;

            if (mTorrent) {
                globalLimitsCheckBox->setChecked(mTorrent->data().honorSessionLimits);
                downloadSpeedCheckBox->setChecked(mTorrent->data().downloadSpeedLimited);
                downloadSpeedSpinBox->setValue(mTorrent->data().downloadSpeedLimit);
                uploadSpeedCheckBox->setChecked(mTorrent->data().uploadSpeedLimited);
                uploadSpeedSpinBox->setValue(mTorrent->data().uploadSpeedLimit);
                priorityComboBox->setCurrentIndex(
                    indexOfCasted<int>(priorityComboBoxItems, mTorrent->data().bandwidthPriority).value()
                );
                ratioLimitComboBox->setCurrentIndex(
                    indexOfCasted<int>(ratioLimitComboBoxItems, mTorrent->data().ratioLimitMode).value()
                );
                ratioLimitSpinBox->setValue(mTorrent->data().ratioLimit);
                idleSeedingLimitComboBox->setCurrentIndex(
                    indexOfCasted<int>(idleSeedingLimitComboBoxItems, mTorrent->data().idleSeedingLimitMode).value()
                );
                idleSeedingLimitSpinBox->setValue(mTorrent->data().idleSeedingLimit);
                peersLimitsSpinBox->setValue(mTorrent->data().peersLimit);
            } else {
                globalLimitsCheckBox->setChecked(false);
                downloadSpeedCheckBox->setChecked(false);
                downloadSpeedSpinBox->clear();
                uploadSpeedCheckBox->setChecked(false);
                uploadSpeedSpinBox->clear();
                priorityComboBox->setCurrentIndex(0);
                ratioLimitComboBox->setCurrentIndex(0);
                ratioLimitSpinBox->clear();
                idleSeedingLimitComboBox->setCurrentIndex(0);
                idleSeedingLimitSpinBox->clear();
                peersLimitsSpinBox->clear();
            }

            mUpdatingLimits = false;
        };
    }

    void TorrentPropertiesWidget::setTorrent(Torrent* torrent) { setTorrent(torrent, false); }

    void TorrentPropertiesWidget::setTorrent(Torrent* torrent, bool oldTorrentDestroyed) {
        if (torrent == mTorrent) {
            return;
        }
        if (mTorrent && !oldTorrentDestroyed) {
            QObject::disconnect(mTorrent, nullptr, this, nullptr);
        }

        const bool hadTorrent = mTorrent != nullptr;
        mTorrent = torrent;

        const auto update = [this] {
            mUpdateDetailsTab();
            if (mTorrent) {
                mWebSeedersModel->setStringList(mTorrent->data().webSeeders);
            } else {
                mWebSeedersModel->setStringList({});
            }
            mUpdateLimitsTab();
        };
        update();
        if (mTorrent) {
            QObject::connect(mTorrent, &Torrent::changed, this, update);
            QObject::connect(mTorrent, &Torrent::filesUpdated, this, [this] { mUpdateDetailsTab(); });
            QObject::connect(mTorrent, &QObject::destroyed, this, [this] { setTorrent(nullptr, true); });
        }
        mFilesModel->setTorrent(mTorrent, oldTorrentDestroyed);
        mTrackersViewWidget->setTorrent(mTorrent, oldTorrentDestroyed);
        mPeersModel->setTorrent(mTorrent, oldTorrentDestroyed);

        const auto onHasTorrentChanged = [this](bool hasTorrent) {
            setEnabled(hasTorrent);
            emit hasTorrentChanged(hasTorrent);
        };
        if (hadTorrent && !mTorrent) {
            onHasTorrentChanged(false);
        } else if (!hadTorrent && mTorrent) {
            onHasTorrentChanged(true);
        }
    }
}
