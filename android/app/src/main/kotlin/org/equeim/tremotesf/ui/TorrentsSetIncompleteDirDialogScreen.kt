// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.window.DialogProperties
import androidx.navigation3.runtime.metadata
import androidx.navigation3.scene.DialogSceneStrategy
import androidx.navigation3.scene.DialogSceneStrategy.Companion.DialogKey
import kotlinx.parcelize.Parcelize
import org.equeim.tremotesf.R
import org.equeim.tremotesf.rpc.GlobalRpcClient
import org.equeim.tremotesf.rpc.requests.torrentproperties.setTorrentIncompleteDirectory
import org.equeim.tremotesf.ui.components.DialogPadding
import org.equeim.tremotesf.ui.components.TremotesfAlertDialogContent
import org.equeim.tremotesf.ui.components.TremotesfSwitchWithText

@Parcelize
data class TorrentsSetIncompleteDirDialogDestination(
    val torrentHashString: String,
    val incompleteDirectory: String,
) : Destination {
    @Composable
    override fun Content(navController: NavController) {
        var directory by rememberSaveable { mutableStateOf(incompleteDirectory) }
        var moveData by rememberSaveable { mutableStateOf(false) }
        TremotesfAlertDialogContent(
            title = { Text(stringResource(R.string.set_incomplete_directory)) },
            text = {
                Column(
                    Modifier
                        .fillMaxWidth()
                        .padding(horizontal = DialogPadding),
                    verticalArrangement = Arrangement.spacedBy(DialogPadding)
                ) {
                    OutlinedTextField(
                        value = directory,
                        onValueChange = { directory = it },
                        label = { Text(stringResource(R.string.incomplete_directory)) },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                    TremotesfSwitchWithText(
                        checked = moveData,
                        text = R.string.move_incomplete_data,
                        onCheckedChange = { moveData = it },
                        horizontalContentPadding = DialogPadding,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
            },
            applyHorizontalPaddingToText = false,
            buttons = {
                TextButton(onClick = navController::popBackStack) {
                    Text(stringResource(android.R.string.cancel))
                }
                TextButton(
                    enabled = directory.isNotBlank(),
                    onClick = {
                        val newDirectory = directory
                        GlobalRpcClient.performBackgroundRpcRequest(R.string.torrent_set_incomplete_dir_error) {
                            setTorrentIncompleteDirectory(torrentHashString, newDirectory, moveData)
                        }
                        navController.popBackStack()
                    }
                ) {
                    Text(stringResource(android.R.string.ok))
                }
            }
        )
    }

    override val metadata: Map<String, Any>
        get() = metadata { put(DialogKey, DialogProperties()) }
}
