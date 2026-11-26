// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
// SPDX-FileCopyrightText: 2021 LuK1337
// SPDX-FileCopyrightText: 2022 Alex <tabell@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include <array>
#include <concepts>
#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSslError>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <KMessageWidget>
#include <KPageWidget>

#include "stdutils.h"
#include "target_os.h"
#include "settings.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "ui/screens/addtorrent/addtorrentdialog.h"
#include "ui/screens/addtorrent/addtorrenthelpers.h"
#include "ui/whisparr/whisparrutils.h"
#include "ui/widgets/torrentremotedirectoryselectionwidget.h"

#ifdef Q_OS_WIN
#    include "ui/systemcolorsprovider.h"
#endif

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
#ifdef Q_OS_WIN
        constexpr std::array darkThemeComboBoxValues{
            Settings::DarkThemeMode::FollowSystem, Settings::DarkThemeMode::On, Settings::DarkThemeMode::Off
        };
#endif

        constexpr std::array torrentDoubleClickActionComboBoxValues{
            Settings::TorrentDoubleClickAction::OpenPropertiesDialog,
            Settings::TorrentDoubleClickAction::OpenTorrentFile,
            Settings::TorrentDoubleClickAction::OpenDownloadDirectory
        };

        std::invocable auto createGeneralPage(KPageWidget* pageWidget, Settings* settings) {
            auto page = new QWidget();
            //: Options tab
            pageWidget->addPage(page, qApp->translate("tremotesf", "General"))
                ->setIcon(QIcon::fromTheme("preferences-desktop"));
            auto layout = new QFormLayout(page);
            layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

#ifdef Q_OS_WIN
            auto darkThemeComboBox = new QComboBox(pageWidget);

            //: Dark theme mode
            darkThemeComboBox->addItem(qApp->translate("tremotesf", "Follow system"));
            //: Dark theme mode
            darkThemeComboBox->addItem(qApp->translate("tremotesf", "On"));
            //: Dark theme mode
            darkThemeComboBox->addItem(qApp->translate("tremotesf", "Off"));
            layout->addRow(qApp->translate("tremotesf", "Dark theme"), darkThemeComboBox);

            auto systemAccentColorCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Use system accent color"),
                page
            );
            layout->addRow(systemAccentColorCheckBox);

            darkThemeComboBox->setCurrentIndex(
                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                indexOfCasted<int>(darkThemeComboBoxValues, settings->get_darkThemeMode()).value()
            );
            systemAccentColorCheckBox->setChecked(settings->get_useSystemAccentColor());
