// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.torrentslist

import android.content.Context
import androidx.core.content.edit
import org.equeim.tremotesf.TremotesfApplication
import org.equeim.tremotesf.rpc.Server

object PerServerFilters {
    private val preferences = TremotesfApplication.instance.getSharedPreferences(
        "${TremotesfApplication.instance.packageName}_per_server_filters",
        Context.MODE_PRIVATE
    )

    private fun key(server: Server) = "${server.name}|${server.address}"

    fun label(server: Server): String = preferences.getString("label.${key(server)}", "") ?: ""

    fun setLabel(server: Server, value: String) {
        preferences.edit { putString("label.${key(server)}", value) }
    }

    fun tracker(server: Server): String = preferences.getString("tracker.${key(server)}", "") ?: ""

    fun setTracker(server: Server, value: String) {
        preferences.edit { putString("tracker.${key(server)}", value) }
    }

    fun directory(server: Server): String = preferences.getString("directory.${key(server)}", "") ?: ""

    fun setDirectory(server: Server, value: String) {
        preferences.edit { putString("directory.${key(server)}", value) }
    }
}
