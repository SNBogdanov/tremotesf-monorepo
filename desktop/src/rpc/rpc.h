// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_RPC_H
#define TREMOTESF_RPC_RPC_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include "coroutines/scope.h"
#include "torrent.h"

#ifdef Q_MOC_RUN
#    include "serversettings.h"
#    include "serverstats.h"
#else
namespace tremotesf {
    class ServerSettings;
    class ServerStats;
    class ScriptJobsManager;
}
#endif

namespace tremotesf {
    Q_NAMESPACE

    namespace impl {
        class RequestRouter;
    }

    struct ArchiveResponse {
        bool success{};
        QJsonObject arguments{};
    };

    struct ScriptResponse {
        bool success{};
        QJsonObject arguments{};
        QString result{};
    };

    struct ConnectionConfiguration {
        Q_GADGET
    public:
        enum class ProxyType { Default, Http, Socks5, None };
        Q_ENUM(ProxyType)

        enum class ServerCertificateMode { None, SelfSigned, CustomRoot };
        Q_ENUM(ServerCertificateMode)

        QString address{};
        int port{};
        QString apiPath{};

        ProxyType proxyType{ProxyType::Default};
        QString proxyHostname{};
        int proxyPort{};
        QString proxyUser{};
        QString proxyPassword{};

        bool https{};

        ServerCertificateMode serverCertificateMode{};
        QByteArray serverRootCertificate{};
        QByteArray serverLeafCertificate{};

        bool clientCertificateEnabled{};
        QByteArray clientCertificate{};

        bool authentication{};
        QString username{};
        QString password{};

        int updateInterval{};
        int timeout{};

        bool autoReconnectEnabled{};
        int autoReconnectInterval{};

        bool operator==(const ConnectionConfiguration& rhs) const = default;
    };

    enum class RpcConnectionState { Disconnected, Connecting, Connected };
    Q_ENUM_NS(RpcConnectionState)

    enum class RpcError {
        NoError,
        TimedOut,
        ConnectionError,
        AuthenticationError,
        ParseError,
        ServerIsTooNew,
        ServerIsTooOld
    };
    Q_ENUM_NS(RpcError)

    class Rpc : public QObject {
        Q_OBJECT
    public:
        using ConnectionState = RpcConnectionState;
        using Error = RpcError;

        explicit Rpc(QObject* parent = nullptr);
        ~Rpc() override;
        Q_DISABLE_COPY_MOVE(Rpc)

        ServerSettings* serverSettings() const;
        ServerStats* serverStats() const;
        ScriptJobsManager* scriptJobsManager() const;

        const std::vector<std::unique_ptr<Torrent>>& torrents() const;
        Torrent* torrentByHash(const QString& hash) const;
        Torrent* torrentById(int id) const;

        struct Status {
            ConnectionState connectionState{ConnectionState::Disconnected};
            Error error{Error::NoError};
            QString errorMessage{};
            QString detailedErrorMessage{};

            bool operator==(const Status& other) const = default;

            QString toString() const;
        };

        bool isConnected() const;
        const Status& status() const;
        ConnectionState connectionState() const;
        Error error() const;
        const QString& errorMessage() const;
        const QString& detailedErrorMessage() const;
        bool isLocal() const;

        int torrentsCount() const;

        void setConnectionConfiguration(const ConnectionConfiguration& configuration);
        void resetConnectionConfiguration();

        void connect();
        void disconnect();

        enum class DeleteFileMode { No, Delete, MoveToTrash };

        void addTorrentFile(
            QString filePath,
            QString downloadDirectory,
            std::vector<int> unwantedFiles,
            std::vector<int> highPriorityFiles,
            std::vector<int> lowPriorityFiles,
            std::map<QString, QString> renamedFiles,
            TorrentData::Priority bandwidthPriority,
            bool start,
            DeleteFileMode deleteFileMode,
            std::vector<QString> labels,
            std::optional<bool> sequentialDownload = std::nullopt,
            std::optional<bool> metadataOnly = std::nullopt
        );

        void addTorrentLinks(
            QStringList links,
            QString downloadDirectory,
            TorrentData::Priority bandwidthPriority,
            bool start,
            std::vector<QString> labels,
            std::optional<bool> sequentialDownload = std::nullopt,
            std::optional<bool> metadataOnly = std::nullopt
        );

        void startTorrents(std::span<const int> ids);
        void startTorrentsNow(std::span<const int> ids);
        void pauseTorrents(std::span<const int> ids);
        void removeTorrents(std::span<const int> ids, bool deleteFiles);
        void checkTorrents(std::span<const int> ids);
        void moveTorrentsToTop(std::span<const int> ids);
        void moveTorrentsUp(std::span<const int> ids);
        void moveTorrentsDown(std::span<const int> ids);
        void moveTorrentsToBottom(std::span<const int> ids);

        void reannounceTorrents(std::span<const int> ids);

        void setSessionProperty(QString property, QJsonValue value);
        void setSessionProperties(QJsonObject properties);
        void setTorrentProperty(int id, QString property, QJsonValue value, bool updateIfSuccessful = false);
        void setTorrentsIncompleteDirectory(std::span<const int> ids, QString directory, bool move);
        void setTorrentsLocation(std::span<const int> ids, QString location, bool moveFiles);
        void setTorrentsLabels(std::span<const int> ids, std::span<const QString> labels);
        void getTorrentFiles(int torrentId);
        void getTorrentPeers(int torrentId);

