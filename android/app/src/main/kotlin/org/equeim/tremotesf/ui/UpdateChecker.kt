// SPDX-FileCopyrightText: 2026 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.Serializable
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.json.Json
import org.equeim.tremotesf.BuildConfig
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

object UpdateChecker {
    private const val MANIFEST_URL =
        "https://github.com/SNBogdanov/tremotesf-monorepo/releases/latest/download/update.json"
    private const val DOWNLOAD_URL_PREFIX =
        "https://github.com/SNBogdanov/tremotesf-monorepo/releases/download/"

    private val json = Json { ignoreUnknownKeys = true }

    data class UpdateInfo(
        val version: String,
        val versionCode: Int,
        val url: String,
        val sha256: String,
    )

    @Serializable
    private data class Manifest(
        val version: String,
        val android: AndroidAsset? = null,
    )

    @Serializable
    private data class AndroidAsset(
        val versionCode: Int,
        val url: String,
        val sha256: String,
    )

    suspend fun checkForUpdate(): UpdateInfo? = withContext(Dispatchers.IO) {
        val text = openStream(MANIFEST_URL).bufferedReader().use { it.readText() }
        val manifest = json.decodeFromString<Manifest>(text)
        val asset = manifest.android ?: return@withContext null
        if (asset.versionCode <= BuildConfig.VERSION_CODE) {
            return@withContext null
        }
        if (!asset.url.startsWith(DOWNLOAD_URL_PREFIX) || asset.sha256.isBlank()) {
            throw IOException("Invalid update manifest")
        }
        UpdateInfo(manifest.version, asset.versionCode, asset.url, asset.sha256)
    }

    suspend fun downloadApk(context: Context, info: UpdateInfo): File = withContext(Dispatchers.IO) {
        val directory = File(context.cacheDir, "updates").apply { mkdirs() }
        val file = File(directory, "tremotesf-${info.version}.apk")
        val digest = MessageDigest.getInstance("SHA-256")
        openStream(info.url).use { input ->
            file.outputStream().use { output ->
                val buffer = ByteArray(64 * 1024)
                while (true) {
                    val read = input.read(buffer)
                    if (read < 0) break
                    digest.update(buffer, 0, read)
                    output.write(buffer, 0, read)
                }
            }
        }
        val actual = digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xff) }
        if (!actual.equals(info.sha256, ignoreCase = true)) {
            file.delete()
            throw IOException("Update file checksum mismatch")
        }
        file
    }

    fun install(context: Context, file: File) {
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", file)
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        try {
            context.startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            throw IOException("No application installer found", e)
        }
    }

    private fun openStream(url: String): InputStream {
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.connectTimeout = 15000
        connection.readTimeout = 30000
        connection.requestMethod = "GET"
        connection.setRequestProperty("User-Agent", "Tremotesf")
        val code = connection.responseCode
        if (code != HttpURLConnection.HTTP_OK) {
            connection.disconnect()
            throw IOException("HTTP $code")
        }
        return object : InputStream() {
            private val input = connection.inputStream

            override fun read(): Int = input.read()

            override fun read(b: ByteArray, off: Int, len: Int): Int = input.read(b, off, len)

            override fun close() {
                try {
                    input.close()
                } finally {
                    connection.disconnect()
                }
            }
        }
    }
}
