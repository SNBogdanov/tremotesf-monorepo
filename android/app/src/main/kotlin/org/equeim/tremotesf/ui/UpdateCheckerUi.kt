// SPDX-FileCopyrightText: 2026 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import kotlinx.coroutines.launch
import org.equeim.tremotesf.R

@Composable
fun CheckForUpdatesButton(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var checking by remember { mutableStateOf(false) }
    var downloading by remember { mutableStateOf(false) }
    var update by remember { mutableStateOf<UpdateChecker.UpdateInfo?>(null) }
    var upToDate by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }

    OutlinedButton(
        onClick = {
            checking = true
            scope.launch {
                try {
                    val result = UpdateChecker.checkForUpdate()
                    if (result == null) {
                        upToDate = true
                    } else {
                        update = result
                    }
                } catch (e: Exception) {
                    error = context.getString(R.string.update_check_failed, e.message.orEmpty())
                } finally {
                    checking = false
                }
            }
        },
        enabled = !checking && !downloading,
        modifier = modifier.fillMaxWidth()
    ) {
        Text(stringResource(R.string.check_for_updates))
    }

    if (checking) {
        AlertDialog(
            onDismissRequest = {},
            title = { Text(stringResource(R.string.check_for_updates)) },
            text = {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingSmall),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    CircularProgressIndicator()
                    Text(stringResource(R.string.checking_for_updates))
                }
            },
            confirmButton = {}
        )
    }

    if (downloading) {
        AlertDialog(
            onDismissRequest = {},
            title = { Text(stringResource(R.string.update_downloading)) },
            text = {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.Center,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    CircularProgressIndicator()
                }
            },
            confirmButton = {}
        )
    }

    if (upToDate) {
        AlertDialog(
            onDismissRequest = { upToDate = false },
            title = { Text(stringResource(R.string.check_for_updates)) },
            text = { Text(stringResource(R.string.update_up_to_date)) },
            confirmButton = {
                TextButton(onClick = { upToDate = false }) {
                    Text(stringResource(android.R.string.ok))
                }
            }
        )
    }

    error?.let { message ->
        AlertDialog(
            onDismissRequest = { error = null },
            title = { Text(stringResource(R.string.check_for_updates)) },
            text = { Text(message) },
            confirmButton = {
                TextButton(onClick = { error = null }) {
                    Text(stringResource(android.R.string.ok))
                }
            }
        )
    }

    update?.let { info ->
        AlertDialog(
            onDismissRequest = { update = null },
            title = { Text(stringResource(R.string.update_available_title)) },
            text = { Text(stringResource(R.string.update_available_message, info.version)) },
            confirmButton = {
                TextButton(
                    onClick = {
                        update = null
                        downloading = true
                        scope.launch {
                            val file = try {
                                UpdateChecker.downloadApk(context, info)
                            } catch (e: Exception) {
                                error = context.getString(R.string.update_download_failed, e.message.orEmpty())
                                downloading = false
                                return@launch
                            }
                            try {
                                UpdateChecker.install(context, file)
                            } catch (e: Exception) {
                                error = context.getString(R.string.update_install_failed, e.message.orEmpty())
                            } finally {
                                downloading = false
                            }
                        }
                    }
                ) {
                    Text(stringResource(R.string.install))
                }
            },
            dismissButton = {
                TextButton(onClick = { update = null }) {
                    Text(stringResource(android.R.string.cancel))
                }
            }
        )
    }
}
