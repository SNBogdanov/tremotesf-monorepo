// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.scripts

import android.content.Context
import androidx.core.content.edit
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import org.equeim.tremotesf.TremotesfApplication

object ScriptParameterHistory {
    private val preferences = TremotesfApplication.instance.getSharedPreferences(
        "${TremotesfApplication.instance.packageName}_script_parameters",
        Context.MODE_PRIVATE
    )

    fun load(scriptId: String): Map<String, String> {
        val value = preferences.getString(scriptId, null) ?: return emptyMap()
        return runCatching { Json.decodeFromString<Map<String, String>>(value) }.getOrDefault(emptyMap())
    }

    fun save(scriptId: String, values: Map<String, String>) {
        preferences.edit {
            putString(scriptId, Json.encodeToString(values))
        }
    }
}
