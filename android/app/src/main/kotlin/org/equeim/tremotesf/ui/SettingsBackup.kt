// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.floatOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.longOrNull
import org.equeim.tremotesf.rpc.GlobalServers
import org.equeim.tremotesf.rpc.Servers

object SettingsBackup {
    private const val CURRENT_VERSION = 1

    private val json = Json {
        prettyPrint = true
        ignoreUnknownKeys = true
        encodeDefaults = true
    }

    @Serializable
    private data class BackupFile(
        val version: Int = CURRENT_VERSION,
        val servers: Servers.ServersState? = null,
        val preferences: Map<String, PreferenceValue> = emptyMap(),
    )

    @Serializable
    private data class PreferenceValue(
        val type: String,
        val value: JsonElement,
    )

    suspend fun export(context: Context, uri: Uri): Result<Unit> = withContext(Dispatchers.IO) {
        runCatching {
            val backup = BackupFile(
                servers = GlobalServers.serversState.value,
                preferences = Settings.snapshotPreferences().mapValues { (_, value) ->
                    value.toPreferenceValue()
                },
            )
            val data = json.encodeToString(backup).toByteArray()
            context.contentResolver.openOutputStream(uri, "wt")?.use { stream ->
                stream.write(data)
            } ?: error("Cannot open output file")
        }
    }

    suspend fun restore(context: Context, uri: Uri): Result<Unit> = withContext(Dispatchers.IO) {
        runCatching {
            val data = context.contentResolver.openInputStream(uri)?.use { stream ->
                stream.reader().readText()
            } ?: error("Cannot open input file")
            val backup = json.decodeFromString<BackupFile>(data)
            require(backup.version == CURRENT_VERSION) {
                "Unsupported backup version ${backup.version}"
            }
            backup.servers?.let(GlobalServers::restoreServersState)
            Settings.replacePreferences(
                backup.preferences.mapValues { (_, value) -> value.toAny() }
            )
        }
    }

    private fun Any?.toPreferenceValue(): PreferenceValue = when (this) {
        is Boolean -> PreferenceValue("boolean", JsonPrimitive(this))
        is Int -> PreferenceValue("int", JsonPrimitive(this))
        is Long -> PreferenceValue("long", JsonPrimitive(this))
        is Float -> PreferenceValue("float", JsonPrimitive(this))
        is String -> PreferenceValue("string", JsonPrimitive(this))
        is Set<*> -> {
            val values = this
            PreferenceValue(
                "string_set",
                buildJsonArray { values.forEach { add(JsonPrimitive(it.toString())) } },
            )
        }
        else -> PreferenceValue("string", JsonPrimitive(toString()))
    }

    private fun PreferenceValue.toAny(): Any? = when (type) {
        "boolean" -> value.jsonPrimitive.booleanOrNull
        "int" -> value.jsonPrimitive.intOrNull
        "long" -> value.jsonPrimitive.longOrNull
        "float" -> value.jsonPrimitive.floatOrNull
        "string" -> value.jsonPrimitive.contentOrNull
        "string_set" -> value.jsonArray.mapNotNull { it.jsonPrimitive.contentOrNull }.toSet()
        else -> null
    }
}