#endif

            auto showTorrentPropertiesInMainWindowCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Show torrent properties in a panel in the main window"),
                page
            );
            layout->addRow(showTorrentPropertiesInMainWindowCheckBox);
            showTorrentPropertiesInMainWindowCheckBox->setChecked(settings->get_showTorrentPropertiesInMainWindow());

            auto torrentDoubleClickActionComboBox = new QComboBox(page);
            layout->addRow(
                qApp->translate("tremotesf", "What to do when torrent in the list is double clicked:"),
                torrentDoubleClickActionComboBox
            );
            for (const auto action : torrentDoubleClickActionComboBoxValues) {
                switch (action) {
                case Settings::TorrentDoubleClickAction::OpenPropertiesDialog:
                    torrentDoubleClickActionComboBox->addItem(qApp->translate("tremotesf", "Open properties dialog"));
                    break;
                case Settings::TorrentDoubleClickAction::OpenTorrentFile:
                    torrentDoubleClickActionComboBox->addItem(qApp->translate("tremotesf", "Open torrent's file"));
                    break;
                case Settings::TorrentDoubleClickAction::OpenDownloadDirectory:
                    torrentDoubleClickActionComboBox->addItem(qApp->translate("tremotesf", "Open download directory"));
                    break;
                default:
                    break;
                }
            }

            torrentDoubleClickActionComboBox->setCurrentIndex(
                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                indexOfCasted<int>(torrentDoubleClickActionComboBoxValues, settings->get_torrentDoubleClickAction())
                    .value()
            );

            auto dialogWarningMessage = new KMessageWidget(page);
            layout->addRow(dialogWarningMessage);
            dialogWarningMessage->setMessageType(KMessageWidget::Warning);
            dialogWarningMessage->setCloseButtonVisible(false);
            dialogWarningMessage->setText(qApp->translate(
                "tremotesf",
                "Properties dialog won't be shown because torrent properties are shown in the main window"
            ));
            const auto updateDialogWarningMessage = [=] {
                if (showTorrentPropertiesInMainWindowCheckBox->isChecked()
                    && torrentDoubleClickActionComboBox->currentIndex()
                           == indexOfCasted<int>(
                               torrentDoubleClickActionComboBoxValues,
                               Settings::TorrentDoubleClickAction::OpenPropertiesDialog
                           )) {
                    dialogWarningMessage->animatedShow();
                } else {
                    dialogWarningMessage->animatedHide();
                }
            };
            updateDialogWarningMessage();
            QObject::connect(
                showTorrentPropertiesInMainWindowCheckBox,
                &QCheckBox::toggled,
                dialogWarningMessage,
                updateDialogWarningMessage
            );
            QObject::connect(
                torrentDoubleClickActionComboBox,
                &QComboBox::currentIndexChanged,
                dialogWarningMessage,
                updateDialogWarningMessage
            );

            auto connectOnStartupCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Connect to server on startup"),
                page
            );
            layout->addRow(connectOnStartupCheckBox);
            connectOnStartupCheckBox->setChecked(settings->get_connectOnStartup());

            auto displayRelativeTimeCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Display relative time"),
                page
            );
            layout->addRow(displayRelativeTimeCheckBox);
            displayRelativeTimeCheckBox->setChecked(settings->get_displayRelativeTime());

            auto languageComboBox = new QComboBox(page);
            const std::array languageCodes{
                u""_s,
                u"en"_s,
                u"ru"_s,
                u"de"_s,
                u"es"_s,
                u"es_ES"_s,
                u"fr"_s,
                u"it_IT"_s,
                u"nl"_s,
                u"nl_BE"_s,
                u"pl"_s,
                u"tr"_s,
                u"zh_CN"_s
            };
            for (const auto& code : languageCodes) {
                if (code.isEmpty()) {
                    languageComboBox->addItem(qApp->translate("tremotesf", "System default"), QString());
                } else {
                    const QLocale locale(code);
                    auto name = locale.nativeLanguageName();
                    if (name.isEmpty()) {
                        name = QLocale::languageToString(locale.language());
                    }
                    languageComboBox->addItem(name, code);
                }
            }
            const auto currentLanguageIndex = languageComboBox->findData(settings->get_language());
            if (currentLanguageIndex >= 0) {
                languageComboBox->setCurrentIndex(currentLanguageIndex);
            }
            layout->addRow(qApp->translate("tremotesf", "Language:"), languageComboBox);

            auto languageRestartMessage = new KMessageWidget(page);
            languageRestartMessage->setMessageType(KMessageWidget::Information);
            languageRestartMessage->setCloseButtonVisible(false);
            languageRestartMessage->setText(
                qApp->translate("tremotesf", "Restart Tremotesf to apply the language change")
            );
            languageRestartMessage->setVisible(false);
            layout->addRow(languageRestartMessage);
            const auto updateLanguageRestartMessage = [=] {
                languageRestartMessage->setVisible(
                    languageComboBox->currentData().toString() != settings->get_language()
                );
            };
            QObject::connect(
                languageComboBox,
                &QComboBox::currentIndexChanged,
                languageRestartMessage,
                updateLanguageRestartMessage
            );

            auto displayFullDownloadDirectoryPathCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Display full path of download directories in sidebar and torrents list"),
                page
            );
            layout->addRow(displayFullDownloadDirectoryPathCheckBox);
            displayFullDownloadDirectoryPathCheckBox->setChecked(settings->get_displayFullDownloadDirectoryPath());

            auto checkForUpdatesCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Check for updates on startup"),
                page
            );
            layout->addRow(checkForUpdatesCheckBox);
            checkForUpdatesCheckBox->setChecked(settings->get_checkForUpdates());

            return [=] {
#ifdef Q_OS_WIN
                if (const auto index = darkThemeComboBox->currentIndex(); index != -1) {
                    settings->set_darkThemeMode(darkThemeComboBoxValues.at(static_cast<size_t>(index)));
                }
                if (systemAccentColorCheckBox) {
                    settings->set_useSystemAccentColor(systemAccentColorCheckBox->isChecked());
                }
#endif
                settings->set_showTorrentPropertiesInMainWindow(showTorrentPropertiesInMainWindowCheckBox->isChecked());
                if (const auto index = torrentDoubleClickActionComboBox->currentIndex(); index != -1) {
                    settings->set_torrentDoubleClickAction(
                        torrentDoubleClickActionComboBoxValues.at(static_cast<size_t>(index))
                    );
                }
                settings->set_connectOnStartup(connectOnStartupCheckBox->isChecked());
                settings->set_displayRelativeTime(displayRelativeTimeCheckBox->isChecked());
                settings->set_language(languageComboBox->currentData().toString());
                settings->set_displayFullDownloadDirectoryPath(displayFullDownloadDirectoryPathCheckBox->isChecked());
                settings->set_checkForUpdates(checkForUpdatesCheckBox->isChecked());
            };
        }

        std::invocable auto createWhisparrPage(KPageWidget* pageWidget, Settings* settings) {
            auto page = new QWidget();
            pageWidget->addPage(page, qApp->translate("tremotesf", "Whisparr"))
                ->setIcon(QIcon::fromTheme("tremotesf-whisparr"_L1));
            auto layout = new QFormLayout(page);
            layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

            auto urlEdit = new QLineEdit(page);
            urlEdit->setPlaceholderText("http://localhost:6969"_L1);
            layout->addRow(qApp->translate("tremotesf", "URL:"), urlEdit);

            auto apiKeyEdit = new QLineEdit(page);
            apiKeyEdit->setEchoMode(QLineEdit::Password);
            layout->addRow(qApp->translate("tremotesf", "API key:"), apiKeyEdit);

            auto ignoreSslErrorsCheckBox =
                new QCheckBox(qApp->translate("tremotesf", "Allow self-signed certificates (unsafe)"), page);
            layout->addRow(ignoreSslErrorsCheckBox);

            auto testButton = new QPushButton(qApp->translate("tremotesf", "Test connection"), page);
            layout->addRow(testButton);

            auto statusWidget = new KMessageWidget(page);
            statusWidget->setCloseButtonVisible(false);
            statusWidget->setVisible(false);
            layout->addRow(statusWidget);

            urlEdit->setText(settings->get_whisparrUrl());
            apiKeyEdit->setText(settings->get_whisparrApiKey());
            ignoreSslErrorsCheckBox->setChecked(settings->get_whisparrIgnoreSslErrors());

            auto* network = new QNetworkAccessManager(page);
            const auto showStatus = [statusWidget](KMessageWidget::MessageType type, const QString& text) {
                statusWidget->setMessageType(type);
                statusWidget->setText(text);
                statusWidget->animatedShow();
            };

            QObject::connect(testButton, &QPushButton::clicked, page, [=] {
                const auto baseText = urlEdit->text().trimmed();
                const auto apiKey = apiKeyEdit->text().trimmed();
                if (baseText.isEmpty()) {
                    showStatus(KMessageWidget::Warning, qApp->translate("tremotesf", "Enter Whisparr URL"));
                    return;
                }
                if (apiKey.isEmpty()) {
                    showStatus(KMessageWidget::Warning, qApp->translate("tremotesf", "Enter Whisparr API key"));
                    return;
                }

                QUrl url(baseText);
                if (url.scheme() != "http"_L1 && url.scheme() != "https"_L1) {
                    url = QUrl("http://"_L1 + baseText);
                }
                if (!url.isValid() || url.host().isEmpty()) {
                    showStatus(KMessageWidget::Warning, qApp->translate("tremotesf", "Invalid Whisparr URL"));
                    return;
                }
                QString path = url.path();
                while (path.endsWith(u'/')) {
                    path.chop(1);
                }
                if (!path.endsWith("/api/v3/system/status"_L1)) {
                    path += "/api/v3/system/status"_L1;
                }
                url.setPath(path);

                QNetworkRequest request(url);
                request.setRawHeader("X-Api-Key", apiKey.toUtf8());
                request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Tremotesf"));

                testButton->setEnabled(false);
                statusWidget->animatedHide();
                auto* reply = network->get(request);
                auto sslErrors = std::make_shared<QList<QSslError>>();
                QObject::connect(reply, &QNetworkReply::sslErrors, reply, [sslErrors](const QList<QSslError>& errors) {
                    *sslErrors = errors;
                });
                whisparr::applySelfSignedCertificatePolicy(reply, ignoreSslErrorsCheckBox->isChecked());
                auto* timeout = new QTimer(reply);
                timeout->setSingleShot(true);
                timeout->start(10000);
                QObject::connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
                QObject::connect(reply, &QNetworkReply::finished, page, [=] {
                    testButton->setEnabled(true);
                    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (reply->error() == QNetworkReply::OperationCanceledError) {
                        showStatus(
                            KMessageWidget::Warning,
                            qApp->translate("tremotesf", "Connection to Whisparr timed out")
                        );
                    } else if (reply->error() != QNetworkReply::NoError) {
                        QString errorText = reply->errorString();
                        if (!sslErrors->isEmpty()) {
                            errorText += QStringLiteral("\n")
                                         + qApp->translate("tremotesf", "TLS errors:")
                                         + QStringLiteral("\n")
                                         + whisparr::sslErrorsText(*sslErrors);
                        }
                        showStatus(KMessageWidget::Warning, errorText);
                    } else if (httpStatus == 401) {
                        showStatus(KMessageWidget::Warning, qApp->translate("tremotesf", "Invalid Whisparr API key"));
                    } else if (httpStatus != 200) {
                        showStatus(
                            KMessageWidget::Warning,
                            qApp->translate("tremotesf", "Whisparr returned HTTP status %1").arg(httpStatus)
                        );
                    } else {
                        const auto data = reply->readAll();
                        QJsonParseError parseError{};
                        const auto document = QJsonDocument::fromJson(data, &parseError);
                        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                            showStatus(KMessageWidget::Positive, qApp->translate("tremotesf", "Whisparr is available"));
                        } else {
                            const auto object = document.object();
                            const auto appName = object.value("appName"_L1).toString("Whisparr"_L1);
                            const auto version = object.value("version"_L1).toString();
                            showStatus(
                                KMessageWidget::Positive,
                                version.isEmpty()
                                    ? qApp->translate("tremotesf", "Connected to %1").arg(appName)
                                    : qApp->translate("tremotesf", "Connected to %1 %2").arg(appName, version)
                            );
                        }
                    }
                    reply->deleteLater();
                });
            });

            return [=] {
                settings->set_whisparrUrl(urlEdit->text().trimmed());
                settings->set_whisparrApiKey(apiKeyEdit->text().trimmed());
                settings->set_whisparrIgnoreSslErrors(ignoreSslErrorsCheckBox->isChecked());
            };
        }

        std::invocable auto createAddingTorrentsPage(KPageWidget* pageWidget, Settings* settings, Rpc* rpc) {
            auto page = new QWidget();
            //: Options tab
            pageWidget->addPage(page, qApp->translate("tremotesf", "Adding torrents"))
                ->setIcon(QIcon::fromTheme("folder-download"));
            auto layout = new QVBoxLayout(page);
            layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

            auto rememberOpenTorrentDirCheckbox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Remember location of last opened torrent file"),
                page
            );
            layout->addWidget(rememberOpenTorrentDirCheckbox);

            auto rememberAddTorrentParametersCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Remember parameters of last added torrent"),
                page
            );
            layout->addWidget(rememberAddTorrentParametersCheckBox);

            auto addTorrentParametersDisconnectedMessage = new KMessageWidget(
                //: Server connection status
                qApp->translate("tremotesf", "Disconnected"),
                page
            );
            addTorrentParametersDisconnectedMessage->setMessageType(KMessageWidget::Warning);
            addTorrentParametersDisconnectedMessage->setCloseButtonVisible(false);
            layout->addWidget(addTorrentParametersDisconnectedMessage);

            auto addTorrentParametersGroupBox =
                new QGroupBox(qApp->translate("tremotesf", "Add torrent parameters"), page);
            auto addTorrentParametersGroupBoxLayout = new QFormLayout(addTorrentParametersGroupBox);
            addTorrentParametersGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            const auto addTorrentParametersWidgets =
                AddTorrentDialog::createAddTorrentParametersWidgets(true, addTorrentParametersGroupBoxLayout, rpc);
            auto addTorrentParametersResetButton = new QPushButton(qApp->translate("tremotesf", "Reset"), page);
            addTorrentParametersGroupBoxLayout->addRow(addTorrentParametersResetButton);

            layout->addWidget(addTorrentParametersGroupBox);

            QCheckBox* showMainWindowWhenAddingTorrentsCheckBox{};
            // Disabling this option does not work on macOS since the app is always activated when files are opened,
            // which causes us to show main window
            if constexpr (targetOs != TargetOs::UnixMacOS) {
                showMainWindowWhenAddingTorrentsCheckBox = new QCheckBox(
                    //: Check box label
                    qApp->translate("tremotesf", "Show main window when adding torrents"),
                    page
                );
                layout->addWidget(showMainWindowWhenAddingTorrentsCheckBox);
            }

            auto showDialogWhenAddingTorrentsCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Show dialog when adding torrents"),
                page
            );
            layout->addWidget(showDialogWhenAddingTorrentsCheckBox);

            auto fillTorrentLinkFromKeyboardCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Automatically fill link from clipboard when adding torrent link"),
                page
            );
            layout->addWidget(fillTorrentLinkFromKeyboardCheckBox);
            auto pasteTipLabel = new QLabel(
                //: %1 is a key binding, e.g. "Ctrl + C"
                qApp->translate("tremotesf", "Tip: you can also press %1 in main window to add torrents from clipboard")
                    .arg(QKeySequence(QKeySequence::Paste).toString(QKeySequence::NativeText))
            );
            layout->addWidget(pasteTipLabel);

            auto askForMergingTrackersCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Ask for merging trackers when adding existing torrent"),
                page
            );
            layout->addWidget(askForMergingTrackersCheckBox);

            auto mergeTrackersCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Merge trackers when adding existing torrent"),
                page
            );
            layout->addWidget(mergeTrackersCheckBox);
            QObject::connect(
                askForMergingTrackersCheckBox,
                &QCheckBox::toggled,
                mergeTrackersCheckBox,
                [mergeTrackersCheckBox](bool checked) { mergeTrackersCheckBox->setEnabled(!checked); }
            );

            layout->addStretch();

            rememberOpenTorrentDirCheckbox->setChecked(settings->get_rememberOpenTorrentDir());
            rememberAddTorrentParametersCheckBox->setChecked(settings->get_rememberAddTorrentParameters());
            addTorrentParametersDisconnectedMessage->setVisible(!rpc->isConnected());
            addTorrentParametersGroupBox->setEnabled(rpc->isConnected());
            QObject::connect(rpc, &Rpc::connectedChanged, page, [=] {
                const bool connected = rpc->isConnected();
                if (connected) {
                    addTorrentParametersDisconnectedMessage->animatedHide();
                } else {
                    addTorrentParametersDisconnectedMessage->animatedShow();
                }
                addTorrentParametersGroupBox->setEnabled(connected);
                if (connected) {
                    // Update parameters which initial values depend on server state
                    const auto parameters = getAddTorrentParameters(rpc);
                    addTorrentParametersWidgets.downloadDirectoryWidget->resetPath(parameters.downloadDirectory);
                    addTorrentParametersWidgets.startTorrentCheckBox->setChecked(parameters.startAfterAdding);
                    addTorrentParametersWidgets.sequentialDownloadCheckBox->setChecked(parameters.sequentialDownload);
                    if (addTorrentParametersWidgets.metadataOnlyCheckBox) {
                        addTorrentParametersWidgets.metadataOnlyCheckBox->setChecked(parameters.metadataOnly);
                        addTorrentParametersWidgets.metadataOnlyCheckBox->setVisible(
                            rpc->serverSettings()->data().hasForkFeature("metadata_only/v1"_L1)
                        );
                    }
                }
            });
            QObject::connect(addTorrentParametersResetButton, &QPushButton::clicked, page, [=] {
                addTorrentParametersWidgets.reset(rpc);
            });

            if (showMainWindowWhenAddingTorrentsCheckBox) {
                showMainWindowWhenAddingTorrentsCheckBox->setChecked(settings->get_showMainWindowWhenAddingTorrent());
            }
            showDialogWhenAddingTorrentsCheckBox->setChecked(settings->get_showAddTorrentDialog());
            fillTorrentLinkFromKeyboardCheckBox->setChecked(settings->get_fillTorrentLinkFromClipboard());
            askForMergingTrackersCheckBox->setChecked(settings->get_askForMergingTrackersWhenAddingExistingTorrent());
            mergeTrackersCheckBox->setChecked(settings->get_mergeTrackersWhenAddingExistingTorrent());
            mergeTrackersCheckBox->setEnabled(!askForMergingTrackersCheckBox->isChecked());

            return [=] {
                settings->set_rememberOpenTorrentDir(rememberOpenTorrentDirCheckbox->isChecked());
                settings->set_rememberAddTorrentParameters(rememberAddTorrentParametersCheckBox->isChecked());
                addTorrentParametersWidgets.saveToSettings();
                if (showMainWindowWhenAddingTorrentsCheckBox) {
                    settings->set_showMainWindowWhenAddingTorrent(
                        showMainWindowWhenAddingTorrentsCheckBox->isChecked()
                    );
                }
                settings->set_showAddTorrentDialog(showDialogWhenAddingTorrentsCheckBox->isChecked());
                settings->set_fillTorrentLinkFromClipboard(fillTorrentLinkFromKeyboardCheckBox->isChecked());
                settings->set_askForMergingTrackersWhenAddingExistingTorrent(
                    askForMergingTrackersCheckBox->isChecked()
                );
                settings->set_mergeTrackersWhenAddingExistingTorrent(mergeTrackersCheckBox->isChecked());
            };
        }

        std::invocable auto createNotificationsPage(KPageWidget* pageWidget, Settings* settings) {
            auto page = new QWidget();
            //: Options tab
            pageWidget->addPage(page, qApp->translate("tremotesf", "Notifications"))
                ->setIcon(QIcon::fromTheme("preferences-desktop-notification"));
            auto layout = new QVBoxLayout(page);
            layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

            auto notificationOnDisconnectingCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Notify when disconnecting from server"),
                page
            );
            layout->addWidget(notificationOnDisconnectingCheckBox);

            auto notificationOnAddingTorrentCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Notify on added torrents"),
                page
            );
            layout->addWidget(notificationOnAddingTorrentCheckBox);

            auto notificationOfFinishedTorrentsCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Notify on finished torrents"),
                page
            );
            layout->addWidget(notificationOfFinishedTorrentsCheckBox);

            auto trayIconCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Show icon in the notification area"),
                page
            );
            layout->addWidget(trayIconCheckBox);

            auto closeBehaviorComboBox = new QComboBox(page);
            closeBehaviorComboBox->addItem(
                //: Close window behavior
                qApp->translate("tremotesf", "Hide to notification area"),
                QVariant::fromValue(Settings::CloseBehavior::HideToTray)
            );
            closeBehaviorComboBox->addItem(
                //: Close window behavior
                qApp->translate("tremotesf", "Quit application"),
                QVariant::fromValue(Settings::CloseBehavior::Quit)
            );
            //: Options label
            auto closeBehaviorLayout = new QFormLayout();
            closeBehaviorLayout->setContentsMargins(0, 0, 0, 0);
            closeBehaviorLayout->addRow(qApp->translate("tremotesf", "When closing window:"), closeBehaviorComboBox);
            layout->addLayout(closeBehaviorLayout);

            //: Notifications options section
            auto whenConnectingGroupBox =
                new QGroupBox(qApp->translate("tremotesf", "When connecting to server"), page);
            layout->addWidget(whenConnectingGroupBox);
            auto whenConnectingGroupBoxLayout = new QVBoxLayout(whenConnectingGroupBox);
            whenConnectingGroupBoxLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

            auto addedSinceLastConnectionCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Notify on added torrents since last connection to server"),
                whenConnectingGroupBox
            );
            whenConnectingGroupBoxLayout->addWidget(addedSinceLastConnectionCheckBox);

            auto finishedSinceLastConnectionCheckBox = new QCheckBox(
                //: Check box label
                qApp->translate("tremotesf", "Notify on finished torrents since last connection to server"),
                whenConnectingGroupBox
            );
            whenConnectingGroupBoxLayout->addWidget(finishedSinceLastConnectionCheckBox);

            notificationOnDisconnectingCheckBox->setChecked(settings->get_notificationOnDisconnecting());
            notificationOnAddingTorrentCheckBox->setChecked(settings->get_notificationOnAddingTorrent());
            notificationOfFinishedTorrentsCheckBox->setChecked(settings->get_notificationOfFinishedTorrents());
            trayIconCheckBox->setChecked(settings->get_showTrayIcon());
            closeBehaviorComboBox->setCurrentIndex(
                closeBehaviorComboBox->findData(QVariant::fromValue(settings->get_closeBehavior()))
            );
            addedSinceLastConnectionCheckBox->setChecked(
                settings->get_notificationsOnAddedTorrentsSinceLastConnection()
            );
            finishedSinceLastConnectionCheckBox->setChecked(
                settings->get_notificationsOnFinishedTorrentsSinceLastConnection()
            );

            return [=] {
                settings->set_notificationOnDisconnecting(notificationOnDisconnectingCheckBox->isChecked());
                settings->set_notificationOnAddingTorrent(notificationOnAddingTorrentCheckBox->isChecked());
                settings->set_notificationOfFinishedTorrents(notificationOfFinishedTorrentsCheckBox->isChecked());
                settings->set_showTrayIcon(trayIconCheckBox->isChecked());
                settings->set_closeBehavior(closeBehaviorComboBox->currentData().value<Settings::CloseBehavior>());
                settings->set_notificationsOnAddedTorrentsSinceLastConnection(
                    addedSinceLastConnectionCheckBox->isChecked()
                );
                settings->set_notificationsOnFinishedTorrentsSinceLastConnection(
                    finishedSinceLastConnectionCheckBox->isChecked()
                );
            };
        }
    }

    SettingsDialog::SettingsDialog(Rpc* rpc, QWidget* parent) : QDialog(parent) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Options"));

        auto rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);

        auto pageWidget = new KPageWidget(this);
        rootLayout->addWidget(pageWidget);

        auto settings = Settings::instance();

        const auto saveGeneralPage = createGeneralPage(pageWidget, settings);
        const auto saveWhisparrPage = createWhisparrPage(pageWidget, settings);
        const auto saveAddingTorrentsPage = createAddingTorrentsPage(pageWidget, settings, rpc);
        const auto saveNotificationsPage = createNotificationsPage(pageWidget, settings);

        auto dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
        pageWidget->setPageFooter(dialogButtonBox);

        QObject::connect(this, &SettingsDialog::accepted, this, [=] {
            saveGeneralPage();
            saveWhisparrPage();
            saveAddingTorrentsPage();
            saveNotificationsPage();
        });
    }
}
