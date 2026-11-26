// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_SCREENS_SERVERSETTINGS_SCRIPTPARAMETERSPAGE_H
#define TREMOTESF_UI_SCREENS_SERVERSETTINGS_SCRIPTPARAMETERSPAGE_H

#include <functional>

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace tremotesf {
    class Rpc;
    class ScriptParametersForm;

    class ScriptParametersPage final : public QWidget {
        Q_OBJECT

    public:
        explicit ScriptParametersPage(Rpc* rpc, QWidget* parent = nullptr);

        [[nodiscard]] bool isAvailable() const;
        void load();
        [[nodiscard]] bool hasChanges() const;
        void save(std::function<void(bool success, const QString& error)> callback);

    private:
        struct ScriptForm {
            QString id;
            QString name;
            ScriptParametersForm* form{};
            QLabel* warning{};
            QJsonObject initial{};
        };

        void clearForms();
        void setStatus(const QString& text);

        Rpc* mRpc;
        QVBoxLayout* mFormsLayout{};
        QLabel* mStatusLabel{};
        QString mRevision;
        QList<ScriptForm> mForms;
    };
}

#endif // TREMOTESF_UI_SCREENS_SERVERSETTINGS_SCRIPTPARAMETERSPAGE_H
