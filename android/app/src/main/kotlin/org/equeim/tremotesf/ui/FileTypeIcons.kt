// SPDX-FileCopyrightText: 2025 The Android Open Source Project
//
// SPDX-License-Identifier: Apache-2.0

package org.equeim.tremotesf.ui

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.materialIcon
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.addPathNodes

object FileTypeIcons {
    private fun icon(name: String, pathData: String): ImageVector =
        materialIcon(name = name) {
            addPath(pathData = addPathNodes(pathData), fill = SolidColor(Color.Black))
        }

    private val video: ImageVector = icon(
        "Filled.Movie",
        "M18 4l2 4h-3l-2-4h-2l2 4h-3l-2-4H8l2 4H7L5 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V4h-4z"
    )

    private val audio: ImageVector = icon(
        "Filled.MusicNote",
        "M12 3v10.55c-.59-.34-1.27-.55-2-.55-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4V7h4V3h-6z"
    )

    private val image: ImageVector = icon(
        "Filled.Image",
        "M21 19V5c0-1.1-.9-2-2-2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2zM8.5 13.5l2.5 3.01L14.5 12l4.5 6H5l3.5-4.5z"
    )

    private val archive: ImageVector = icon(
        "Filled.Archive",
        "M20.54 5.23l-1.39-1.68C18.88 3.21 18.47 3 18 3H6c-.47 0-.88.21-1.16.55L3.46 5.23C3.17 5.57 3 6.02 3 6.5V19c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V6.5c0-.48-.17-.93-.46-1.27zM12 17.5L6.5 12H10v-2h4v2h3.5L12 17.5zM5.12 5l.81-1h12l.94 1H5.12z"
    )

    private val document: ImageVector = icon(
        "Filled.Description",
        "M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"
    )

    private val spreadsheet: ImageVector = icon(
        "Filled.TableChart",
        "M10 10.02h5V21h-5zM17 21h3c1.1 0 2-.9 2-2v-9h-5v11zm3-18H5c-1.1 0-2 .9-2 2v3h19V5c0-1.1-.9-2-2-2zM3 19c0 1.1.9 2 2 2h3V10H3v9z"
    )

    private val presentation: ImageVector = icon(
        "Filled.Slideshow",
        "M10 8v8l5-4-5-4zm9-5H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H5V5h14v14z"
    )

    private val code: ImageVector = icon(
        "Filled.Code",
        "M9.4 16.6L4.8 12l4.6-4.6L8 6l-6 6 6 6 1.4-1.4zm5.2 0l4.6-4.6-4.6-4.6L16 6l6 6-6 6-1.4-1.4z"
    )

    private val pdf: ImageVector = icon(
        "Filled.PictureAsPdf",
        "M20 2H8c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm-8.5 7.5c0 .83-.67 1.5-1.5 1.5H9v2H7.5V7H10c.83 0 1.5.67 1.5 1.5v1zm5 2c0 .83-.67 1.5-1.5 1.5h-2.5V7H15c.83 0 1.5.67 1.5 1.5v3zm4-3H19v1h1.5V11H19v2h-1.5V7h3v1.5zM9 9.5h1v-1H9v1zM4 6H2v14c0 1.1.9 2 2 2h14v-2H4V6zm10 5.5h1v-3h-1v3z"
    )

    private val videoExtensions = setOf(
        "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg",
        "ts", "m2ts", "vob", "3gp", "ogv", "divx", "rmvb", "asf"
    )
    private val audioExtensions = setOf(
        "mp3", "flac", "wav", "aac", "ogg", "m4a", "opus", "wma", "ape",
        "aiff", "alac", "mid", "midi", "mka"
    )
    private val imageExtensions = setOf(
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif",
        "svg", "ico", "heic", "heif", "avif", "raw", "psd"
    )
    private val archiveExtensions = setOf(
        "zip", "rar", "7z", "tar", "gz", "bz2", "xz", "zst",
        "iso", "cab", "lz", "lzma", "arj", "tgz", "tbz2", "txz"
    )
    private val documentExtensions = setOf(
        "txt", "log", "nfo", "srt", "sub", "ass", "ssa", "cue", "md",
        "doc", "docx", "odt", "rtf", "pages", "wps", "tex"
    )
    private val spreadsheetExtensions = setOf("xls", "xlsx", "ods", "csv", "tsv", "numbers")
    private val presentationExtensions = setOf("ppt", "pptx", "odp", "key")
    private val codeExtensions = setOf(
        "c", "h", "cpp", "cxx", "hpp", "cc", "hh", "cs", "java", "py", "js", "jsx",
        "ts", "tsx", "json", "yaml", "yml", "xml", "html", "htm", "css", "scss",
        "less", "sh", "bash", "bat", "cmd", "ps1", "php", "rb", "go", "rs", "pl",
        "lua", "swift", "kt", "kts", "sql", "ini", "cfg", "toml", "diff", "patch"
    )

    fun fileIcon(fileName: String): ImageVector {
        val extension = fileName.substringAfterLast('.', "").lowercase()
        return when {
            extension in videoExtensions -> video
            extension in audioExtensions -> audio
            extension in imageExtensions -> image
            extension in archiveExtensions -> archive
            extension in documentExtensions -> document
            extension in spreadsheetExtensions -> spreadsheet
            extension in presentationExtensions -> presentation
            extension in codeExtensions -> code
            extension == "pdf" -> pdf
            else -> Icons.AutoMirrored.Filled.InsertDriveFile
        }
    }
}
