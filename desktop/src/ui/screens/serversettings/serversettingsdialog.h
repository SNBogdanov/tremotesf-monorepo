// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_SERVERSETTINGSDIALOG_H
#define TREMOTESF_SERVERSETTINGSDIALOG_H

#include <QChar>
#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimeEdit;

class KMessageWidget;
class KPageWidget;
class KPageWidgetItem;

namespace tremotesf {
    class RemoteDirectorySelectionWidget;
    class Rpc;
    class ScriptParametersPage;

    class ServerSettingsDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ServerSettingsDialog(Rpc* rpc, QWidget* parent = nullptr);
        void accept() override;

    private:
        void setupUi();
        void loadSettings();
        void updateScriptParametersPage();

        void addMinFreeSpaceRow(const QString& path = {}, quint64 value = 0, QChar unit = u'G');
        void removeSelectedMinFreeSpaceRows();
        void updateMinFreeSpaceFreeSpace(int row);

        Rpc* mRpc;

        KMessageWidget* mDisconnectedMessageWidget = nullptr;
        KPageWidget* mPageWidget = nullptr;
        ScriptParametersPage* mScriptParametersPage = nullptr;
        KPageWidgetItem* mScriptParametersPageItem = nullptr;
        QString mScriptParametersRevision;

        QWidget* mDownloadingPageWidget = nullptr;
        RemoteDirectorySelectionWidget* mDownloadDirectoryWidget = nullptr;
        QCheckBox* mStartAddedTorrentsCheckBox = nullptr;
        QCheckBox* mSequentialDownloadCheckBox = nullptr;
        QCheckBox* mIncompleteFilesCheckBox = nullptr;
        QCheckBox* mIncompleteDirectoryCheckBox = nullptr;
        RemoteDirectorySelectionWidget* mIncompleteDirectoryWidget = nullptr;

        QWidget* mSeedingPageWidget = nullptr;
        QCheckBox* mRatioLimitCheckBox = nullptr;
        QDoubleSpinBox* mRatioLimitSpinBox = nullptr;
        QCheckBox* mIdleSeedingLimitCheckBox = nullptr;
        QSpinBox* mIdleSeedingLimitSpinBox = nullptr;

        QWidget* mQueuePageWidget = nullptr;
        QCheckBox* mMaximumActiveDownloadsCheckBox = nullptr;
        QSpinBox* mMaximumActiveDownloadsSpinBox = nullptr;
        QCheckBox* mMaximumActiveUploadsCheckBox = nullptr;
        QSpinBox* mMaximumActiveUploadsSpinBox = nullptr;
        QCheckBox* mIdleQueueLimitCheckBox = nullptr;
        QSpinBox* mIdleQueueLimitSpinBox = nullptr;
        QCheckBox* mAutoPriorityCheckBox = nullptr;
        QSpinBox* mAutoPriorityMaxHighSpinBox = nullptr;
        QComboBox* mAutoPrioritySortComboBox = nullptr;
        QCheckBox* mQueueMoveToEndCheckBox = nullptr;
        QSpinBox* mQueueMoveToEndMinutesSpinBox = nullptr;

        QTableWidget* mMinFreeSpaceTable = nullptr;
        QPushButton* mAddMinFreeSpaceButton = nullptr;
        QPushButton* mRemoveMinFreeSpaceButton = nullptr;
        QLineEdit* mMinFreeSpaceResumeMarginEdit = nullptr;
        QCheckBox* mMoveCompletedTorrentsCheckBox = nullptr;
        QCheckBox* mMovePartialFilesCheckBox = nullptr;
        QCheckBox* mForceFinishPartialFilesCheckBox = nullptr;

        QCheckBox* mScriptTorrentDoneRemoveTorrentCheckBox = nullptr;
        QCheckBox* mScriptTorrentDoneRemoveTempFilesCheckBox = nullptr;
        QCheckBox* mScriptTorrentDoneRemoveAllFilesCheckBox = nullptr;

        QWidget* mSpeedPageWidget = nullptr;
        QCheckBox* mDownloadSpeedLimitCheckBox = nullptr;
        QSpinBox* mDownloadSpeedLimitSpinBox = nullptr;
        QCheckBox* mUploadSpeedLimitCheckBox = nullptr;
        QSpinBox* mUploadSpeedLimitSpinBox = nullptr;
        QGroupBox* mEnableAlternativeSpeedLimitsGroupBox = nullptr;
        QSpinBox* mAlternativeDownloadSpeedLimitSpinBox = nullptr;
        QSpinBox* mAlternativeUploadSpeedLimitSpinBox = nullptr;
        QGroupBox* mLimitScheduleGroupBox = nullptr;
        QTimeEdit* mLimitScheduleBeginTimeEdit = nullptr;
        QTimeEdit* mLimitScheduleEndTimeEdit = nullptr;
        QComboBox* mLimitScheduleDaysComboBox = nullptr;

        QWidget* mNetworkPageWidget = nullptr;
        QSpinBox* mPeerPortSpinBox = nullptr;
        QCheckBox* mRandomPortCheckBox = nullptr;
        QCheckBox* mPortForwardingCheckBox = nullptr;
        QComboBox* mEncryptionComboBox = nullptr;
        QCheckBox* mUtpCheckBox = nullptr;
        QCheckBox* mPexCheckBox = nullptr;
        QCheckBox* mDhtCheckBox = nullptr;
        QCheckBox* mLpdCheckBox = nullptr;
        QSpinBox* mTorrentPeerLimitSpinBox = nullptr;
        QSpinBox* mGlobalPeerLimitSpinBox = nullptr;
    };
}

#endif // TREMOTESF_SERVERSETTINGSDIALOG_H
