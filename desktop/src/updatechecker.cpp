// SPDX-FileCopyrightText: 2026 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "updatechecker.h"

#include <memory>

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

#include "log/log.h"

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <shellapi.h>
#endif

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        constexpr auto manifestUrl =
            "https://github.com/SNBogdanov/tremotesf-monorepo/releases/latest/download/update.json"_L1;
        constexpr auto releasesUrl = "https://github.com/SNBogdanov/tremotesf-monorepo/releases"_L1;

        QString currentVersion() { return QCoreApplication::applicationVersion(); }
    }

    UpdateChecker::UpdateChecker(QWidget* parentWindow)
        : QObject(parentWindow), mParentWindow(parentWindow), mNetwork(new QNetworkAccessManager(this)) {}

    void UpdateChecker::checkForUpdates(bool silent) {
        if (mReply) {
            return;
        }

        QNetworkRequest request{QUrl(manifestUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Tremotesf"));
        request.setRawHeader("Accept", "application/json");
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        info().log("Checking for updates, current version: {}", currentVersion());

        mReply = mNetwork->get(request);
        auto* timeout = new QTimer(mReply);
        timeout->setSingleShot(true);
        timeout->start(15000);
        QObject::connect(timeout, &QTimer::timeout, mReply, &QNetworkReply::abort);
        QObject::connect(mReply, &QNetworkReply::finished, this, [this, silent] {
            auto* reply = mReply;
            mReply = nullptr;
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                warning().log("Update check failed: {}", reply->errorString());
                if (!silent) {
                    showError(
                        qApp->translate("tremotesf", "Failed to check for updates: %1").arg(reply->errorString())
                    );
                }
                return;
            }

            QJsonParseError parseError{};
            const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                if (!silent) {
                    showError(qApp->translate("tremotesf", "Invalid update manifest"));
                }
                return;
            }

            const auto object = document.object();
            const auto version = object.value("version"_L1).toString();
            const auto remoteVersion = QVersionNumber::fromString(version);
            const auto localVersion = QVersionNumber::fromString(currentVersion());
            if (remoteVersion.isNull()) {
                if (!silent) {
                    showError(qApp->translate("tremotesf", "Invalid update manifest"));
                }
                return;
            }

            if (QVersionNumber::compare(remoteVersion, localVersion) <= 0) {
                info().log("No updates available, latest version: {}", version);
                if (!silent) {
                    QMessageBox::information(
                        mParentWindow,
                        qApp->translate("tremotesf", "Check for Updates"),
                        qApp->translate("tremotesf", "You are using the latest version (%1).").arg(currentVersion())
                    );
                }
                return;
            }

            const auto windows = object.value("windows"_L1).toObject();
            const auto url = QUrl(windows.value("url"_L1).toString());
            const auto sha256 = windows.value("sha256"_L1).toString();
            if (!url.isValid() || !url.scheme().startsWith("https"_L1) || sha256.isEmpty()) {
                if (!silent) {
                    showError(qApp->translate("tremotesf", "Update manifest does not contain a Windows package"));
                }
                return;
            }

            QMessageBox dialog(
                QMessageBox::Question,
                qApp->translate("tremotesf", "Update Available"),
                qApp->translate("tremotesf", "Version %1 is available. Current version: %2.\n\nDownload and install it now?")
                    .arg(version, currentVersion()),
                QMessageBox::Yes | QMessageBox::No,
                mParentWindow
            );
            info().log("Update available: {}", version);
            dialog.setDefaultButton(QMessageBox::Yes);
            if (dialog.exec() != QMessageBox::Yes) {
                return;
            }

            downloadUpdate(url, sha256, version);
        });
    }

    void UpdateChecker::downloadUpdate(const QUrl& url, const QString& sha256, const QString& version) {
        if (mDownloadReply) {
            return;
        }

        const auto directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (directory.isEmpty()) {
            showError(qApp->translate("tremotesf", "Failed to find a temporary directory"));
            return;
        }
        QDir().mkpath(directory);
        const auto filePath = QDir(directory).filePath(QStringLiteral("tremotesf-update-%1.msi").arg(version));
        auto file = std::make_unique<QFile>(filePath);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            showError(qApp->translate("tremotesf", "Failed to create update file: %1").arg(file->errorString()));
            return;
        }

        mProgress = new QProgressDialog(
            qApp->translate("tremotesf", "Downloading update..."),
            qApp->translate("tremotesf", "Cancel"),
            0,
            100,
            mParentWindow
        );
        mProgress->setWindowModality(Qt::WindowModal);
        mProgress->setMinimumDuration(0);
        mProgress->setValue(0);
        mProgress->show();

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Tremotesf"));
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        mDownloadReply = mNetwork->get(request);
        auto* reply = mDownloadReply;

        QObject::connect(mProgress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
        QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, file = file.get()] {
            file->write(reply->readAll());
        });
        QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (mProgress && total > 0) {
                mProgress->setValue(static_cast<int>(received * 100 / total));
            }
        });
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, file = file.release(), sha256, filePath] {
            std::unique_ptr<QFile> fileGuard(file);
            mDownloadReply = nullptr;
            reply->deleteLater();

            file->write(reply->readAll());
            file->close();
            closeProgress();

            if (reply->error() != QNetworkReply::NoError) {
                QFile::remove(filePath);
                showError(qApp->translate("tremotesf", "Failed to download update: %1").arg(reply->errorString()));
                return;
            }

            QFile downloaded(filePath);
            if (!downloaded.open(QIODevice::ReadOnly)) {
                QFile::remove(filePath);
                showError(qApp->translate("tremotesf", "Failed to open downloaded update"));
                return;
            }
            const auto actualHash =
                QString::fromLatin1(QCryptographicHash::hash(downloaded.readAll(), QCryptographicHash::Sha256).toHex());
            downloaded.close();
            if (actualHash.compare(sha256, Qt::CaseInsensitive) != 0) {
                QFile::remove(filePath);
                showError(qApp->translate("tremotesf", "Update file checksum mismatch"));
                return;
            }

#ifdef Q_OS_WIN
            const auto parameters = QStringLiteral("/i \"%1\" /passive /norestart").arg(QDir::toNativeSeparators(filePath));
            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS;
            info.lpVerb = L"runas";
            info.lpFile = L"msiexec.exe";
            info.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
            info.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteExW(&info)) {
                showError(qApp->translate("tremotesf", "Failed to start the installer"));
                return;
            }
            if (info.hProcess) {
                CloseHandle(info.hProcess);
            }
            QCoreApplication::quit();
#else
            showError(qApp->translate("tremotesf", "Update installation is supported only on Windows"));
#endif
        });
    }

    void UpdateChecker::showError(const QString& message) {
        QMessageBox::warning(mParentWindow, qApp->translate("tremotesf", "Check for Updates"), message);
    }

    void UpdateChecker::closeProgress() {
        if (mProgress) {
            mProgress->close();
            mProgress->deleteLater();
            mProgress = nullptr;
        }
    }
}
