// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_UI_SCRIPTS_SCRIPTSMENU_H
#define TREMOTESF_UI_SCRIPTS_SCRIPTSMENU_H

#include "rpc/scriptrunner.h"

class QMenu;
class QWidget;

namespace tremotesf {
    class Rpc;
}

namespace tremotesf::scripts {
    [[nodiscard]] QString scriptErrorMessage(const QString& result);

    void populateScriptsMenu(
        QMenu* menu, Rpc* rpc, ScriptContext context, const ScriptSelection& selection, QWidget* parent
    );
}

#endif // TREMOTESF_UI_SCRIPTS_SCRIPTSMENU_H
