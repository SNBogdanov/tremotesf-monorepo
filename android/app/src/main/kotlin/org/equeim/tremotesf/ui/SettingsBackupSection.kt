// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import kotlinx.coroutines.launch
import org.equeim.tremotesf.R
import org.equeim.tremotesf.ui.components.TremotesfSectionHeader
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

@Composable
internal fun SettingsBackupSection(horizontalPadding: Dp) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var message by remember { mutableStateOf<String?>(null) }

    val exportLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/json")
    ) { uri ->
        if (uri != null) {
            scope.launch {
                message = SettingsBackup.export(context, uri).fold(
                    onSuccess = { context.getString(R.string.backup_export_success) },
                    onFailure = {
                        context.getString(R.string.backup_error, it.message ?: it.toString())
                    },
                )
            }
        }
    }

    val importLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            scope.launch {
                message = SettingsBackup.restore(context, uri).fold(
                    onSuccess = { context.getString(R.string.backup_restore_success) },
                    onFailure = {
                        context.getString(R.string.backup_error, it.message ?: it.toString())
                    },
                )
            }
        }
    }

    TremotesfSectionHeader(
        text = R.string.backup,
        modifier = Modifier
            .padding(horizontal = horizontalPadding)
            .padding(top = Dimens.SpacingSmall)
    )

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = horizontalPadding),
        horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingSmall)
    ) {
        OutlinedButton(
            onClick = { exportLauncher.launch(defaultBackupFileName()) },
            modifier = Modifier.weight(1f)
        ) {
            Text(stringResource(R.string.backup_export))
        }
        OutlinedButton(
            onClick = {
                importLauncher.launch(
                    arrayOf("application/json", "text/plain", "application/octet-stream")
                )
            },
            modifier = Modifier.weight(1f)
        ) {
            Text(stringResource(R.string.backup_restore))
        }
    }

    Text(
        stringResource(R.string.backup_warning),
        style = MaterialTheme.typography.bodyMedium,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(horizontal = horizontalPadding)
    )

    message?.let { text ->
        AlertDialog(
            onDismissRequest = { message = null },
            text = { Text(text) },
            confirmButton = {
                TextButton(onClick = { message = null }) {
                    Text(stringResource(android.R.string.ok))
                }
            }
        )
    }
}

private fun defaultBackupFileName(): String =
    "tremotesf-settings-${LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss"))}.json"
