// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivesavedialog.h"

#include <algorithm>
#include <memory>
#include <ranges>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSslError>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "archiveutils.h"
#include "rpc/rpc.h"
#include "rpc/torrent.h"
#include "settings.h"
#include "ui/whisparr/whisparrutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QVector<QPair<QString, QVector<int>>>
        groupByArchive(const QVector<QPointer<Torrent>>& torrents, const QStringList& archiveNames) {
            QVector<QPair<QString, QVector<int>>> groups{};
            for (qsizetype i = 0; i < torrents.size(); ++i) {
                const auto* torrent = torrents[i].data();
                if (!torrent) {
                    continue;
                }
                const auto& name = archiveNames[i];
                const auto found =
                    std::ranges::find_if(groups, [&name](const auto& group) { return group.first == name; });
                if (found == groups.end()) {
                    groups.push_back({name, {torrent->data().id}});
                } else {
                    found->second.push_back(torrent->data().id);
                }
            }
            return groups;
        }

        QString sanitizeArchiveName(QString name) {
            static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
            for (auto& character : name) {
                if (character.unicode() < 32 || forbidden.contains(character)) {
                    character = u' ';
                }
            }
            name = name.simplified();
            if (name.size() > 128) {
                name.truncate(128);
            }
            return name;
        }
    }

    ArchiveSaveDialog::ArchiveSaveDialog(Rpc* rpc, QVector<Torrent*> torrents, QWidget* parent)
        : QDialog(parent), mRpc(rpc), mNetwork(new QNetworkAccessManager(this)) {
        for (auto* torrent : torrents) {
            if (torrent) {
                mTorrents.push_back(torrent);
                mTorrentIds.push_back(torrent->data().id);
            }
        }

        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Save to Archive"));

        auto* const layout = new QVBoxLayout(this);

        auto* const formLayout = new QFormLayout();
        mArchiveComboBox = new QComboBox(this);
        mArchiveComboBox->setEditable(true);
        mArchiveComboBox->setInsertPolicy(QComboBox::NoInsert);
        mArchiveComboBox->setMaxVisibleItems(20);
        //: Archive manager dialog label
        formLayout->addRow(qApp->translate("tremotesf", "Archive:"), mArchiveComboBox);
        layout->addLayout(formLayout);

        const auto* settings = Settings::instance();
        mWhisparrConfigured =
            !settings->get_whisparrUrl().trimmed().isEmpty() && !settings->get_whisparrApiKey().trimmed().isEmpty();

        mUseWhisparrCheckBox = new QCheckBox(qApp->translate("tremotesf", "Use Whisparr to determine episode"), this);
        mUseWhisparrCheckBox->setVisible(mWhisparrConfigured);
        layout->addWidget(mUseWhisparrCheckBox);
        QObject::connect(mUseWhisparrCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            mArchiveComboBox->setEnabled(!checked);
        });

        mOverwriteCheckBox = new QCheckBox(
            //: Save to archive dialog checkbox
            qApp->translate("tremotesf", "Overwrite existing entries"),
            this
        );
        layout->addWidget(mOverwriteCheckBox);

        mDeleteLocalDataCheckBox = new QCheckBox(
            //: Save to archive dialog checkbox
            qApp->translate("tremotesf", "Delete downloaded files after archiving"),
            this
        );
        layout->addWidget(mDeleteLocalDataCheckBox);

        mStatusLabel = new QLabel(this);
        mStatusLabel->setWordWrap(true);
        mStatusLabel->setVisible(false);
        layout->addWidget(mStatusLabel);

        mProgress = new QProgressBar(this);
        mProgress->setTextVisible(false);
        mProgress->setVisible(false);
        layout->addWidget(mProgress);

        mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        mButtonBox
            ->button(QDialogButtonBox::Ok)
            //: Dialog confirmation button
            ->setText(qApp->translate("tremotesf", "Save"));
        layout->addWidget(mButtonBox);

        QObject::connect(mButtonBox, &QDialogButtonBox::accepted, this, &ArchiveSaveDialog::save);
        QObject::connect(mButtonBox, &QDialogButtonBox::rejected, this, &ArchiveSaveDialog::reject);

        loadArchives();
    }

    ArchiveSaveDialog::~ArchiveSaveDialog() {
        if (mReply) {
            QObject::disconnect(mReply, nullptr, this, nullptr);
            mReply->abort();
            mReply->deleteLater();
            mReply = nullptr;
        }
    }

    void ArchiveSaveDialog::loadArchives() {
        mRpc->requestArchive(QJsonObject{{u"action"_s, "list_archives"_L1}}, this, [this](ArchiveResponse response) {
            if (!response.success) {
                return;
            }
            const QString current = mArchiveComboBox->currentText();
            const QJsonArray archives = response.arguments.value("archives"_L1).toArray();
            mArchiveComboBox->clear();
            for (const auto& archive : archives) {
                if (archive.isString()) {
                    mArchiveComboBox->addItem(archive.toString());
                }
            }
            mArchiveComboBox->setCurrentText(current);
        });
    }

    void ArchiveSaveDialog::save() {
        if (mTorrentIds.empty()) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "There are no selected torrents to archive.")
            );
            return;
        }

        if (mUseWhisparrCheckBox->isChecked()) {
            startWhisparrAnalysis();
            return;
        }

        const QString archiveName = mArchiveComboBox->currentText().trimmed();
        if (archiveName.isEmpty()) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "Please enter an archive name.")
            );
            mArchiveComboBox->setFocus();
            return;
        }
        if (!isValidArchiveName(archiveName)) {
            QMessageBox::warning(
                this,
                windowTitle(),
                //: Error message
                qApp->translate("tremotesf", "The archive name contains characters that are not allowed.")
            );
            mArchiveComboBox->setFocus();
            return;
        }

        saveManual(archiveName);
    }

    void ArchiveSaveDialog::saveManual(const QString& archiveName) {
        const auto count = static_cast<int>(mTorrentIds.size());
        if (mDeleteLocalDataCheckBox->isChecked()) {
            if (QMessageBox::question(
                    this,
                    windowTitle(),
                    //: Confirmation message, %n is number of torrents
                    qApp->translate(
                        "tremotesf",
                        "Delete the downloaded files of the %n selected torrent(s) after archiving?",
                        nullptr,
                        count
                    )
                )
                != QMessageBox::Yes) {
                return;
            }
        }

        QJsonArray ids;
        for (const int id : mTorrentIds) {
            ids.append(id);
        }

        updateUiEnabled(false);
        mRpc->requestArchive(
            QJsonObject{
                {u"action"_s, "save"_L1},
                {u"archive"_s, archiveName},
                {u"ids"_s, ids},
                {u"delete_local_data"_s, mDeleteLocalDataCheckBox->isChecked()},
                {u"overwrite"_s, mOverwriteCheckBox->isChecked()},
            },
            this,
            [this, archiveName](ArchiveResponse response) {
                updateUiEnabled(true);
                if (!response.success) {
                    QMessageBox::critical(
                        this,
                        windowTitle(),
                        //: Error message
                        qApp->translate("tremotesf", "Failed to save torrents to archive.")
                    );
                    return;
                }

                const int saved = response.arguments.value("saved"_L1).toInt();
                QString errorsText;
                const QJsonArray errors = response.arguments.value("errors"_L1).toArray();
                for (const auto& errorValue : errors) {
                    const QJsonObject error = errorValue.toObject();
                    if (!errorsText.isEmpty()) {
                        errorsText += u'\n';
                    }
                    errorsText += QStringLiteral("#%1: %2")
                                      .arg(error.value("id"_L1).toInt())
                                      .arg(error.value("error"_L1).toString());
                }

                if (errorsText.isEmpty()) {
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        //: %n is number of torrents, %1 is archive name
                        qApp->translate("tremotesf", "Archived %n torrent(s) to archive \"%1\"", nullptr, saved)
                            .arg(archiveName)
                    );
                    accept();
                } else {
                    QMessageBox::warning(
                        this,
                        windowTitle(),
                        //: %n is number of torrents, %1 is archive name
                        qApp->translate(
                                "tremotesf",
                                "Archived %n torrent(s) to archive \"%1\", but some entries failed:",
                                nullptr,
                                saved
                        )
                                .arg(archiveName)
                            + u'\n'
                            + errorsText
                    );
                }
            }
        );
    }

    void ArchiveSaveDialog::startWhisparrAnalysis() {
        mAnalysisIndex = 0;
        mArchiveNames = QStringList{};
        mArchiveNames.reserve(static_cast<qsizetype>(mTorrents.size()));
        for (qsizetype i = 0; i < mTorrents.size(); ++i) {
            mArchiveNames.append(QString());
        }
        mAnalysisErrors.clear();
        mStatusLabel->setVisible(true);
        mProgress->setRange(0, static_cast<int>(mTorrents.size()));
        mProgress->setValue(0);
        mProgress->setVisible(true);
        updateUiEnabled(false);

        QSet<Torrent*> waiting{};
        for (const auto& torrent : mTorrents) {
            if (torrent && torrent->files().empty()) {
                torrent->setFilesEnabled(true);
                waiting.insert(torrent.data());
            }
        }
        if (waiting.isEmpty()) {
            analyzeNextTorrent();
            return;
        }

        auto remaining = std::make_shared<QSet<Torrent*>>(waiting);
        for (auto* torrent : waiting) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection =
                QObject::connect(torrent, &Torrent::filesUpdated, this, [this, torrent, remaining, connection] {
                    QObject::disconnect(*connection);
                    remaining->remove(torrent);
                    if (remaining->isEmpty()) {
                        analyzeNextTorrent();
                    }
                });
        }
    }

    void ArchiveSaveDialog::analyzeNextTorrent() {
        if (mAnalysisIndex >= mTorrents.size()) {
            finishAnalysis();
            return;
        }

        const auto* torrent = mTorrents[mAnalysisIndex].data();
        const auto torrentName = torrent ? torrent->data().name : QString();
        if (!torrent) {
            mAnalysisErrors.push_back(qApp->translate("tremotesf", "Torrent is no longer available"));
            ++mAnalysisIndex;
            QTimer::singleShot(0, this, &ArchiveSaveDialog::analyzeNextTorrent);
            return;
        }

        const auto title = analysisTitle(torrent);
        if (title.isEmpty()) {
            mAnalysisErrors.push_back(qApp->translate("tremotesf", "%1: no files").arg(torrentName));
            ++mAnalysisIndex;
            QTimer::singleShot(0, this, &ArchiveSaveDialog::analyzeNextTorrent);
            return;
        }

        mStatusLabel->setText(qApp->translate("tremotesf", "Analyzing with Whisparr: %1 (%2/%3)")
                                  .arg(torrentName)
                                  .arg(mAnalysisIndex + 1)
                                  .arg(mTorrents.size()));

        const auto* settings = Settings::instance();
        const QUrl url = whisparr::seriesParseUrl(settings->get_whisparrUrl(), title);
        QNetworkRequest request(url);
        request.setRawHeader("X-Api-Key", settings->get_whisparrApiKey().toUtf8());
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Tremotesf"));

        auto* reply = mNetwork->get(request);
        auto sslErrors = std::make_shared<QList<QSslError>>();
        QObject::connect(reply, &QNetworkReply::sslErrors, reply, [sslErrors](const QList<QSslError>& errors) {
            *sslErrors = errors;
        });
        whisparr::applySelfSignedCertificatePolicy(reply, Settings::instance()->get_whisparrIgnoreSslErrors());
        mReply = reply;
        auto* timeout = new QTimer(reply);
        timeout->setSingleShot(true);
        timeout->start(15000);
        QObject::connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, sslErrors, torrentName] {
            if (reply != mReply) {
                return;
            }
            mReply = nullptr;

            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString error{};
            if (httpStatus == 401) {
                error = qApp->translate("tremotesf", "Invalid Whisparr API key");
            } else if (reply->error() != QNetworkReply::NoError) {
                if (reply->error() == QNetworkReply::OperationCanceledError) {
                    error = qApp->translate("tremotesf", "Connection to Whisparr timed out");
                } else if (!sslErrors->isEmpty()) {
                    error = qApp->translate("tremotesf", "TLS errors:")
                            + QStringLiteral("\n")
                            + whisparr::sslErrorsText(*sslErrors);
                } else {
                    error = reply->errorString();
                }
            } else if (httpStatus == 204) {
                error = qApp->translate("tremotesf", "Series was not recognized");
            } else if (httpStatus != 200) {
                error = qApp->translate("tremotesf", "Whisparr returned HTTP status %1").arg(httpStatus);
            } else {
                const auto document = QJsonDocument::fromJson(reply->readAll());
                const auto archiveName = sanitizeArchiveName(document.object().value("title"_L1).toString());
                if (archiveName.isEmpty()) {
                    error = qApp->translate("tremotesf", "Series was not recognized");
                } else if (!isValidArchiveName(archiveName)) {
                    error = qApp->translate("tremotesf", "Invalid archive name");
                } else {
                    mArchiveNames[mAnalysisIndex] = archiveName;
                }
            }

            if (!error.isEmpty()) {
                mAnalysisErrors.push_back(QStringLiteral("%1: %2").arg(torrentName, error));
            }
            ++mAnalysisIndex;
            mProgress->setValue(static_cast<int>(mAnalysisIndex));
            reply->deleteLater();
            QTimer::singleShot(0, this, &ArchiveSaveDialog::analyzeNextTorrent);
        });
    }

    void ArchiveSaveDialog::finishAnalysis() {
        mProgress->setVisible(false);
        mStatusLabel->setVisible(false);
        updateUiEnabled(true);

        if (!mAnalysisErrors.isEmpty()) {
            QMessageBox::warning(
                this,
                windowTitle(),
                qApp->translate("tremotesf", "Whisparr could not determine archive names:")
                    + QStringLiteral("\n")
                    + mAnalysisErrors.join('\n')
            );
            return;
        }

        confirmAndSave(groupByArchive(mTorrents, mArchiveNames));
    }

    void ArchiveSaveDialog::confirmAndSave(const QVector<QPair<QString, QVector<int>>>& groups) {
        if (groups.isEmpty()) {
            return;
        }

        QDialog confirmation(this);
        confirmation.setWindowTitle(qApp->translate("tremotesf", "Confirm archiving"));
        auto* layout = new QVBoxLayout(&confirmation);

        auto* label = new QLabel(
            qApp->translate("tremotesf", "The following torrents will be saved to archives:"),
            &confirmation
        );
        label->setWordWrap(true);
        layout->addWidget(label);

        auto* table = new QTableWidget(&confirmation);
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels(
            {qApp->translate("tremotesf", "Torrent"), qApp->translate("tremotesf", "Archive")}
        );
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setRowCount(static_cast<int>(mTorrents.size()));

        for (qsizetype i = 0; i < mTorrents.size(); ++i) {
            const auto* torrent = mTorrents[i].data();
            table->setItem(static_cast<int>(i), 0, new QTableWidgetItem(torrent ? torrent->data().name : QString()));
            table->setItem(static_cast<int>(i), 1, new QTableWidgetItem(mArchiveNames[i]));
        }
        layout->addWidget(table);

        auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &confirmation);
        buttonBox->button(QDialogButtonBox::Ok)->setText(qApp->translate("tremotesf", "Save"));
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &confirmation, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, &confirmation, &QDialog::reject);
        layout->addWidget(buttonBox);

        if (confirmation.exec() != QDialog::Accepted) {
            return;
        }

        if (mDeleteLocalDataCheckBox->isChecked()) {
            if (QMessageBox::question(
                    this,
                    windowTitle(),
                    qApp->translate(
                        "tremotesf",
                        "Delete the downloaded files of the %n selected torrent(s) after archiving?",
                        nullptr,
                        static_cast<int>(mTorrentIds.size())
                    )
                )
                != QMessageBox::Yes) {
                return;
            }
        }

        performSave(groups);
    }

    void ArchiveSaveDialog::performSave(const QVector<QPair<QString, QVector<int>>>& groups) {
        struct State {
            int pending{};
            int saved{};
            QStringList errors{};
        };
        auto state = std::make_shared<State>();
        state->pending = static_cast<int>(groups.size());

        updateUiEnabled(false);
        for (const auto& [archiveName, torrentIds] : groups) {
            QJsonArray ids;
            for (const int id : torrentIds) {
                ids.append(id);
            }
            mRpc->requestArchive(
                QJsonObject{
                    {u"action"_s, "save"_L1},
                    {u"archive"_s, archiveName},
                    {u"ids"_s, ids},
                    {u"delete_local_data"_s, mDeleteLocalDataCheckBox->isChecked()},
                    {u"overwrite"_s, mOverwriteCheckBox->isChecked()},
                },
                this,
                [this, state, archiveName](ArchiveResponse response) {
                    if (!response.success) {
                        state->errors.push_back(qApp->translate("tremotesf", "%1: failed to save").arg(archiveName));
                    } else {
                        state->saved += response.arguments.value("saved"_L1).toInt();
                        const QJsonArray errors = response.arguments.value("errors"_L1).toArray();
                        for (const auto& errorValue : errors) {
                            const QJsonObject error = errorValue.toObject();
                            state->errors.push_back(QStringLiteral("%1: #%2: %3")
                                                        .arg(archiveName)
                                                        .arg(error.value("id"_L1).toInt())
                                                        .arg(error.value("error"_L1).toString()));
                        }
                    }
                    --state->pending;
                    if (state->pending == 0) {
                        updateUiEnabled(true);
                        if (state->errors.isEmpty()) {
                            QMessageBox::information(
                                this,
                                windowTitle(),
                                qApp->translate(
                                    "tremotesf",
                                    "Archived %n torrent(s) to archive(s).",
                                    nullptr,
                                    state->saved
                                )
                            );
                            accept();
                        } else {
                            QMessageBox::warning(
                                this,
                                windowTitle(),
                                qApp->translate("tremotesf", "Some entries failed to archive:")
                                    + QStringLiteral("\n")
                                    + state->errors.join('\n')
                            );
                        }
                    }
                }
            );
        }
    }

    void ArchiveSaveDialog::updateUiEnabled(bool enabled) {
        mUseWhisparrCheckBox->setEnabled(enabled && mWhisparrConfigured);
        mArchiveComboBox->setEnabled(enabled && !mUseWhisparrCheckBox->isChecked());
        mOverwriteCheckBox->setEnabled(enabled);
        mDeleteLocalDataCheckBox->setEnabled(enabled);
        mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    }

    QString ArchiveSaveDialog::analysisTitle(const Torrent* torrent) const {
        const auto& files = torrent->files();
        if (files.size() == 1) {
            return QFileInfo(files.front().path).fileName();
        }
        return torrent->data().name;
    }
}
