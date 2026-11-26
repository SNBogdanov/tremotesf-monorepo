// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serversettingsdialog.h"

#include <array>
#include <optional>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <KColumnResizer>
#include <KMessageWidget>
#include <KPageModel>
#include <KPageWidget>

#include "stdutils.h"
#include "formatutils.h"
#include "log/log.h"
#include "rpc/numbervaluerange.h"
#include "settings.h"
#include "rpc/pathutils.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "scriptparameterspage.h"
#include "ui/widgets/remotedirectoryselectionwidget.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        struct ParsedSize {
            quint64 value{};
            QChar unit = u'B';
        };

        std::optional<ParsedSize> parseSize(const QString& text) {
            const auto trimmed = text.trimmed();
            if (trimmed.isEmpty()) {
                return std::nullopt;
            }
            int digits = 0;
            while (digits < trimmed.size() && trimmed[digits].isDigit()) {
                ++digits;
            }
            if (digits == 0) {
                return std::nullopt;
            }
            bool ok = false;
            const auto value = trimmed.left(digits).toULongLong(&ok);
            if (!ok) {
                return std::nullopt;
            }
            const auto suffix = trimmed.mid(digits).trimmed().toUpper();
            QChar unit = u'B';
            if (!suffix.isEmpty()) {
                const QChar first = suffix.front();
                if (first == u'B') {
                    unit = u'B';
                } else if (first == u'K' || first == u'M' || first == u'G' || first == u'T') {
                    unit = first;
                } else {
                    return std::nullopt;
                }
            }
            return ParsedSize{value, unit};
        }

        QString unitLabel(QChar unit) {
            switch (unit.unicode()) {
            case u'B':
                //: Size unit, bytes
                return qApp->translate("tremotesf", "B");
            case u'K':
                //: Size unit, kilobytes (1024 bytes)
                return qApp->translate("tremotesf", "KB");
            case u'M':
                //: Size unit, megabytes (1024^2 bytes)
                return qApp->translate("tremotesf", "MB");
            case u'G':
                //: Size unit, gigabytes (1024^3 bytes)
                return qApp->translate("tremotesf", "GB");
            case u'T':
                //: Size unit, terabytes (1024^4 bytes)
                return qApp->translate("tremotesf", "TB");
            default:
                return QString(unit) + QStringLiteral("B");
            }
        }

        constexpr std::array sizeUnits{u'B', u'K', u'M', u'G', u'T'};
        constexpr std::array encryptionModeComboBoxItems{
            ServerSettingsData::EncryptionMode::Allowed,
            ServerSettingsData::EncryptionMode::Preferred,
            ServerSettingsData::EncryptionMode::Required
        };

        ServerSettingsData::EncryptionMode encryptionModeFromComboBoxItem(int index) {
            if (index == -1) {
                return {};
            }
            return encryptionModeComboBoxItems.at(static_cast<size_t>(index));
        }
    }

    ServerSettingsDialog::ServerSettingsDialog(Rpc* rpc, QWidget* parent) : QDialog(parent), mRpc(rpc) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Server Options"));

        setupUi();
        mScriptParametersPage = new ScriptParametersPage(mRpc, this);
        if (!restoreGeometry(Settings::instance()->get_serverSettingsDialogGeometry())) {
            resize(sizeHint().expandedTo(QSize(700, 550)));
        }
        if (mMinFreeSpaceTable) {
            mMinFreeSpaceTable->horizontalHeader()->restoreState(
                Settings::instance()->get_minFreeSpaceTableHeaderState()
            );
        }

        const auto onConnectedChanged = [this] {
            if (mRpc->isConnected()) {
                mDisconnectedMessageWidget->animatedHide();
                mDisconnectedMessageWidget->setEnabled(true);
                mDownloadingPageWidget->setEnabled(true);
                mSeedingPageWidget->setEnabled(true);
                mQueuePageWidget->setEnabled(true);
                mSpeedPageWidget->setEnabled(true);
                mNetworkPageWidget->setEnabled(true);
                loadSettings();
                updateScriptParametersPage();
            } else {
                mDisconnectedMessageWidget->hide();
                mDisconnectedMessageWidget->animatedShow();
                mDownloadingPageWidget->setEnabled(false);
                mSeedingPageWidget->setEnabled(false);
                mQueuePageWidget->setEnabled(false);
                mSpeedPageWidget->setEnabled(false);
                mNetworkPageWidget->setEnabled(false);
            }
        };
        QObject::connect(mRpc, &Rpc::connectedChanged, this, onConnectedChanged);
        onConnectedChanged();
        QObject::connect(
            mRpc->serverSettings(),
            &ServerSettings::changed,
            this,
            &ServerSettingsDialog::updateScriptParametersPage
        );

        QObject::connect(this, &QDialog::finished, this, [this] {
            Settings::instance()->set_serverSettingsDialogGeometry(saveGeometry());
            Settings::instance()->set_minFreeSpaceTableHeaderState(mMinFreeSpaceTable->horizontalHeader()->saveState());
        });
    }

    void ServerSettingsDialog::addMinFreeSpaceRow(const QString& path, quint64 value, QChar unit) {
        const int row = mMinFreeSpaceTable->rowCount();
        mMinFreeSpaceTable->insertRow(row);
        mMinFreeSpaceTable->setItem(row, 0, new QTableWidgetItem(path));
        mMinFreeSpaceTable->setItem(row, 1, new QTableWidgetItem(QString::number(value)));
        auto* const unitComboBox = new QComboBox(mMinFreeSpaceTable);
        for (const auto sizeUnit : sizeUnits) {
            unitComboBox->addItem(unitLabel(sizeUnit), QString(sizeUnit));
        }
        const int unitIndex = unitComboBox->findData(QString(unit));
        unitComboBox->setCurrentIndex(unitIndex >= 0 ? unitIndex : unitComboBox->findData(QStringLiteral("G")));
        mMinFreeSpaceTable->setCellWidget(row, 2, unitComboBox);
        auto* const freeSpaceItem = new QTableWidgetItem();
        freeSpaceItem->setFlags(freeSpaceItem->flags() & ~Qt::ItemIsEditable);
        mMinFreeSpaceTable->setItem(row, 3, freeSpaceItem);
    }

    void ServerSettingsDialog::removeSelectedMinFreeSpaceRows() {
        const auto selectedRows = mMinFreeSpaceTable->selectionModel()->selectedRows();
        for (auto iterator = selectedRows.crbegin(), end = selectedRows.crend(); iterator != end; ++iterator) {
            mMinFreeSpaceTable->removeRow(iterator->row());
        }
    }

    void ServerSettingsDialog::updateMinFreeSpaceFreeSpace(int row) {
        if (row < 0 || row >= mMinFreeSpaceTable->rowCount()) {
            return;
        }
        const auto* const pathItem = mMinFreeSpaceTable->item(row, 0);
        auto* const freeSpaceItem = mMinFreeSpaceTable->item(row, 3);
        if (!pathItem || !freeSpaceItem) {
            return;
        }
        const auto path = pathItem->text().trimmed();
        if (path.isEmpty()) {
            freeSpaceItem->setText({});
            return;
        }
        freeSpaceItem->setText(QStringLiteral("…"));
        const auto serverPath = normalizePath(path, mRpc->serverSettings()->data().pathOs);
        info().log("Min free space: requesting for row {} path '{}' (server path '{}')", row, path, serverPath);
        mRpc->requestFreeSpaceForPath(serverPath, this, [this, row, path](std::optional<qint64> freeSpace) {
            info().log(
                "Min free space: row {} path '{}' result {}",
                row,
                path,
                freeSpace ? QString::number(*freeSpace) : QStringLiteral("nullopt")
            );
            if (row >= mMinFreeSpaceTable->rowCount()) {
                return;
            }
            const auto* const currentPathItem = mMinFreeSpaceTable->item(row, 0);
            if (!currentPathItem || currentPathItem->text().trimmed() != path) {
                return;
            }
            auto* const currentFreeSpaceItem = mMinFreeSpaceTable->item(row, 3);
            if (!currentFreeSpaceItem) {
                return;
            }
            currentFreeSpaceItem->setText(
                freeSpace ? formatutils::formatByteSize(*freeSpace) : qApp->translate("tremotesf", "Error")
            );
        });
    }

    void ServerSettingsDialog::accept() {
        ServerSettings* settings = mRpc->serverSettings();

        settings->setSaveOnSet(false);

        settings->setDownloadDirectory(mDownloadDirectoryWidget->path());
        settings->setStartAddedTorrents(mStartAddedTorrentsCheckBox->isChecked());
        if (settings->data().hasSequentialDownload) {
            settings->setSequentialDownload(mSequentialDownloadCheckBox->isChecked());
        }
        //settings->setTrashTorrentFiles(mTrashTorrentFilesCheckBox->isChecked());
        settings->setRenameIncompleteFiles(mIncompleteFilesCheckBox->isChecked());
        settings->setIncompleteDirectoryEnabled(mIncompleteDirectoryCheckBox->isChecked());
        settings->setIncompleteDirectory(mIncompleteDirectoryWidget->path());

        settings->setRatioLimited(mRatioLimitCheckBox->isChecked());
        settings->setRatioLimit(mRatioLimitSpinBox->value());
        settings->setIdleSeedingLimited(mIdleSeedingLimitCheckBox->isChecked());
        settings->setIdleSeedingLimit(mIdleSeedingLimitSpinBox->value());

        settings->setDownloadQueueEnabled(mMaximumActiveDownloadsCheckBox->isChecked());
        settings->setDownloadQueueSize(mMaximumActiveDownloadsSpinBox->value());
        settings->setSeedQueueEnabled(mMaximumActiveUploadsCheckBox->isChecked());
        settings->setSeedQueueSize(mMaximumActiveUploadsSpinBox->value());
        settings->setIdleQueueLimited(mIdleQueueLimitCheckBox->isChecked());
        settings->setIdleQueueLimit(mIdleQueueLimitSpinBox->value());
        settings->setAutoPriorityEnabled(mAutoPriorityCheckBox->isChecked());
        settings->setAutoPriorityMaxHigh(mAutoPriorityMaxHighSpinBox->value());
        settings->setAutoPrioritySort(
            mAutoPrioritySortComboBox->currentData().value<ServerSettingsData::AutoPrioritySort>()
        );
        settings->setQueueMoveToEndEnabled(mQueueMoveToEndCheckBox->isChecked());
        settings->setQueueMoveToEndMinutes(mQueueMoveToEndMinutesSpinBox->value());
        QStringList minFreeSpaceEntries;
        for (int row = 0, count = mMinFreeSpaceTable->rowCount(); row < count; ++row) {
            const auto* pathItem = mMinFreeSpaceTable->item(row, 0);
            const auto* valueItem = mMinFreeSpaceTable->item(row, 1);
            const auto* unitComboBox = qobject_cast<QComboBox*>(mMinFreeSpaceTable->cellWidget(row, 2));
            if (!pathItem || !valueItem || !unitComboBox) {
                continue;
            }
            const auto path = pathItem->text().trimmed();
            bool ok = false;
            const auto value = valueItem->text().trimmed().toULongLong(&ok);
            if (!path.isEmpty() && ok) {
                minFreeSpaceEntries.append(QString("%1=%2%3"_L1)
                                               .arg(
                                                   normalizePath(path, settings->data().pathOs),
                                                   QString::number(value),
                                                   unitComboBox->currentData().toString()
                                               ));
            }
        }
        settings->setMinFreeSpace(minFreeSpaceEntries);
        settings->setMinFreeSpaceResumeMargin(mMinFreeSpaceResumeMarginEdit->text());
        settings->setMoveCompletedTorrentsEnabled(mMoveCompletedTorrentsCheckBox->isChecked());
        settings->setMovePartialFilesEnabled(mMovePartialFilesCheckBox->isChecked());
        settings->setForceFinishPartialFilesEnabled(mForceFinishPartialFilesCheckBox->isChecked());
        settings->setScriptTorrentDoneRemoveTorrentEnabled(mScriptTorrentDoneRemoveTorrentCheckBox->isChecked());
        settings->setScriptTorrentDoneRemoveTempFilesEnabled(mScriptTorrentDoneRemoveTempFilesCheckBox->isChecked());
        settings->setScriptTorrentDoneRemoveAllFilesEnabled(mScriptTorrentDoneRemoveAllFilesCheckBox->isChecked());

        settings->setDownloadSpeedLimited(mDownloadSpeedLimitCheckBox->isChecked());
        settings->setDownloadSpeedLimit(mDownloadSpeedLimitSpinBox->value());
        settings->setUploadSpeedLimited(mUploadSpeedLimitCheckBox->isChecked());
        settings->setUploadSpeedLimit(mUploadSpeedLimitSpinBox->value());
        settings->setAlternativeSpeedLimitsEnabled(mEnableAlternativeSpeedLimitsGroupBox->isChecked());
        settings->setAlternativeDownloadSpeedLimit(mAlternativeDownloadSpeedLimitSpinBox->value());
        settings->setAlternativeUploadSpeedLimit(mAlternativeUploadSpeedLimitSpinBox->value());
        settings->setAlternativeSpeedLimitsScheduled(mLimitScheduleGroupBox->isChecked());
        settings->setAlternativeSpeedLimitsBeginTime(mLimitScheduleBeginTimeEdit->time());
        settings->setAlternativeSpeedLimitsEndTime(mLimitScheduleEndTimeEdit->time());

        settings->setAlternativeSpeedLimitsDays(
            static_cast<ServerSettingsData::AlternativeSpeedLimitsDays>(
                mLimitScheduleDaysComboBox->currentData().toInt()
            )
        );

        settings->setPeerPort(mPeerPortSpinBox->value());
        settings->setRandomPortEnabled(mRandomPortCheckBox->isChecked());
        settings->setPortForwardingEnabled(mPortForwardingCheckBox->isChecked());
        settings->setEncryptionMode(encryptionModeFromComboBoxItem(mEncryptionComboBox->currentIndex()));
        settings->setUtpEnabled(mUtpCheckBox->isChecked());
        settings->setPexEnabled(mPexCheckBox->isChecked());
        settings->setDhtEnabled(mDhtCheckBox->isChecked());
        settings->setLpdEnabled(mLpdCheckBox->isChecked());
        settings->setMaximumPeersPerTorrent(mTorrentPeerLimitSpinBox->value());
        settings->setMaximumPeersGlobally(mGlobalPeerLimitSpinBox->value());

        const auto finish = [this, settings] {
            settings->save();
            settings->setSaveOnSet(true);
            QDialog::accept();
        };

        if (mScriptParametersPage && mScriptParametersPageItem && mScriptParametersPage->hasChanges()) {
            mScriptParametersPage->save([this, finish](bool success, const QString& error) {
                if (!success) {
                    QMessageBox::warning(
                        this,
                        qApp->translate("tremotesf", "Failed to save script settings"),
                        error
                    );
                    return;
                }
                finish();
            });
            return;
        }

        finish();
    }

    void ServerSettingsDialog::setupUi() {
        //
        // Creating layout
        //

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        //: Message that appears when disconnected from server
        mDisconnectedMessageWidget = new KMessageWidget(qApp->translate("tremotesf", "Disconnected"), this);
        mDisconnectedMessageWidget->setCloseButtonVisible(false);
        mDisconnectedMessageWidget->setMessageType(KMessageWidget::Warning);
        mDisconnectedMessageWidget->setVisible(false);
        layout->addWidget(mDisconnectedMessageWidget);

        mPageWidget = new KPageWidget(this);

        // Downloading page
        mDownloadingPageWidget = new QWidget(this);

        KPageWidgetItem* downloadingPageItem = mPageWidget->addPage(
            mDownloadingPageWidget,
            //: "Downloading" server setting page
            qApp->translate("tremotesf", "Downloading", "Noun")
        );
        downloadingPageItem->setIcon(QIcon::fromTheme("folder-download"_L1));

        auto downloadingPageLayout = new QFormLayout(mDownloadingPageWidget);
        downloadingPageLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mDownloadDirectoryWidget = new RemoteDirectorySelectionWidget(this);
        mDownloadDirectoryWidget->setup({}, mRpc);
        downloadingPageLayout->addRow(qApp->translate("tremotesf", "Download directory:"), mDownloadDirectoryWidget);

        //: Check box label
        mStartAddedTorrentsCheckBox = new QCheckBox(qApp->translate("tremotesf", "Start added torrents"), this);
        downloadingPageLayout->addRow(mStartAddedTorrentsCheckBox);

        mSequentialDownloadCheckBox = new QCheckBox(
            //: Check box label
            qApp->translate("tremotesf", "Download sequentially by default"),
            this
        );
        downloadingPageLayout->addRow(mSequentialDownloadCheckBox);

        /*mTrashTorrentFilesCheckBox = new QCheckBox(qApp->translate("tremotesf", "Trash .torrent files"), this);
        downloadingPageLayout->addRow(mTrashTorrentFilesCheckBox);*/

        mIncompleteFilesCheckBox = new QCheckBox(
            //: Check box label
            qApp->translate("tremotesf", "Append \".part\" to names of incomplete files"),
            this
        );
        downloadingPageLayout->addRow(mIncompleteFilesCheckBox);

        mIncompleteDirectoryCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Directory for incomplete files:"), this);
        downloadingPageLayout->addRow(mIncompleteDirectoryCheckBox);

        auto incompleteDirectoryWidgetLayout = new QHBoxLayout();
        downloadingPageLayout->addRow(incompleteDirectoryWidgetLayout);
        mIncompleteDirectoryWidget = new RemoteDirectorySelectionWidget(this);
        mIncompleteDirectoryWidget->setup({}, mRpc);
        mIncompleteDirectoryWidget->setEnabled(false);
        QObject::connect(
            mIncompleteDirectoryCheckBox,
            &QCheckBox::toggled,
            mIncompleteDirectoryWidget,
            &RemoteDirectorySelectionWidget::setEnabled
        );
        //downloadingPageLayout->addRow(mIncompleteDirectoryCheckBox, mIncompleteDirectoryWidget);
        incompleteDirectoryWidgetLayout->addSpacing(28);
        incompleteDirectoryWidgetLayout->addWidget(mIncompleteDirectoryWidget);

        mMinFreeSpaceTable = new QTableWidget(0, 4, this);
        mMinFreeSpaceTable->setToolTip(
            qApp->translate("tremotesf", "Minimum free space rules, for example /downloads=5G")
        );
        mMinFreeSpaceTable->setHorizontalHeaderLabels(
            {qApp->translate("tremotesf", "Path"),
             qApp->translate("tremotesf", "Value"),
             qApp->translate("tremotesf", "Units"),
             qApp->translate("tremotesf", "Current")}
        );
        auto* const minFreeSpaceHeader = mMinFreeSpaceTable->horizontalHeader();
        minFreeSpaceHeader->setSectionResizeMode(QHeaderView::Interactive);
        minFreeSpaceHeader->setStretchLastSection(false);
        mMinFreeSpaceTable->setColumnWidth(0, 240);
        mMinFreeSpaceTable->setColumnWidth(1, 100);
        mMinFreeSpaceTable->setColumnWidth(2, 90);
        mMinFreeSpaceTable->setColumnWidth(3, 120);
        mMinFreeSpaceTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        mMinFreeSpaceTable->setWordWrap(false);
        mMinFreeSpaceTable->verticalHeader()->setVisible(false);
        mMinFreeSpaceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        mMinFreeSpaceTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
        mMinFreeSpaceTable->setMinimumHeight(120);

        auto* const minFreeSpaceWidget = new QWidget(this);
        auto* const minFreeSpaceLayout = new QVBoxLayout(minFreeSpaceWidget);
        minFreeSpaceLayout->setContentsMargins(0, 0, 0, 0);
        minFreeSpaceLayout->addWidget(mMinFreeSpaceTable);

        auto* const minFreeSpaceButtonsLayout = new QHBoxLayout();
        mAddMinFreeSpaceButton = new QPushButton(QIcon::fromTheme("list-add"_L1), QString(), this);
        mAddMinFreeSpaceButton->setToolTip(qApp->translate("tremotesf", "Add rule"));
        minFreeSpaceButtonsLayout->addWidget(mAddMinFreeSpaceButton);
        mRemoveMinFreeSpaceButton = new QPushButton(QIcon::fromTheme("list-remove"_L1), QString(), this);
        mRemoveMinFreeSpaceButton->setToolTip(qApp->translate("tremotesf", "Remove selected rules"));
        minFreeSpaceButtonsLayout->addWidget(mRemoveMinFreeSpaceButton);
        minFreeSpaceButtonsLayout->addStretch();
        minFreeSpaceLayout->addLayout(minFreeSpaceButtonsLayout);

        QObject::connect(mAddMinFreeSpaceButton, &QPushButton::clicked, this, [this] {
            addMinFreeSpaceRow();
            const int row = mMinFreeSpaceTable->rowCount() - 1;
            mMinFreeSpaceTable->setCurrentCell(row, 0);
            mMinFreeSpaceTable->editItem(mMinFreeSpaceTable->item(row, 0));
        });
        QObject::connect(
            mRemoveMinFreeSpaceButton,
            &QPushButton::clicked,
            this,
            &ServerSettingsDialog::removeSelectedMinFreeSpaceRows
        );
        QObject::connect(mMinFreeSpaceTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
            if (item->column() == 0) {
                updateMinFreeSpaceFreeSpace(item->row());
            }
        });

        downloadingPageLayout->addRow(qApp->translate("tremotesf", "Minimum free space:"), minFreeSpaceWidget);

        mMinFreeSpaceResumeMarginEdit = new QLineEdit(this);
        mMinFreeSpaceResumeMarginEdit->setToolTip(
            qApp->translate("tremotesf", "Extra free space required above the limit before resuming torrents")
        );
        downloadingPageLayout->addRow(
            qApp->translate("tremotesf", "Minimum free space resume margin:"),
            mMinFreeSpaceResumeMarginEdit
        );

        mMoveCompletedTorrentsCheckBox = new QCheckBox(qApp->translate("tremotesf", "Move completed torrents"), this);
        downloadingPageLayout->addRow(mMoveCompletedTorrentsCheckBox);

        mMovePartialFilesCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Move partial files when relocating"), this);
        downloadingPageLayout->addRow(mMovePartialFilesCheckBox);

        mForceFinishPartialFilesCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Force finish partial files"), this);
        downloadingPageLayout->addRow(mForceFinishPartialFilesCheckBox);

        // Seeding page
        mSeedingPageWidget = new QWidget(this);
        KPageWidgetItem* seedingPageItem = mPageWidget->addPage(
            mSeedingPageWidget,
            //: "Seeding" server setting page
            qApp->translate("tremotesf", "Seeding", "Noun")
        );
        seedingPageItem->setIcon(QIcon::fromTheme("network-server"_L1));

        auto seedingPageLayout = new QGridLayout(mSeedingPageWidget);

        mRatioLimitCheckBox = new QCheckBox(qApp->translate("tremotesf", "Stop seeding at ratio:"), this);
        seedingPageLayout->addWidget(mRatioLimitCheckBox, 0, 0, 1, 2, Qt::AlignTop);

        mRatioLimitSpinBox = new QDoubleSpinBox(this);
        mRatioLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mRatioLimitSpinBox, nonNegativeDecimalsRange);
        QObject::connect(mRatioLimitCheckBox, &QCheckBox::toggled, mRatioLimitSpinBox, &QSpinBox::setEnabled);
        seedingPageLayout->addWidget(mRatioLimitSpinBox, 1, 1, Qt::AlignTop);

        mIdleSeedingLimitCheckBox = new QCheckBox(qApp->translate("tremotesf", "Stop seeding if idle for:"), this);
        seedingPageLayout->addWidget(mIdleSeedingLimitCheckBox, 2, 0, 1, 2, Qt::AlignTop);

        mIdleSeedingLimitSpinBox = new QSpinBox(this);
        mIdleSeedingLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mIdleSeedingLimitSpinBox, minutesRange);
        //: Suffix that is added to input field with number of minuts, e.g. "5 min"
        mIdleSeedingLimitSpinBox->setSuffix(qApp->translate("tremotesf", " min"));
        QObject::connect(
            mIdleSeedingLimitCheckBox,
            &QCheckBox::toggled,
            mIdleSeedingLimitSpinBox,
            &QSpinBox::setEnabled
        );
        seedingPageLayout->addWidget(mIdleSeedingLimitSpinBox, 3, 1, Qt::AlignTop);

        mScriptTorrentDoneRemoveTorrentCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Done script removes torrent"), this);
        seedingPageLayout->addWidget(mScriptTorrentDoneRemoveTorrentCheckBox, 4, 0, 1, 2, Qt::AlignTop);

        mScriptTorrentDoneRemoveTempFilesCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Done script removes temporary files"), this);
        seedingPageLayout->addWidget(mScriptTorrentDoneRemoveTempFilesCheckBox, 5, 0, 1, 2, Qt::AlignTop);

        mScriptTorrentDoneRemoveAllFilesCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Done script removes all files"), this);
        seedingPageLayout->addWidget(mScriptTorrentDoneRemoveAllFilesCheckBox, 6, 0, 1, 2, Qt::AlignTop);

        seedingPageLayout->setRowStretch(6, 1);
        seedingPageLayout->setColumnStretch(1, 1);
        seedingPageLayout->setColumnMinimumWidth(0, 28 - seedingPageLayout->spacing());

        // Queue page
        mQueuePageWidget = new QWidget(this);
        KPageWidgetItem* queuePageItem = mPageWidget->addPage(
            mQueuePageWidget,
            //: "Queue" server settings page
            qApp->translate("tremotesf", "Queue")
        );
        queuePageItem->setIcon(QIcon::fromTheme("tremotesf-queue"_L1));

        auto queuePageLayout = new QGridLayout(mQueuePageWidget);

        mMaximumActiveDownloadsCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Maximum active downloads:"), this);
        queuePageLayout->addWidget(mMaximumActiveDownloadsCheckBox, 0, 0, 1, 2, Qt::AlignTop);

        mMaximumActiveDownloadsSpinBox = new QSpinBox(this);
        mMaximumActiveDownloadsSpinBox->setEnabled(false);
        setSpinBoxLimits(mMaximumActiveDownloadsSpinBox, nonNegativeIntegersRange);
        QObject::connect(
            mMaximumActiveDownloadsCheckBox,
            &QCheckBox::toggled,
            mMaximumActiveDownloadsSpinBox,
            &QSpinBox::setEnabled
        );
        queuePageLayout->addWidget(mMaximumActiveDownloadsSpinBox, 1, 1, Qt::AlignTop);

        mMaximumActiveUploadsCheckBox = new QCheckBox(qApp->translate("tremotesf", "Maximum active uploads:"), this);
        queuePageLayout->addWidget(mMaximumActiveUploadsCheckBox, 2, 0, 1, 2, Qt::AlignTop);

        mMaximumActiveUploadsSpinBox = new QSpinBox(this);
        mMaximumActiveUploadsSpinBox->setEnabled(false);
        setSpinBoxLimits(mMaximumActiveUploadsSpinBox, nonNegativeIntegersRange);
        QObject::connect(
            mMaximumActiveUploadsCheckBox,
            &QCheckBox::toggled,
            mMaximumActiveUploadsSpinBox,
            &QSpinBox::setEnabled
        );
        queuePageLayout->addWidget(mMaximumActiveUploadsSpinBox, 3, 1, Qt::AlignTop);

        mIdleQueueLimitCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Ignore queue position if idle for:"), this);
        queuePageLayout->addWidget(mIdleQueueLimitCheckBox, 4, 0, 1, 2, Qt::AlignTop);

        mIdleQueueLimitSpinBox = new QSpinBox(this);
        mIdleQueueLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mIdleQueueLimitSpinBox, minutesRange);
        //: Suffix that is added to input field with number of minuts, e.g. "5 min"
        mIdleQueueLimitSpinBox->setSuffix(qApp->translate("tremotesf", " min"));
        QObject::connect(mIdleQueueLimitCheckBox, &QCheckBox::toggled, mIdleQueueLimitSpinBox, &QSpinBox::setEnabled);
        queuePageLayout->addWidget(mIdleQueueLimitSpinBox, 5, 1, Qt::AlignTop);

        mAutoPriorityCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Automatically prioritize torrents"), this);
        queuePageLayout->addWidget(mAutoPriorityCheckBox, 6, 0, 1, 2, Qt::AlignTop);

        queuePageLayout->addWidget(
            new QLabel(qApp->translate("tremotesf", "Maximum files with high priority per torrent:"), this),
            7,
            0,
            Qt::AlignTop
        );

        mAutoPriorityMaxHighSpinBox = new QSpinBox(this);
        mAutoPriorityMaxHighSpinBox->setEnabled(false);
        setSpinBoxLimits(mAutoPriorityMaxHighSpinBox, nonNegativeIntegersRange);
        QObject::connect(
            mAutoPriorityCheckBox,
            &QCheckBox::toggled,
            mAutoPriorityMaxHighSpinBox,
            &QSpinBox::setEnabled
        );
        queuePageLayout->addWidget(mAutoPriorityMaxHighSpinBox, 7, 1, Qt::AlignTop);

        queuePageLayout->addWidget(new QLabel(qApp->translate("tremotesf", "Sort torrents by:"), this), 8, 0, Qt::AlignTop);

        mAutoPrioritySortComboBox = new QComboBox(this);
        mAutoPrioritySortComboBox->addItem(
            qApp->translate("tremotesf", "Id"),
            QVariant::fromValue(ServerSettingsData::AutoPrioritySort::Id)
        );
        mAutoPrioritySortComboBox->addItem(
            qApp->translate("tremotesf", "Name"),
            QVariant::fromValue(ServerSettingsData::AutoPrioritySort::Name)
        );
        mAutoPrioritySortComboBox->addItem(
            qApp->translate("tremotesf", "Size"),
            QVariant::fromValue(ServerSettingsData::AutoPrioritySort::Size)
        );
        queuePageLayout->addWidget(mAutoPrioritySortComboBox, 8, 1, Qt::AlignTop);

        mQueueMoveToEndCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Move inactive downloads to the end of the queue:"), this);
        queuePageLayout->addWidget(mQueueMoveToEndCheckBox, 9, 0, 1, 2, Qt::AlignTop);

        mQueueMoveToEndMinutesSpinBox = new QSpinBox(this);
        mQueueMoveToEndMinutesSpinBox->setEnabled(false);
        setSpinBoxLimits(mQueueMoveToEndMinutesSpinBox, minutesRange);
        //: Suffix that is added to input field with number of minutes, e.g. "5 min"
        mQueueMoveToEndMinutesSpinBox->setSuffix(qApp->translate("tremotesf", " min"));
        QObject::connect(
            mQueueMoveToEndCheckBox,
            &QCheckBox::toggled,
            mQueueMoveToEndMinutesSpinBox,
            &QSpinBox::setEnabled
        );
        queuePageLayout->addWidget(mQueueMoveToEndMinutesSpinBox, 10, 1, Qt::AlignTop);

        queuePageLayout->setRowStretch(10, 1);
        queuePageLayout->setColumnStretch(1, 1);
        queuePageLayout->setColumnMinimumWidth(0, 28 - queuePageLayout->spacing());

        // Speed page
        mSpeedPageWidget = new QWidget(this);
        KPageWidgetItem* speedPageItem = mPageWidget->addPage(
            mSpeedPageWidget,
            //: "Speed" server settings page
            qApp->translate("tremotesf", "Speed")
        );
        speedPageItem->setIcon(QIcon::fromTheme("preferences-system-time"_L1));

        auto speedPageLayout = new QVBoxLayout(mSpeedPageWidget);

        //: Speed limits section
        auto speedLimitsGroupBox = new QGroupBox(qApp->translate("tremotesf", "Limits"), this);
        auto speedLimitsGroupBoxLayout = new QGridLayout(speedLimitsGroupBox);

        //: Suffix that is added to input field with download/upload speed limit, e.g. "5000 kB/s". 'k' prefix means SI prefix, i.e kB = 1000 bytes
        const QString suffix(qApp->translate("tremotesf", " kB/s"));

        //: Download speed limit input field label
        mDownloadSpeedLimitCheckBox = new QCheckBox(qApp->translate("tremotesf", "Download:"), this);
        speedLimitsGroupBoxLayout->addWidget(mDownloadSpeedLimitCheckBox, 0, 0, 1, 2);

        mDownloadSpeedLimitSpinBox = new QSpinBox(this);
        mDownloadSpeedLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mDownloadSpeedLimitSpinBox, speedLimitKbpsRange);
        mDownloadSpeedLimitSpinBox->setSuffix(suffix);
        QObject::connect(
            mDownloadSpeedLimitCheckBox,
            &QCheckBox::toggled,
            mDownloadSpeedLimitSpinBox,
            &QSpinBox::setEnabled
        );
        speedLimitsGroupBoxLayout->addWidget(mDownloadSpeedLimitSpinBox, 1, 1);

        //: Upload speed limit input field label
        mUploadSpeedLimitCheckBox = new QCheckBox(qApp->translate("tremotesf", "Upload:"), this);
        speedLimitsGroupBoxLayout->addWidget(mUploadSpeedLimitCheckBox, 2, 0, 1, 2);

        mUploadSpeedLimitSpinBox = new QSpinBox(this);
        mUploadSpeedLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mUploadSpeedLimitSpinBox, speedLimitKbpsRange);
        mUploadSpeedLimitSpinBox->setSuffix(suffix);
        QObject::connect(
            mUploadSpeedLimitCheckBox,
            &QCheckBox::toggled,
            mUploadSpeedLimitSpinBox,
            &QSpinBox::setEnabled
        );
        speedLimitsGroupBoxLayout->addWidget(mUploadSpeedLimitSpinBox, 3, 1);

        speedLimitsGroupBoxLayout->setColumnMinimumWidth(0, 28 - speedLimitsGroupBoxLayout->spacing());

        speedPageLayout->addWidget(speedLimitsGroupBox);

        //: Alternative speed limits section
        auto alternativeSpeedLimitsGroupBox = new QGroupBox(qApp->translate("tremotesf", "Alternative Limits"), this);
        auto alternativeSpeedLimitsGroupBoxLayout = new QVBoxLayout(alternativeSpeedLimitsGroupBox);

        //: Check box label
        mEnableAlternativeSpeedLimitsGroupBox = new QGroupBox(qApp->translate("tremotesf", "Enable"), this);
        mEnableAlternativeSpeedLimitsGroupBox->setCheckable(true);
        auto enableAlternativeLimitsGroupBoxLayout = new QFormLayout(mEnableAlternativeSpeedLimitsGroupBox);
        enableAlternativeLimitsGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mAlternativeDownloadSpeedLimitSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mAlternativeDownloadSpeedLimitSpinBox, speedLimitKbpsRange);
        mAlternativeDownloadSpeedLimitSpinBox->setSuffix(suffix);
        enableAlternativeLimitsGroupBoxLayout->addRow(
            //: Download speed limit input field label
            qApp->translate("tremotesf", "Download:"),
            mAlternativeDownloadSpeedLimitSpinBox
        );
        mAlternativeUploadSpeedLimitSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mAlternativeUploadSpeedLimitSpinBox, speedLimitKbpsRange);
        mAlternativeUploadSpeedLimitSpinBox->setSuffix(suffix);
        enableAlternativeLimitsGroupBoxLayout->addRow(
            //: Upload speed limit input field label
            qApp->translate("tremotesf", "Upload:"),
            mAlternativeUploadSpeedLimitSpinBox
        );

        alternativeSpeedLimitsGroupBoxLayout->addWidget(mEnableAlternativeSpeedLimitsGroupBox);

        //: Title of alternative speed limit scheduling section
        mLimitScheduleGroupBox = new QGroupBox(qApp->translate("tremotesf", "Scheduled"), this);
        mLimitScheduleGroupBox->setCheckable(true);
        auto scheduleGroupBoxLayout = new QFormLayout(mLimitScheduleGroupBox);
        scheduleGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        auto scheduleTimeLayout = new QHBoxLayout();
        scheduleGroupBoxLayout->addRow(scheduleTimeLayout);
        mLimitScheduleBeginTimeEdit = new QTimeEdit(this);
        scheduleTimeLayout->addWidget(mLimitScheduleBeginTimeEdit, 1);
        //: Separates time range input fields. E.g. "to" inside "1:00 AM to 5:00 AM"
        scheduleTimeLayout->addWidget(new QLabel(qApp->translate("tremotesf", "to")));
        mLimitScheduleEndTimeEdit = new QTimeEdit(this);
        scheduleTimeLayout->addWidget(mLimitScheduleEndTimeEdit, 1);

        mLimitScheduleDaysComboBox = new QComboBox(this);
        mLimitScheduleDaysComboBox->addItem(
            qApp->translate("tremotesf", "Every day"),
            QVariant::fromValue(ServerSettingsData::AlternativeSpeedLimitsDays::All)
        );
        mLimitScheduleDaysComboBox->addItem(
            qApp->translate("tremotesf", "Weekdays"),
            QVariant::fromValue(ServerSettingsData::AlternativeSpeedLimitsDays::Weekdays)
        );
        mLimitScheduleDaysComboBox->addItem(
            qApp->translate("tremotesf", "Weekends"),
            QVariant::fromValue(ServerSettingsData::AlternativeSpeedLimitsDays::Weekends)
        );
        mLimitScheduleDaysComboBox->insertSeparator(mLimitScheduleDaysComboBox->count());
        {
            auto nextDay = [](Qt::DayOfWeek day) {
                if (day == Qt::Sunday) {
                    return Qt::Monday;
                }
                return static_cast<Qt::DayOfWeek>(day + 1);
            };

            auto daysFromQtDay = [](Qt::DayOfWeek day) {
                switch (day) {
                case Qt::Monday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Monday;
                case Qt::Tuesday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Tuesday;
                case Qt::Wednesday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Wednesday;
                case Qt::Thursday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Thursday;
                case Qt::Friday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Friday;
                case Qt::Saturday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Saturday;
                case Qt::Sunday:
                    return ServerSettingsData::AlternativeSpeedLimitsDays::Sunday;
                }
                return ServerSettingsData::AlternativeSpeedLimitsDays::All;
            };

            const QLocale locale;

            const Qt::DayOfWeek first = QLocale().firstDayOfWeek();
            mLimitScheduleDaysComboBox->addItem(locale.dayName(first), QVariant::fromValue(daysFromQtDay(first)));

            for (Qt::DayOfWeek day = nextDay(first); day != first; day = nextDay(day)) {
                mLimitScheduleDaysComboBox->addItem(locale.dayName(day), QVariant::fromValue(daysFromQtDay(day)));
            }
        }

        scheduleGroupBoxLayout->addRow(qApp->translate("tremotesf", "Days:"), mLimitScheduleDaysComboBox);

        alternativeSpeedLimitsGroupBoxLayout->addWidget(mLimitScheduleGroupBox);

        auto alternativeSpeedLimitsResizer = new KColumnResizer(this);
        alternativeSpeedLimitsResizer->addWidgetsFromLayout(enableAlternativeLimitsGroupBoxLayout);
        alternativeSpeedLimitsResizer->addWidgetsFromLayout(scheduleGroupBoxLayout);

        speedPageLayout->addWidget(alternativeSpeedLimitsGroupBox);

        speedPageLayout->addStretch();

        // Network page
        mNetworkPageWidget = new QWidget(this);
        KPageWidgetItem* networkPageItem = mPageWidget->addPage(
            mNetworkPageWidget,
            //: "Network" server settings page
            qApp->translate("tremotesf", "Network")
        );
        networkPageItem->setIcon(QIcon::fromTheme("preferences-system-network"_L1));

        auto networkPageLayout = new QVBoxLayout(mNetworkPageWidget);

        //: Title of settings section related to peer connections
        auto connectionGroupBox = new QGroupBox(qApp->translate("tremotesf", "Connection"), this);
        auto connectionGroupBoxLayout = new QFormLayout(connectionGroupBox);
        connectionGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mPeerPortSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mPeerPortSpinBox, portsRange);
        connectionGroupBoxLayout->addRow(qApp->translate("tremotesf", "Peer port:"), mPeerPortSpinBox);

        //: Check box label
        mRandomPortCheckBox = new QCheckBox(qApp->translate("tremotesf", "Random port on Transmission start"), this);
        connectionGroupBoxLayout->addRow(mRandomPortCheckBox);

        //: Check box label
        mPortForwardingCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable port forwarding"));
        connectionGroupBoxLayout->addRow(mPortForwardingCheckBox);

        mEncryptionComboBox = new QComboBox();
        for (const auto mode : encryptionModeComboBoxItems) {
            switch (mode) {
            case ServerSettingsData::EncryptionMode::Allowed:
                //: Encryption mode (allow/prefer/require)
                mEncryptionComboBox->addItem(qApp->translate("tremotesf", "Allow"));
                break;
            case ServerSettingsData::EncryptionMode::Preferred:
                //: Encryption mode (allow/prefer/require)
                mEncryptionComboBox->addItem(qApp->translate("tremotesf", "Prefer"));
                break;
            case ServerSettingsData::EncryptionMode::Required:
                //: Encryption mode (allow/prefer/require)
                mEncryptionComboBox->addItem(qApp->translate("tremotesf", "Require"));
                break;
            }
        }

        connectionGroupBoxLayout->addRow(qApp->translate("tremotesf", "Encryption:"), mEncryptionComboBox);

        //: Check box label
        mUtpCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable μTP (Micro Transport Protocol)"), this);
        connectionGroupBoxLayout->addRow(mUtpCheckBox);

        //: Check box label
        mPexCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable PEX (Peer exchange)"), this);
        connectionGroupBoxLayout->addRow(mPexCheckBox);

        //: Check box label
        mDhtCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable DHT"), this);
        connectionGroupBoxLayout->addRow(mDhtCheckBox);

        //: Check box label
        mLpdCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable local peer discovery"), this);
        connectionGroupBoxLayout->addRow(mLpdCheckBox);

        networkPageLayout->addWidget(connectionGroupBox);

        auto peerLimitsGroupBox = new QGroupBox(qApp->translate("tremotesf", "Peer Limits"), this);
        auto peerLimitsGroupBoxLayout = new QFormLayout(peerLimitsGroupBox);
        peerLimitsGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mTorrentPeerLimitSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mTorrentPeerLimitSpinBox, peersLimitRange);
        peerLimitsGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Maximum peers per torrent:"),
            mTorrentPeerLimitSpinBox
        );

        mGlobalPeerLimitSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mGlobalPeerLimitSpinBox, peersLimitRange);
        peerLimitsGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Maximum peers globally:"),
            mGlobalPeerLimitSpinBox
        );

        networkPageLayout->addWidget(peerLimitsGroupBox);

        networkPageLayout->addStretch();

        auto networkPageResizer = new KColumnResizer(this);
        networkPageResizer->addWidgetsFromLayout(connectionGroupBoxLayout);
        networkPageResizer->addWidgetsFromLayout(peerLimitsGroupBoxLayout);

        layout->addWidget(mPageWidget);

        auto dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, &ServerSettingsDialog::accept);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &ServerSettingsDialog::reject);
        mPageWidget->setPageFooter(dialogButtonBox);
    }

    void ServerSettingsDialog::updateScriptParametersPage() {
        if (!mScriptParametersPage) {
            return;
        }

        const auto& data = mRpc->serverSettings()->data();
        const bool available = mRpc->isConnected() && mScriptParametersPage->isAvailable();

        if (available && !mScriptParametersPageItem) {
            mScriptParametersPageItem =
                mPageWidget->addPage(mScriptParametersPage, qApp->translate("tremotesf", "Script parameters"));
            mScriptParametersPageItem->setIcon(QIcon::fromTheme("system-run"_L1));
            mScriptParametersRevision = data.scriptRunner.has_value() ? data.scriptRunner->revision : QString();
            mScriptParametersPage->load();
        } else if (!available && mScriptParametersPageItem) {
            mPageWidget->removePage(mScriptParametersPageItem);
            mScriptParametersPageItem = nullptr;
            mScriptParametersRevision.clear();
        } else if (available) {
            const auto revision = data.scriptRunner.has_value() ? data.scriptRunner->revision : QString();
            if (revision != mScriptParametersRevision) {
                mScriptParametersRevision = revision;
                mScriptParametersPage->load();
            }
        }
    }

    void ServerSettingsDialog::loadSettings() {
        const ServerSettings* settings = mRpc->serverSettings();

        mDownloadDirectoryWidget->resetPath(settings->data().downloadDirectory);
        mStartAddedTorrentsCheckBox->setChecked(settings->data().startAddedTorrents);
        mSequentialDownloadCheckBox->setChecked(settings->data().sequentialDownload);
        mSequentialDownloadCheckBox->setVisible(settings->data().hasSequentialDownload);
        mIncompleteFilesCheckBox->setChecked(settings->data().renameIncompleteFiles);
        mIncompleteDirectoryCheckBox->setChecked(settings->data().incompleteDirectoryEnabled);
        mIncompleteDirectoryWidget->resetPath(settings->data().incompleteDirectory);

        mRatioLimitCheckBox->setChecked(settings->data().ratioLimited);
        mRatioLimitSpinBox->setValue(settings->data().ratioLimit);
        mIdleSeedingLimitCheckBox->setChecked(settings->data().idleSeedingLimited);
        mIdleSeedingLimitSpinBox->setValue(settings->data().idleSeedingLimit);

        mMaximumActiveDownloadsCheckBox->setChecked(settings->data().downloadQueueEnabled);
        mMaximumActiveDownloadsSpinBox->setValue(settings->data().downloadQueueSize);
        mMaximumActiveUploadsCheckBox->setChecked(settings->data().seedQueueEnabled);
        mMaximumActiveUploadsSpinBox->setValue(settings->data().seedQueueSize);
        mIdleQueueLimitCheckBox->setChecked(settings->data().idleQueueLimited);
        mIdleQueueLimitSpinBox->setValue(settings->data().idleQueueLimit);

        mAutoPriorityCheckBox->setChecked(settings->data().autoPriorityEnabled);
        mAutoPriorityMaxHighSpinBox->setValue(settings->data().autoPriorityMaxHigh);
        for (int i = 0, max = mAutoPrioritySortComboBox->count(); i < max; i++) {
            if (mAutoPrioritySortComboBox->itemData(i).value<ServerSettingsData::AutoPrioritySort>()
                == settings->data().autoPrioritySort) {
                mAutoPrioritySortComboBox->setCurrentIndex(i);
                break;
            }
        }
        mQueueMoveToEndCheckBox->setChecked(settings->data().queueMoveToEndEnabled);
        mQueueMoveToEndMinutesSpinBox->setValue(settings->data().queueMoveToEndMinutes);

        {
            const QSignalBlocker blocker(mMinFreeSpaceTable);
            mMinFreeSpaceTable->setRowCount(0);
            for (const auto& rule : settings->data().minFreeSpace) {
                const auto separator = rule.indexOf('=');
                if (separator > 0) {
                    const auto path = rule.left(separator).trimmed();
                    if (const auto size = parseSize(rule.mid(separator + 1)); size) {
                        addMinFreeSpaceRow(
                            toNativeSeparators(normalizePath(path, settings->data().pathOs), settings->data().pathOs),
                            size->value,
                            size->unit
                        );
                    } else {
                        addMinFreeSpaceRow(rule);
                    }
                } else {
                    addMinFreeSpaceRow(rule);
                }
            }
        }
        for (int row = 0, count = mMinFreeSpaceTable->rowCount(); row < count; ++row) {
            updateMinFreeSpaceFreeSpace(row);
        }
        mMinFreeSpaceResumeMarginEdit->setText(settings->data().minFreeSpaceResumeMargin);
        mMoveCompletedTorrentsCheckBox->setChecked(settings->data().moveCompletedTorrentsEnabled);
        mMovePartialFilesCheckBox->setChecked(settings->data().movePartialFilesEnabled);
        mForceFinishPartialFilesCheckBox->setChecked(settings->data().forceFinishPartialFilesEnabled);
        mScriptTorrentDoneRemoveTorrentCheckBox->setChecked(settings->data().scriptTorrentDoneRemoveTorrentEnabled);
        mScriptTorrentDoneRemoveTempFilesCheckBox->setChecked(settings->data().scriptTorrentDoneRemoveTempFilesEnabled);
        mScriptTorrentDoneRemoveAllFilesCheckBox->setChecked(settings->data().scriptTorrentDoneRemoveAllFilesEnabled);

        mDownloadSpeedLimitCheckBox->setChecked(settings->data().downloadSpeedLimited);
        mDownloadSpeedLimitSpinBox->setValue(settings->data().downloadSpeedLimit);
        mUploadSpeedLimitCheckBox->setChecked(settings->data().uploadSpeedLimited);
        mUploadSpeedLimitSpinBox->setValue(settings->data().uploadSpeedLimit);
        mEnableAlternativeSpeedLimitsGroupBox->setChecked(settings->data().alternativeSpeedLimitsEnabled);
        mAlternativeDownloadSpeedLimitSpinBox->setValue(settings->data().alternativeDownloadSpeedLimit);
        mAlternativeUploadSpeedLimitSpinBox->setValue(settings->data().alternativeUploadSpeedLimit);
        mLimitScheduleGroupBox->setChecked(settings->data().alternativeSpeedLimitsScheduled);
        mLimitScheduleBeginTimeEdit->setTime(settings->data().alternativeSpeedLimitsBeginTime);
        mLimitScheduleEndTimeEdit->setTime(settings->data().alternativeSpeedLimitsEndTime);

        const auto days = settings->data().alternativeSpeedLimitsDays;
        for (int i = 0, max = mLimitScheduleDaysComboBox->count(); i < max; i++) {
            if (mLimitScheduleDaysComboBox->itemData(i).value<ServerSettingsData::AlternativeSpeedLimitsDays>()
                == days) {
                mLimitScheduleDaysComboBox->setCurrentIndex(i);
                break;
            }
        }

        mPeerPortSpinBox->setValue(settings->data().peerPort);
        mRandomPortCheckBox->setChecked(settings->data().randomPortEnabled);
        mPortForwardingCheckBox->setChecked(settings->data().portForwardingEnabled);
        mEncryptionComboBox->setCurrentIndex(
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            indexOfCasted<int>(encryptionModeComboBoxItems, settings->data().encryptionMode).value()
        );
        mUtpCheckBox->setChecked(settings->data().utpEnabled);
        mPexCheckBox->setChecked(settings->data().pexEnabled);
        mDhtCheckBox->setChecked(settings->data().dhtEnabled);
        mLpdCheckBox->setChecked(settings->data().lpdEnabled);
        mTorrentPeerLimitSpinBox->setValue(settings->data().maximumPeersPerTorrent);
        mGlobalPeerLimitSpinBox->setValue(settings->data().maximumPeersGlobally);
    }
}
