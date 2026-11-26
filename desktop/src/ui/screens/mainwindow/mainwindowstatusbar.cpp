// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindowstatusbar.h"

#include <ranges>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QShowEvent>
#include <QStyleOption>
#include <QToolButton>
#include <QWindow>

#include "desktoputils.h"
#include "rpc/serversettings.h"
#include "rpc/serverstats.h"
#include "rpc/servers.h"
#include "rpc/rpc.h"
#include "formatutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    MainWindowStatusBar::MainWindowStatusBar(const Rpc* rpc, QAction* altSpeedLimitsAction, QWidget* parent)
        : QStatusBar(parent), mRpc(rpc) {
        setSizeGripEnabled(false);

        auto container = new QWidget(this);
        addPermanentWidget(container, 1);

        mLayout = new QHBoxLayout(container);
        // Set top/bottom margins to zero so that separators can be full-height.
        mLayout->setContentsMargins(8, 0, 8, 0);

        mServerLabel = new QLabel(this);
        mLayout->addWidget(mServerLabel);

        mFirstSeparator = new StatusBarSeparator(this);
        mLayout->addWidget(mFirstSeparator);

        mStatusLabel = new QLabel(this);
        mLayout->addWidget(mStatusLabel);

        mServerSpacer = new QWidget(this);
        mServerSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        mLayout->addWidget(mServerSpacer);

        mAltSpeedLimitsButton = new QToolButton(this);
        mAltSpeedLimitsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        mAltSpeedLimitsAction = altSpeedLimitsAction;
        mAltSpeedLimitsButton->setDefaultAction(mAltSpeedLimitsAction);
        mLayout->addWidget(mAltSpeedLimitsButton);
        const auto updateAltSpeedLimitsButton = [this](bool enabled) {
            {
                const QSignalBlocker blocker(mAltSpeedLimitsAction);
                mAltSpeedLimitsAction->setChecked(enabled);
            }
            mAltSpeedLimitsAction->setText(
                enabled ? qApp->translate("tremotesf", "Alternative speed limits are enabled. Click to disable")
                        : qApp->translate("tremotesf", "Alternative speed limits are disabled. Click to enable")
            );
            updateAltSpeedLimitIcon();
        };
        updateAltSpeedLimitsButton(mRpc->serverSettings()->data().alternativeSpeedLimitsEnabled);
        QObject::connect(mRpc->serverSettings(), &ServerSettings::changed, this, [this, updateAltSpeedLimitsButton] {
            updateAltSpeedLimitsButton(mRpc->serverSettings()->data().alternativeSpeedLimitsEnabled);
        });
        QObject::connect(
            mAltSpeedLimitsAction,
            &QAction::toggled,
            this,
            [this, updateAltSpeedLimitsButton](bool checked) { updateAltSpeedLimitsButton(checked); }
        );

        mLayout->addWidget(new StatusBarSeparator(this));

        auto downloadSpeedIcon = new IconWidget(QIcon::fromTheme("go-down"_L1), this);
        downloadSpeedIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        mLayout->addWidget(downloadSpeedIcon);

        auto downloadSpeedLabel = new QLabel(this);
        mLayout->addWidget(downloadSpeedLabel);

        mLayout->addWidget(new StatusBarSeparator(this));

        auto uploadSpeedIcon = new IconWidget(QIcon::fromTheme("go-up"_L1), this);
        uploadSpeedIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        mLayout->addWidget(uploadSpeedIcon);

        auto uploadSpeedLabel = new QLabel(this);
        mLayout->addWidget(uploadSpeedLabel);

        mLayout->addWidget(new StatusBarSeparator(this));

        auto freeSpaceLabel = new QLabel(this);
        freeSpaceLabel->setContentsMargins(8, 0, 0, 0);
        mLayout->addWidget(freeSpaceLabel);

        mTemporaryFreeSpaceLabel = new QLabel(this);
        mTemporaryFreeSpaceLabel->setContentsMargins(12, 0, 0, 0);
        mTemporaryFreeSpaceLabel->hide();
        mLayout->addWidget(mTemporaryFreeSpaceLabel);

        updateLayout();
        QObject::connect(mRpc, &Rpc::connectionStateChanged, this, &MainWindowStatusBar::updateLayout);
        QObject::connect(Servers::instance(), &Servers::hasServersChanged, this, &MainWindowStatusBar::updateLayout);

        updateServerText();
        QObject::connect(
            Servers::instance(),
            &Servers::currentServerChanged,
            this,
            &MainWindowStatusBar::updateServerText
        );
        QObject::connect(
            Servers::instance(),
            &Servers::hasServersChanged,
            this,
            &MainWindowStatusBar::updateServerText
        );
        QObject::connect(Servers::instance(), &Servers::serversChanged, this, &MainWindowStatusBar::updateServerText);

        updateStatusLabels();
        QObject::connect(mRpc, &Rpc::statusChanged, this, &MainWindowStatusBar::updateStatusLabels);

        const auto updateSpeedAndFreeSpaceLabels = [=, this] {
            const auto& serverSettings = mRpc->serverSettings()->data();
            const auto formatLimit = [](int limit) -> QString {
                return limit > 0 ? formatutils::formatSpeedLimit(limit) : QStringLiteral("\u221E");
            };
            const int downloadLimit =
                serverSettings.alternativeSpeedLimitsEnabled
                    ? serverSettings.alternativeDownloadSpeedLimit
                    : (serverSettings.downloadSpeedLimited ? serverSettings.downloadSpeedLimit : 0);
            const int uploadLimit = serverSettings.alternativeSpeedLimitsEnabled
                                        ? serverSettings.alternativeUploadSpeedLimit
                                        : (serverSettings.uploadSpeedLimited ? serverSettings.uploadSpeedLimit : 0);
            downloadSpeedLabel->setText(
                formatutils::formatByteSpeed(mRpc->serverStats()->downloadSpeed())
                + u" ("_s
                + formatLimit(downloadLimit)
                + u')'
            );
            uploadSpeedLabel->setText(
                formatutils::formatByteSpeed(mRpc->serverStats()->uploadSpeed())
                + u" ("_s
                + formatLimit(uploadLimit)
                + u')'
            );
            const bool usingTemporaryDirectory =
                serverSettings.incompleteDirectoryEnabled
                && !serverSettings.incompleteDirectory.isEmpty()
                && serverSettings.incompleteDirectory != serverSettings.downloadDirectory;
            freeSpaceLabel->setText(qApp->translate("tremotesf", "Free space: %1")
                                        .arg(formatutils::formatByteSize(mRpc->serverStats()->freeSpace())));
            const auto incompleteDirFreeSpace = mRpc->serverStats()->incompleteDirFreeSpace();
            if (usingTemporaryDirectory && incompleteDirFreeSpace.has_value()) {
                mTemporaryFreeSpaceLabel->setText(qApp->translate("tremotesf", "Free space in temporary folder: %1")
                                                      .arg(formatutils::formatByteSize(*incompleteDirFreeSpace)));
                mTemporaryFreeSpaceLabel->show();
            } else {
                mTemporaryFreeSpaceLabel->hide();
            }
        };
        updateSpeedAndFreeSpaceLabels();
        QObject::connect(mRpc->serverStats(), &ServerStats::updated, this, updateSpeedAndFreeSpaceLabels);
        QObject::connect(mRpc->serverSettings(), &ServerSettings::changed, this, updateSpeedAndFreeSpaceLabels);
    }

    namespace {
        auto widgetsAfter(QWidget* widget, QLayout* layout) {
            return std::views::iota(layout->indexOf(widget) + 1, layout->count())
                   | std::views::transform([layout](int i) { return layout->itemAt(i)->widget(); })
                   | std::views::filter([](QWidget* w) { return w != nullptr; });
        }
    }

    bool MainWindowStatusBar::event(QEvent* event) {
        if (event->type() == QEvent::PaletteChange) {
            updateAltSpeedLimitIcon();
        }
        return QStatusBar::event(event);
    }

    void MainWindowStatusBar::updateLayout() {
        if (Servers::instance()->hasServers()) {
            mFirstSeparator->show();
            mStatusLabel->show();
            if (mRpc->connectionState() == Rpc::ConnectionState::Connected) {
                for (auto* widget : widgetsAfter(mServerSpacer, mLayout)) {
                    widget->show();
                }
            } else {
                for (auto* widget : widgetsAfter(mServerSpacer, mLayout)) {
                    widget->hide();
                }
            }
        } else {
            mFirstSeparator->hide();
            mStatusLabel->hide();
            for (auto* widget : widgetsAfter(mServerSpacer, mLayout)) {
                widget->hide();
            }
        }
    }

    void MainWindowStatusBar::updateServerText() {
        if (Servers::instance()->hasServers()) {
            mServerLabel->setText(Servers::instance()->currentServerName());
        } else {
            mServerLabel->setText(qApp->translate("tremotesf", "No servers"));
        }
    }

    void MainWindowStatusBar::updateStatusLabels() {
        mStatusLabel->setText(mRpc->status().toString());
        if (mRpc->error() != RpcError::NoError) {
            mStatusLabel->setToolTip(mRpc->errorMessage());
        } else {
            mStatusLabel->setToolTip({});
        }
    }

    void MainWindowStatusBar::updateAltSpeedLimitIcon() {
        const bool enabled = mAltSpeedLimitsAction->isChecked();
        mAltSpeedLimitsAction->setIcon(QIcon::fromTheme(enabled ? "tremotesf-slow-mode"_L1 : "tremotesf-fast-mode"_L1));
        mAltSpeedLimitsButton->setAutoRaise(!enabled);
    }

    StatusBarSeparator::StatusBarSeparator(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    QSize StatusBarSeparator::sizeHint() const {
        QStyleOption opt{};
        opt.initFrom(this);
        opt.state.setFlag(QStyle::State_Horizontal);
        const int extent = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent, &opt, this);
        return {extent, extent};
    }

    void StatusBarSeparator::paintEvent(QPaintEvent*) {
        QPainter p(this);
        QStyleOption opt{};
        opt.initFrom(this);
        opt.state.setFlag(QStyle::State_Horizontal);
        style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator, &opt, &p, this);
    }

    QSize IconWidget::sizeHint() const {
        const int size = desktoputils::iconSizeForDevicePixelRatio(devicePixelRatio());
        return {size, size};
    }

    bool IconWidget::event(QEvent* event) {
        switch (event->type()) {
        case QEvent::DevicePixelRatioChange:
        case QEvent::ScreenChangeInternal:
            updateIconSize();
            break;
        default:
            break;
        }
        return QWidget::event(event);
    }

    void IconWidget::showEvent(QShowEvent* event) {
        updateIconSize();
        if (!mScreenChangeConnected) {
            if (auto* window = windowHandle()) {
                QObject::connect(window, &QWindow::screenChanged, this, [this] { updateIconSize(); });
                mScreenChangeConnected = true;
            }
        }
        QWidget::showEvent(event);
    }

    void IconWidget::updateIconSize() {
        const int size = desktoputils::iconSizeForDevicePixelRatio(devicePixelRatio());
        setFixedSize(size, size);
    }

    void IconWidget::paintEvent(QPaintEvent*) {
        QPainter painter(this);
        mIcon.paint(&painter, contentsRect());
    }
}

#include "mainwindowstatusbar.moc"
