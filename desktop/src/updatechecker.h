// SPDX-FileCopyrightText: 2026 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UPDATECHECKER_H
#define TREMOTESF_UPDATECHECKER_H

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class QUrl;
class QWidget;

namespace tremotesf {
    class UpdateChecker final : public QObject {
        Q_OBJECT

    public:
        explicit UpdateChecker(QWidget* parentWindow);

        void checkForUpdates(bool silent);

    private:
        void downloadUpdate(const QUrl& url, const QString& sha256, const QString& version);
        void showError(const QString& message);
        void closeProgress();

        QWidget* mParentWindow;
        QNetworkAccessManager* mNetwork{};
        QNetworkReply* mReply{};
        QNetworkReply* mDownloadReply{};
        QProgressDialog* mProgress{};
    };
}

#endif // TREMOTESF_UPDATECHECKER_H