        void renameTorrentFile(int torrentId, QString filePath, QString newName);

        Coroutine<std::optional<qint64>> getFreeSpaceForPath(QString path);
        void
        requestFreeSpaceForPath(QString path, QObject* context, std::function<void(std::optional<qint64>)> callback);

        void requestArchive(QJsonObject arguments, QObject* context, std::function<void(ArchiveResponse)> callback);

        void requestScriptRun(QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback);
        void requestScriptStatus(QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback);
        void requestScriptCancel(QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback);
        void
        requestScriptSettingsGet(QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback);
        void
        requestScriptSettingsSet(QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback);
        void requestScriptRestartDaemon(
            QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback
        );

        void shutdownServer();

    private:
        void setStatus(Status&& status);
        void resetStateOnConnectionStateChanged(ConnectionState oldConnectionState);
        void emitSignalsOnConnectionStateChanged(ConnectionState oldConnectionState);

        Coroutine<> postRequest(QLatin1String method, QJsonObject arguments, bool updateIfSuccessful = true);

        Coroutine<> addTorrentFileImpl(
            QString filePath,
            QString downloadDirectory,
            std::vector<int> unwantedFiles,
            std::vector<int> highPriorityFiles,
            std::vector<int> lowPriorityFiles,
            std::map<QString, QString> renamedFiles,
            TorrentData::Priority bandwidthPriority,
            bool start,
            DeleteFileMode deleteFileMode,
            std::vector<QString> labels,
            std::optional<bool> sequentialDownload = std::nullopt,
            std::optional<bool> metadataOnly = std::nullopt
        );
        Coroutine<> addTorrentLinksImpl(
            QStringList links,
            QString downloadDirectory,
            TorrentData::Priority bandwidthPriority,
            bool start,
            std::vector<QString> labels,
            std::optional<bool> sequentialDownload = std::nullopt,
            std::optional<bool> metadataOnly = std::nullopt
        );
        Coroutine<> addTorrentLinkImpl(
            QString link,
            QString downloadDirectory,
            int bandwidthPriority,
            bool start,
            std::vector<QString> labels,
            std::optional<bool> sequentialDownload = std::nullopt,
            std::optional<bool> metadataOnly = std::nullopt
        );
        Coroutine<> getTorrentsFiles(QJsonArray ids);
        Coroutine<> getTorrentsPeers(QJsonArray ids);
        Coroutine<> renameTorrentFileImpl(int torrentId, QString filePath, QString newName);
        Coroutine<std::optional<qint64>> getDownloadDirFreeSpaceImpl();
        Coroutine<std::optional<qint64>> getDownloadDirFreeSpaceLegacyMethod();
        Coroutine<std::optional<qint64>> getFreeSpaceForPathImpl(QString path);
        Coroutine<> requestFreeSpaceForPathImpl(
            QString path, QPointer<QObject> context, std::function<void(std::optional<qint64>)> callback
        );
        Coroutine<> requestArchiveImpl(
            QJsonObject arguments, QPointer<QObject> context, std::function<void(ArchiveResponse)> callback
        );
        void requestScriptMethod(
            QLatin1String method, QJsonObject arguments, QObject* context, std::function<void(ScriptResponse)> callback
        );
        Coroutine<> requestScriptMethodImpl(
            QLatin1String method,
            QJsonObject arguments,
            QPointer<QObject> context,
            std::function<void(ScriptResponse)> callback
        );
        Coroutine<> shutdownServerImpl();

        Coroutine<> getServerSettings();
        Coroutine<> getTorrents();
        Coroutine<> getServerStats();

        Coroutine<> connectAndPerformDataUpdates();
        Coroutine<> updateData();

        Coroutine<> checkIfServerIsLocal();

        void onRequestFailed(RpcError error, const QString& errorMessage, const QString& detailedErrorMessage);
        Coroutine<> autoReconnect();

        impl::RequestRouter* mRequestRouter{};
        CoroutineScope mBackgroundRequestsCoroutineScope{};
        CoroutineScope mAutoReconnectCoroutineScope{};
        CoroutineScope mDeletingFilesCoroutineScope{};

        bool mAutoReconnectEnabled{};

        std::optional<bool> mServerIsLocal{};
        QByteArray mGetTorrentsRequestData{};

        std::chrono::seconds mUpdateInterval{};
        std::chrono::seconds mAutoReconnectInterval{};

        ServerSettings* mServerSettings{};
        std::vector<std::unique_ptr<Torrent>> mTorrents{};
        ServerStats* mServerStats{};
        std::unique_ptr<ScriptJobsManager> mScriptJobsManager{};

        Status mStatus{};

    signals:
        void aboutToDisconnect();
        void statusChanged();
        void connectedChanged();
        void connectionStateChanged();

        void onAboutToRemoveTorrents(size_t first, size_t last);
        void onRemovedTorrents(size_t first, size_t last);
        void onChangedTorrents(size_t first, size_t last);
        void onAboutToAddTorrents(size_t count);
        void onAddedTorrents(size_t count);

        void torrentsUpdated();

        void torrentAdded(tremotesf::Torrent* torrent);
        void torrentFinished(tremotesf::Torrent* torrent);

        void torrentAddDuplicate();
        void torrentAddError(const QString& filePathOrUrl);

        void torrentFileRenameFailed(const QString& path, const QString& error);
    };
}

#endif // TREMOTESF_RPC_RPC_H
