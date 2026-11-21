// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktoputils.h"

#include <optional>

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSize>
#include <QStringBuilder>
#include <QStyle>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QUrl>

#ifdef TREMOTESF_UNIX_FREEDESKTOP
#    include <QPointer>
#    include <KWindowSystem>
#    include <fmt/chrono.h>
#    include "coroutines/coroutines.h"
#    include "coroutines/qobjectsignal.h"
#    include "coroutines/scope.h"
#    include "coroutines/timer.h"
#    include "coroutines/waitall.h"
#endif

#include <fmt/format.h>

#include "log/log.h"
#include "rpc/pathutils.h"

SPECIALIZE_FORMATTER_FOR_QDEBUG(QUrl)

using namespace Qt::StringLiterals;

namespace {
    QIcon loadStatusIcon(QLatin1String name) {
        QIcon icon;
        for (const int size : {16, 24, 32, 48, 64, 96}) {
            icon.addFile(QStringLiteral(":/status/%1/%2.png").arg(size).arg(name), QSize(size, size));
        }
        return icon;
    }
}

namespace tremotesf::desktoputils {
    int iconSizeForDevicePixelRatio(qreal devicePixelRatio) {
        // 16x16 icons on a 100% screen, 24x24 on a 150% screen, 32x32 on a 200% screen, etc.
        return qMax(16, qRound(16.0 * devicePixelRatio));
    }

    const QIcon& statusIcon(StatusIcon icon) {
        using enum StatusIcon;
        switch (icon) {
        case ActiveDownload: {
            static const QIcon qicon = loadStatusIcon("active-download"_L1);
            return qicon;
        }
        case ActiveUpload: {
            static const QIcon qicon = loadStatusIcon("active-upload"_L1);
            return qicon;
        }
        case All: {
            static const QIcon qicon = loadStatusIcon("all"_L1);
            return qicon;
        }
        case CheckWaiting: {
            static const QIcon qicon = loadStatusIcon("check-waiting"_L1);
            return qicon;
        }
        case CriticalError: {
            static const QIcon qicon = QIcon::fromTheme("dialog-error"_L1, loadStatusIcon("critical-error"_L1));
            return qicon;
        }
        case Done: {
            static const QIcon qicon = loadStatusIcon("done"_L1);
            return qicon;
        }
        case Inactive: {
            static const QIcon qicon = loadStatusIcon("inactive"_L1);
            return qicon;
        }
        case QueuedForDownloading: {
            static const QIcon qicon = loadStatusIcon("queued-download"_L1);
            return qicon;
        }
        case QueuedForSeeding: {
            static const QIcon qicon = loadStatusIcon("queued-seed"_L1);
            return qicon;
        }
        case TrackerErrorDownloading: {
            static const QIcon qicon = loadStatusIcon("tracker-error-downloading"_L1);
            return qicon;
        }
        case TrackerErrorSeeding: {
            static const QIcon qicon = loadStatusIcon("tracker-error-seeding"_L1);
            return qicon;
        }
        case Waiting: {
            static const QIcon qicon = loadStatusIcon("waiting"_L1);
            return qicon;
        }
        case PausedDueToSpace: {
            static const QIcon qicon = loadStatusIcon("paused-due-to-space"_L1);
            return qicon;
        }
        case WaitingForSpaceToMove: {
            static const QIcon qicon = loadStatusIcon("waiting-to-move"_L1);
            return qicon;
        }
        case RunningScript: {
            static const QIcon qicon = loadStatusIcon("running-script"_L1);
            return qicon;
        }
        case Moving: {
            static const QIcon qicon = loadStatusIcon("moving"_L1);
            return qicon;
        }
        case Checking: {
            static const QIcon qicon = loadStatusIcon("checking"_L1);
            return qicon;
        }
        case Downloading: {
            static const QIcon qicon = loadStatusIcon("downloading"_L1);
            return qicon;
        }
        case Paused: {
            static const QIcon qicon = loadStatusIcon("paused"_L1);
            return qicon;
        }
        case Seeding: {
            static const QIcon qicon = loadStatusIcon("seeding"_L1);
            return qicon;
        }
        case Speedometer: {
            static const QIcon qicon = loadStatusIcon("speedometer"_L1);
            return qicon;
        }
        }

        throw std::logic_error(fmt::format("Unknown StatusIcon value {}", std::to_underlying(icon)));
    }

    const QIcon& priorityIcon(Priority priority) {
        switch (priority) {
        case Priority::High: {
            static const QIcon qicon = loadStatusIcon("priority-high"_L1);
            return qicon;
        }
        case Priority::Normal: {
            static const QIcon qicon = loadStatusIcon("priority-normal"_L1);
            return qicon;
        }
        case Priority::Low: {
            static const QIcon qicon = loadStatusIcon("priority-low"_L1);
            return qicon;
        }
        }
        throw std::logic_error(fmt::format("Unknown Priority value {}", std::to_underlying(priority)));
    }

    const QIcon& standardFileIcon() {
        static const auto icon = qApp->style()->standardIcon(QStyle::SP_FileIcon);
        return icon;
    }

