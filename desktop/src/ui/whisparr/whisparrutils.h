// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_WHISPARR_WHISPARRUTILS_H
#define TREMOTESF_UI_WHISPARR_WHISPARRUTILS_H

#include <QFileInfo>
#include <QNetworkReply>
#include <QSet>
#include <QSslError>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

namespace tremotesf::whisparr {
    using namespace Qt::StringLiterals;

    inline bool isVideoFile(const QString& path) {
        static const QSet<QString> extensions{
            u"mkv"_s,
            u"mp4"_s,
            u"avi"_s,
            u"wmv"_s,
            u"mov"_s,
            u"m4v"_s,
            u"ts"_s,
            u"webm"_s,
            u"flv"_s,
            u"mpg"_s,
            u"mpeg"_s,
            u"iso"_s,
            u"m2ts"_s
        };
        return extensions.contains(QFileInfo(path).suffix().toLower());
    }

    inline bool isInSampleDirectory(const QString& path) {
        const auto parts = path.split(u'/', Qt::SkipEmptyParts);
        for (const auto& part : parts) {
            if (part.compare("sample"_L1, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    }

    inline QString joinRemotePath(const QString& directory, const QString& path) {
        if (directory.isEmpty()) {
            return path;
        }
        if (directory.endsWith(u'/')) {
            return directory + path;
        }
        return directory + QStringLiteral("/") + path;
    }

    inline QUrl parseUrl(const QString& baseUrl, const QString& fileName, const QString& fullPath) {
        QUrl url(baseUrl);
        if (url.scheme().isEmpty()) {
            url = QUrl(QStringLiteral("http://") + baseUrl);
        }
        auto path = url.path();
        while (path.endsWith(u'/')) {
            path.chop(1);
        }
        if (path.endsWith("/api/v3"_L1)) {
            path += "/parse"_L1;
        } else {
            path += "/api/v3/parse"_L1;
        }
        url.setPath(path);

        QUrlQuery query{};
        query.addQueryItem("title"_L1, fileName);
        query.addQueryItem("path"_L1, fullPath);
        url.setQuery(query);
        return url;
    }

    inline QUrl seriesParseUrl(const QString& baseUrl, const QString& title) {
        QUrl url(baseUrl);
        if (url.scheme().isEmpty()) {
            url = QUrl(QStringLiteral("http://") + baseUrl);
        }
        auto path = url.path();
        while (path.endsWith(u'/')) {
            path.chop(1);
        }
        if (path.endsWith("/api/v3"_L1)) {
            path += "/parse/series"_L1;
        } else {
            path += "/api/v3/parse/series"_L1;
        }
        url.setPath(path);

        QUrlQuery query{};
        query.addQueryItem("title"_L1, title);
        url.setQuery(query);
        return url;
    }

    inline QString ensureFileExtension(const QString& candidate, const QString& originalName) {
        const QFileInfo candidateInfo(QFileInfo(candidate).fileName());
        const QFileInfo originalInfo(originalName);
        const auto originalSuffix = originalInfo.suffix();
        if (originalSuffix.isEmpty() || candidateInfo.suffix().compare(originalSuffix, Qt::CaseInsensitive) == 0) {
            return candidateInfo.fileName();
        }
        return candidateInfo.completeBaseName() + QStringLiteral(".") + originalSuffix;
    }

    inline void applySelfSignedCertificatePolicy(QNetworkReply* reply, bool allowSelfSigned) {
        if (!allowSelfSigned) {
            return;
        }
        QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) {
            reply->ignoreSslErrors();
        });
    }

    inline QString sslErrorsText(const QList<QSslError>& errors) {
        QStringList lines{};
        lines.reserve(errors.size());
        for (const auto& error : errors) {
            lines.push_back(error.errorString());
        }
        return lines.join('\n');
    }
}

#endif // TREMOTESF_UI_WHISPARR_WHISPARRUTILS_H
