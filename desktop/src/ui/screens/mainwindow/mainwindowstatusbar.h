// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_MAINWINDOWSTATUSBAR_H
#define TREMOTESF_MAINWINDOWSTATUSBAR_H

#include <QStatusBar>

class QLabel;
class QToolButton;
class QAction;

namespace tremotesf {
    class Rpc;

    class StatusBarSeparator;

    class MainWindowStatusBar final : public QStatusBar {
        Q_OBJECT

    public:
        explicit MainWindowStatusBar(const Rpc* rpc, QAction* altSpeedLimitsAction, QWidget* parent = nullptr);

    protected:
        bool event(QEvent* event) override;

    private:
        void updateLayout();
        void updateServerText();
        void updateStatusLabels();
        void updateAltSpeedLimitIcon();

        const Rpc* mRpc{};
        QLayout* mLayout{};
        QLabel* mServerLabel{};
        QToolButton* mAltSpeedLimitsButton{};
        QWidget* mServerSpacer{};
        StatusBarSeparator* mFirstSeparator{};
        QLabel* mStatusLabel{};
        QLabel* mTemporaryFreeSpaceLabel{};
        QAction* mAltSpeedLimitsAction{};
    };

    class StatusBarSeparator final : public QWidget {
        Q_OBJECT
    public:
        explicit StatusBarSeparator(QWidget* parent = nullptr);
        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
    };

    class IconWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit IconWidget(QIcon icon, QWidget* parent = nullptr) : QWidget(parent), mIcon(std::move(icon)) {
            updateIconSize();
        }

        QSize sizeHint() const override;

    protected:
        bool event(QEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void paintEvent(QPaintEvent* event) override;

    private:
        void updateIconSize();

        QIcon mIcon;
        bool mScreenChangeConnected = false;
    };
}

#endif // TREMOTESF_MAINWINDOWSTATUSBAR_H