    const QIcon& standardDirIcon() {
        static const auto icon = qApp->style()->standardIcon(QStyle::SP_DirIcon);
        return icon;
    }

    namespace {
        void showOpenFileError(const QUrl& url, std::optional<QString> error, QWidget* parent) {
            const auto pathToDisplay = url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.toString();
            auto dialog = new QMessageBox(
                QMessageBox::Warning,
                //: Dialog title
                qApp->translate("tremotesf", "Error"),
                //: File opening error, %1 is a file path
                qApp->translate("tremotesf", "Error opening %1").arg(pathToDisplay),
                QMessageBox::Close,
                parent
            );
            if (error.has_value()) {
                dialog->setText(dialog->text() % "\n\n"_L1 % *error);
            }
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        }

        void openFileImpl(const QUrl& url, QWidget* parent) {
            info().log("Executing QDesktopServices::openUrl() for {}", url);
            if (!QDesktopServices::openUrl(url)) {
                warning().log("QDesktopServices::openUrl() failed for {}", url);
                showOpenFileError(url, {}, parent);
            }
        }

#ifdef TREMOTESF_UNIX_FREEDESKTOP
        class DelayedUrlOpener : public QObject {
            Q_OBJECT
        public:
            using QObject::QObject;

            static DelayedUrlOpener* instance() {
                static const auto instance = new DelayedUrlOpener(qApp);
                return instance;
            }

            void openUrlAfterFocusWindowChange(QUrl url, QPointer<QWidget> parent) {
                mScope.launch(openUrlAfterFocusWindowChangeImpl(std::move(url), std::move(parent)));
            }

        private:
            Coroutine<> openUrlAfterFocusWindowChangeImpl(QUrl url, QPointer<QWidget> parent) {
                co_await waitAny(
                    []() -> Coroutine<> {
                        debug().log("Waiting for focusWindowChanged signal");
                        co_await waitForSignal(qApp, &QGuiApplication::focusWindowChanged);
                        debug().log("Received focusWindowChanged signal");
                    }(),
                    []() -> Coroutine<> {
                        using namespace std::chrono_literals;
                        constexpr auto timeout = 100ms;
                        co_await waitFor(timeout);
                        debug().log("Did not receive focusWindowChanged signal in {}", timeout);
                    }()
                );
                openFileImpl(url, parent);
            }

            CoroutineScope mScope;
        };
#endif
    }

    void openFile(const QString& filePath, QWidget* parent) {
        const auto parsedUrl = parsePathAsUrl(filePath);
        if (!parsedUrl.has_value() && !QFile::exists(filePath)) {
            warning().log("Can't open file {}, it does not exist", filePath);
            showOpenFileError(filePath, qApp->translate("tremotesf", "This file/directory does not exist"), parent);
            return;
        }
        auto url = parsedUrl.has_value() ? *parsedUrl : QUrl::fromLocalFile(filePath);
#ifdef TREMOTESF_UNIX_FREEDESKTOP
        // If focusWindow returns null in this moment (which is possible when we've just closed a context menu) then Qt will not pass activation token to launched app:
        // https://bugreports.qt.io/browse/QTBUG-138892
        // Wait for QGuiApplication::focusWindowChanged signal first so that token is requested and passed along
        if (KWindowSystem::isPlatformWayland() && !QGuiApplication::focusWindow()) {
            DelayedUrlOpener::instance()->openUrlAfterFocusWindowChange(std::move(url), parent);
        } else {
            openFileImpl(url, parent);
        }
#else
        openFileImpl(url, parent);
#endif
    }

    namespace {
        QRegularExpression urlRegex() {
            constexpr auto protocol = "(?:(?:[a-z]+:)?//)"_L1;
            constexpr auto host = R"((?:(?:[a-z\x{00a1}-\x{ffff0}-9][-_]*)*[a-z\x{00a1}-\x{ffff0}-9]+))";
            constexpr auto domain = R"((?:\.(?:[a-z\x{00a1}-\x{ffff0}-9]-*)*[a-z\x{00a1}-\x{ffff0}-9]+)*)";
            constexpr auto tld = R"((?:\.(?:[a-z\x{00a1}-\x{ffff}]{2,}))\.?)";
            constexpr auto port = R"((?::\d{2,5})?)";
            constexpr auto path = R"((?:[/?#][^\s"\)']*)?)";
            const auto regex =
                QString("(?:"_L1 % protocol % R"(|www\.)(?:)" % host % domain % tld % ")"_L1 % port % path);
            return QRegularExpression(regex, QRegularExpression::CaseInsensitiveOption);
        }
    }

    void findLinksAndAddAnchors(QTextDocument* document) {
        auto baseFormat = QTextCharFormat();
        baseFormat.setAnchor(true);
        baseFormat.setFontUnderline(true);
        baseFormat.setForeground(qApp->palette().link());
        const auto regex = desktoputils::urlRegex();
        auto cursor = QTextCursor();
        while (true) {
            cursor = document->find(regex, cursor);
            if (cursor.isNull()) {
                break;
            }
            auto format = baseFormat;
            format.setAnchorHref(cursor.selection().toPlainText());
            cursor.setCharFormat(format);
        }
    }
}

#include "desktoputils.moc"
