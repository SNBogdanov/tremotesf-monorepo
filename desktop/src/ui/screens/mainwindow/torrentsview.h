// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTSVIEW_H
#define TREMOTESF_TORRENTSVIEW_H

#include <QTreeView>

class QEvent;
class QKeyEvent;
class QShowEvent;

namespace tremotesf {
    class TorrentsProxyModel;

    class TorrentsView final : public QTreeView {
        Q_OBJECT
    public:
        TorrentsView(TorrentsProxyModel* model, QWidget* parent = nullptr);
        ~TorrentsView() override = default;
        Q_DISABLE_COPY_MOVE(TorrentsView)

        void saveState();

    protected:
        bool event(QEvent* event) override;
        void changeEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:
        void updateIconSize();
        void updateAlternateRowColor();
        void ensureSelection();

        bool mScreenChangeConnected = false;
        int mRowToSelectAfterRemoval = -1;
    };
}

#endif // TREMOTESF_TORRENTSVIEW_H
