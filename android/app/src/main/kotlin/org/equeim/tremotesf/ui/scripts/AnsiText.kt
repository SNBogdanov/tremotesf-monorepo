// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.scripts

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight

private val ANSI_PATTERN = Regex("\u001B\\[([0-9;]*)m")

fun ansiToAnnotatedString(text: String): AnnotatedString {
    if (!text.contains('\u001B')) {
        return AnnotatedString(text)
    }

    return buildAnnotatedString {
        var foreground: Color? = null
        var bold = false
        var last = 0

        fun appendStyled(part: String) {
            if (part.isEmpty()) return
            if (foreground == null && !bold) {
                append(part)
            } else {
                pushStyle(SpanStyle(color = foreground ?: Color.Unspecified, fontWeight = if (bold) FontWeight.Bold else null))
                append(part)
                pop()
            }
        }

        for (match in ANSI_PATTERN.findAll(text)) {
            appendStyled(text.substring(last, match.range.first))
            val codes = match.groupValues[1]
                .split(';')
                .filter { it.isNotEmpty() }
                .map { it.toIntOrNull() ?: 0 }
                .ifEmpty { listOf(0) }
            for (code in codes) {
                when (code) {
                    0 -> {
                        foreground = null
                        bold = false
                    }
                    1 -> bold = true
                    22 -> bold = false
                    in 30..37 -> foreground = standardColor(code - 30)
                    in 90..97 -> foreground = brightColor(code - 90)
                    39 -> foreground = null
                }
            }
            last = match.range.last + 1
        }
        appendStyled(text.substring(last))
    }
}

private fun standardColor(index: Int): Color = when (index) {
    0 -> Color(0xFF000000)
    1 -> Color(0xFFEF5350)
    2 -> Color(0xFF66BB6A)
    3 -> Color(0xFFFFCA28)
    4 -> Color(0xFF42A5F5)
    5 -> Color(0xFFAB47BC)
    6 -> Color(0xFF26C6DA)
    else -> Color(0xFFE0E0E0)
}

private fun brightColor(index: Int): Color = when (index) {
    0 -> Color(0xFF9E9E9E)
    1 -> Color(0xFFFF8A80)
    2 -> Color(0xFFA5D6A7)
    3 -> Color(0xFFFFE082)
    4 -> Color(0xFF90CAF9)
    5 -> Color(0xFFCE93D8)
    6 -> Color(0xFF80DEEA)
    else -> Color(0xFFFFFFFF)
}
