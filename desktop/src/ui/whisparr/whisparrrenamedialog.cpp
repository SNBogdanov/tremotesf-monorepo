// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "whisparrrenamedialog.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "rpc/torrent.h"
#include "settings.h"
#include "whisparrutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString replaceFileName(const QString& oldPath, const QString& newFileName) {
            const auto directory = QFileInfo(oldPath).path();
            if (directory.isEmpty() || directory == "."_L1) {
                return newFileName;
            }
            return directory + QStringLiteral("/") + newFileName;
        }

        QString appendNumberToFileName(const QString& path, int number) {
            const QFileInfo info(path);
            const auto directory = info.path();
            const auto suffix = info.suffix();
            auto name = info.completeBaseName() + u" (%1)"_s.arg(number);
            if (!suffix.isEmpty()) {
                name += QStringLiteral(".") + suffix;
            }
            if (directory.isEmpty() || directory == "."_L1) {
                return name;
            }
            return directory + QStringLiteral("/") + name;
        }
    }

    WhisparrRenameDialog::WhisparrRenameDialog(
        QVector<Torrent*> torrents, QString baseUrl, QString apiKey, QWidget* parent
    )
        : QDialog(parent),
          mTorrents(std::move(torrents)),
          mBaseUrl(std::move(baseUrl)),
          mApiKey(std::move(apiKey)),
          mNetwork(new QNetworkAccessManager(this)) {
        setWindowTitle(qApp->translate("tremotesf", "Whisparr: recognize and rename"));
        resize(900, 500);

        auto* layout = new QVBoxLayout(this);

        auto* infoLabel = new QLabel(this);
        infoLabel->setWordWrap(true);
        if (mTorrents.size() == 1 && mTorrents.first()) {
            infoLabel->setText(qApp->translate("tremotesf", "Torrent: %1").arg(mTorrents.first()->data().name));
        } else {
            infoLabel->setText(qApp->translate("tremotesf", "Torrents: %1").arg(mTorrents.size()));
        }
        layout->addWidget(infoLabel);

        const bool multipleTorrents = mTorrents.size() > 1;
        mNewNameColumn = multipleTorrents ? 2 : 1;
        mTable = new QTableWidget(this);
        mTable->setColumnCount(multipleTorrents ? 3 : 2);
        if (multipleTorrents) {
            mTable->setHorizontalHeaderLabels(
                {qApp->translate("tremotesf", "Torrent"),
                 qApp->translate("tremotesf", "File"),
                 qApp->translate("tremotesf", "New name")}
            );
        } else {
            mTable->setHorizontalHeaderLabels(
                {qApp->translate("tremotesf", "File"), qApp->translate("tremotesf", "New name")}
            );
        }
        auto* header = mTable->horizontalHeader();
        for (int column = 0; column < mTable->columnCount(); ++column) {
            header->setSectionResizeMode(column, QHeaderView::Interactive);
        }
        header->setMinimumSectionSize(80);
        const auto headerState = multipleTorrents ? Settings::instance()->get_whisparrRenameDialogMultiHeaderState()
                                                  : Settings::instance()->get_whisparrRenameDialogHeaderState();
        if (!headerState.isEmpty()) {
            header->restoreState(headerState);
        } else if (multipleTorrents) {
            mTable->setColumnWidth(0, 260);
            mTable->setColumnWidth(1, 420);
            mTable->setColumnWidth(2, 420);
        } else {
            mTable->setColumnWidth(0, 450);
            mTable->setColumnWidth(1, 450);
        }
        mTable->verticalHeader()->setVisible(false);
        mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        layout->addWidget(mTable);

        mStatusLabel = new QLabel(this);
        mStatusLabel->setWordWrap(true);
        layout->addWidget(mStatusLabel);

        mProgress = new QProgressBar(this);
        mProgress->setTextVisible(false);
        layout->addWidget(mProgress);

        auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        mRenameButton = buttonBox->button(QDialogButtonBox::Ok);
        mRenameButton->setText(qApp->translate("tremotesf", "Rename"));
        mRenameButton->setEnabled(false);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &WhisparrRenameDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &WhisparrRenameDialog::reject);
        layout->addWidget(buttonBox);

        collectFiles();
        mTable->setRowCount(static_cast<int>(mEntries.size()));
        for (int i = 0; i < mEntries.size(); ++i) {
            const auto& entry = mEntries[i];
            if (multipleTorrents) {
                mTable->setItem(i, 0, new QTableWidgetItem(entry.torrentName));
                mTable->setItem(i, 1, new QTableWidgetItem(QFileInfo(entry.oldPath).fileName()));
            } else {
                mTable->setItem(i, 0, new QTableWidgetItem(QFileInfo(entry.oldPath).fileName()));
            }
            mTable->setItem(i, mNewNameColumn, new QTableWidgetItem(QStringLiteral("...")));
            updateTableRow(i);
        }

        mProgress->setRange(0, static_cast<int>(mEntries.size()));
        mProgress->setValue(0);

        if (mEntries.isEmpty()) {
            mStatusLabel->setText(qApp->translate("tremotesf", "No video files to rename"));
            mProgress->hide();
            mRenameButton->setEnabled(false);
        } else {
            updateStatus();
            QTimer::singleShot(0, this, &WhisparrRenameDialog::startNextRequest);
        }
    }

    WhisparrRenameDialog::~WhisparrRenameDialog() {
        if (mTable) {
            const auto headerState = mTable->horizontalHeader()->saveState();
            if (mTable->columnCount() > 2) {
                Settings::instance()->set_whisparrRenameDialogMultiHeaderState(headerState);
            } else {
                Settings::instance()->set_whisparrRenameDialogHeaderState(headerState);
            }
        }
        if (mReply) {
            QObject::disconnect(mReply, nullptr, this, nullptr);
            mReply->abort();
            mReply->deleteLater();
            mReply = nullptr;
        }
    }

    void WhisparrRenameDialog::collectFiles() {
        for (auto* torrent : mTorrents) {
            if (!torrent) {
                continue;
            }
            const auto& files = torrent->files();
            const bool singleFile = files.size() == 1;
            const auto torrentName = torrent->data().name;
            const auto downloadDirectory = torrent->data().downloadDirectory;
            for (const auto& file : files) {
                if (!singleFile && (!whisparr::isVideoFile(file.path) || whisparr::isInSampleDirectory(file.path))) {
                    continue;
                }
                Entry entry{};
                entry.torrent = torrent;
                entry.torrentName = torrentName;
                entry.oldPath = file.path;
                entry.fullPath = whisparr::joinRemotePath(downloadDirectory, file.path);
                mEntries.push_back(std::move(entry));
            }
        }
    }

    void WhisparrRenameDialog::startNextRequest() {
        if (mCurrentIndex >= mEntries.size()) {
            finishParsing();
            return;
        }

        const auto& entry = mEntries[mCurrentIndex];
        updateStatus();

        QNetworkRequest request(parseUrl(entry));
        request.setRawHeader("X-Api-Key", mApiKey.toUtf8());
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
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, sslErrors] {
            if (reply != mReply) {
                return;
            }
            mReply = nullptr;

            auto& entry = mEntries[mCurrentIndex];
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (httpStatus == 401) {
                entry.error = qApp->translate("tremotesf", "Invalid Whisparr API key");
            } else if (reply->error() != QNetworkReply::NoError) {
                if (reply->error() == QNetworkReply::OperationCanceledError) {
                    entry.error = qApp->translate("tremotesf", "Connection to Whisparr timed out");
                } else if (!sslErrors->isEmpty()) {
                    entry.error = qApp->translate("tremotesf", "TLS errors:")
                                  + QStringLiteral("\n")
                                  + whisparr::sslErrorsText(*sslErrors);
                } else {
                    entry.error = reply->errorString();
                }
            } else if (httpStatus == 204) {
                entry.error = qApp->translate("tremotesf", "File was not recognized");
            } else if (httpStatus != 200) {
                entry.error = qApp->translate("tremotesf", "Whisparr returned HTTP status %1").arg(httpStatus);
            } else {
                const auto document = QJsonDocument::fromJson(reply->readAll());
                const auto candidates = document.object().value("candidates"_L1).toArray();
                for (const auto& candidate : candidates) {
                    const auto fileName = candidate.toObject().value("fileName"_L1).toString();
                    if (!fileName.isEmpty()) {
                        const auto oldName = QFileInfo(entry.oldPath).fileName();
                        const auto newName = whisparr::ensureFileExtension(fileName, oldName);
                        entry.newPath = replaceFileName(entry.oldPath, newName);
                        entry.parsed = true;
                        entry.changed = entry.newPath != entry.oldPath;
                        break;
                    }
                }
                if (!entry.parsed) {
                    entry.error = qApp->translate("tremotesf", "File was not recognized");
                }
            }

            updateTableRow(static_cast<int>(mCurrentIndex));
            ++mCurrentIndex;
            mProgress->setValue(static_cast<int>(mCurrentIndex));
            reply->deleteLater();
            QTimer::singleShot(0, this, &WhisparrRenameDialog::startNextRequest);
        });
    }

    void WhisparrRenameDialog::finishParsing() {
        QSet<QString> usedPaths{};
        for (const auto& entry : mEntries) {
            if (!entry.changed) {
                usedPaths.insert((entry.parsed ? entry.targetFullPath() : entry.fullPath).toLower());
            }
        }
        for (auto& entry : mEntries) {
            if (!entry.changed) {
                continue;
            }
            const auto original = entry.newPath;
            auto candidate = original;
            int counter = 1;
            while (usedPaths.contains(entry.targetFullPath().toLower())) {
                ++counter;
                candidate = appendNumberToFileName(original, counter);
                entry.newPath = candidate;
            }
            usedPaths.insert(entry.targetFullPath().toLower());
        }

        for (int i = 0; i < mEntries.size(); ++i) {
            updateTableRow(i);
        }

        const auto parsedCount =
            static_cast<int>(std::ranges::count_if(mEntries, [](const Entry& entry) { return entry.parsed; }));
        const auto changedCount =
            static_cast<int>(std::ranges::count_if(mEntries, [](const Entry& entry) { return entry.changed; }));
        mProgress->hide();
        if (parsedCount == 0) {
            mStatusLabel->setText(qApp->translate("tremotesf", "Whisparr could not recognize any files"));
        } else {
            mStatusLabel->setText(
                qApp->translate("tremotesf", "Recognized: %1, to rename: %2").arg(parsedCount).arg(changedCount)
            );
        }
        mRenameButton->setEnabled(changedCount > 0);
    }

    void WhisparrRenameDialog::updateTableRow(int row) {
        if (row < 0 || row >= mEntries.size()) {
            return;
        }
        const auto& entry = mEntries[row];
        auto* nameItem = mTable->item(row, mNewNameColumn);
        if (!nameItem) {
            return;
        }

        auto font = nameItem->font();
        font.setBold(false);
        font.setItalic(false);
        if (entry.changed) {
            nameItem->setText(QFileInfo(entry.newPath).fileName());
            font.setBold(true);
        } else if (entry.parsed) {
            nameItem->setText(QFileInfo(entry.newPath).fileName());
        } else if (!entry.error.isEmpty()) {
            nameItem->setText(entry.error);
            font.setItalic(true);
        } else {
            nameItem->setText(QStringLiteral("..."));
        }
        nameItem->setFont(font);
        nameItem->setToolTip(entry.error);
    }

    void WhisparrRenameDialog::updateStatus() {
        mStatusLabel->setText(qApp->translate("tremotesf", "Recognizing: %1 (%2/%3)")
                                  .arg(QFileInfo(mEntries[mCurrentIndex].oldPath).fileName())
                                  .arg(mCurrentIndex + 1)
                                  .arg(mEntries.size()));
    }

    QUrl WhisparrRenameDialog::parseUrl(const Entry& entry) const {
        return whisparr::parseUrl(mBaseUrl, QFileInfo(entry.oldPath).fileName(), entry.fullPath);
    }

    void WhisparrRenameDialog::accept() {
        for (const auto& entry : mEntries) {
            if (entry.changed && entry.torrent) {
                entry.torrent->renameFile(entry.oldPath, QFileInfo(entry.newPath).fileName());
            }
        }
        QDialog::accept();
    }
}
