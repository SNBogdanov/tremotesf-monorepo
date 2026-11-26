// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.serversettings

import androidx.annotation.StringRes
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ListItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import kotlinx.coroutines.launch
import kotlinx.parcelize.Parcelize
import org.equeim.tremotesf.R
import org.equeim.tremotesf.rpc.GlobalRpcClient
import org.equeim.tremotesf.rpc.requests.restartDaemon
import org.equeim.tremotesf.ui.Destination
import org.equeim.tremotesf.ui.NavController
import org.equeim.tremotesf.ui.ScreenPreview
import org.equeim.tremotesf.ui.components.TremotesfTopAppBar

@Parcelize
data object ServerSettingsDestination : Destination {
    @Composable
    override fun Content(navController: NavController) {
        ServerSettingsScreen(
            navigateUp = navController::popBackStack,
            navigateTo = navController::navigateTo
        )
    }
}

@Composable
private fun ServerSettingsScreen(navigateUp: () -> Unit, navigateTo: (Destination) -> Unit) {
    val capabilities = GlobalRpcClient.serverCapabilities
    val snackbarHostState = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    var showRestartDialog by rememberSaveable { mutableStateOf(false) }

    Scaffold(
        snackbarHost = { SnackbarHost(snackbarHostState) },
        topBar = {
            TremotesfTopAppBar(
                title = stringResource(R.string.server_settings),
                navigateUp = navigateUp,
            )
        }
    ) { innerPadding ->
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .consumeWindowInsets(innerPadding)
                .padding(innerPadding)
        ) {
            PageListItem(R.string.server_settings_downloading) { navigateTo(ServerSettingsDownloadingDestination) }
            PageListItem(R.string.server_settings_seeding) { navigateTo(ServerSettingsSeedingDestination) }
            PageListItem(R.string.server_settings_queue) { navigateTo(ServerSettingsQueueDestination) }
            PageListItem(R.string.server_settings_speed) { navigateTo(ServerSettingsSpeedDestination) }
            PageListItem(R.string.server_settings_network) { navigateTo(ServerSettingsNetworkDestination) }

            if (capabilities?.supportsScriptRunnerSettings == true &&
                capabilities?.scriptRunner?.scripts?.any { it.hasHostParameters() } == true
            ) {
                PageListItem(R.string.script_parameters) { navigateTo(ServerSettingsScriptsDestination) }
            }

            if (capabilities?.supportsScriptRestartDaemon == true) {
                PageListItem(R.string.restart_daemon) { showRestartDialog = true }
            }
        }
    }

    if (showRestartDialog) {
        AlertDialog(
            onDismissRequest = { showRestartDialog = false },
            title = { Text(stringResource(R.string.restart_daemon)) },
            text = { Text(stringResource(R.string.restart_daemon_confirm)) },
            confirmButton = {
                TextButton(onClick = {
                    showRestartDialog = false
                    scope.launch {
                        try {
                            GlobalRpcClient.restartDaemon()
                            snackbarHostState.showSnackbar(context.getString(R.string.restart_daemon_scheduled))
                        } catch (e: Exception) {
                            snackbarHostState.showSnackbar(
                                context.getString(R.string.restart_daemon_failed, e.message ?: e.toString())
                            )
                        }
                    }
                }) {
                    Text(stringResource(R.string.restart_daemon))
                }
            },
            dismissButton = {
                TextButton(onClick = { showRestartDialog = false }) {
                    Text(stringResource(R.string.script_cancel))
                }
            }
        )
    }
}

@Composable
private fun PageListItem(@StringRes title: Int, onClick: () -> Unit) {
    ListItem(
        headlineContent = {
            Text(stringResource(title))
        },
        modifier = Modifier.clickable(onClick = onClick)
    )
}

@Preview
@Composable
private fun ServerSettingsScreenPreview() = ScreenPreview {
    ServerSettingsScreen(
        navigateUp = {},
        navigateTo = {}
    )
}
