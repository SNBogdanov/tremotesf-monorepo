// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.serversettings

import android.app.Application
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.ReceiveChannel
import kotlinx.coroutines.flow.StateFlow
import kotlinx.parcelize.Parcelize
import org.equeim.tremotesf.R
import org.equeim.tremotesf.rpc.GlobalRpcClient
import org.equeim.tremotesf.rpc.RpcClient
import org.equeim.tremotesf.rpc.RpcRequestState
import org.equeim.tremotesf.rpc.performRecoveringRequestIntoStateFlow
import org.equeim.tremotesf.rpc.requests.serversettings.SeedingServerSettings
import org.equeim.tremotesf.rpc.requests.serversettings.getSeedingServerSettings
import org.equeim.tremotesf.rpc.requests.serversettings.setServerIdleSeedingLimit
import org.equeim.tremotesf.rpc.requests.serversettings.setServerIdleSeedingLimited
import org.equeim.tremotesf.rpc.requests.serversettings.setServerRatioLimit
import org.equeim.tremotesf.rpc.requests.serversettings.setServerRatioLimited
import org.equeim.tremotesf.rpc.requests.serversettings.setScriptTorrentDoneRemoveAllFilesEnabled
import org.equeim.tremotesf.rpc.requests.serversettings.setScriptTorrentDoneRemoveTempFilesEnabled
import org.equeim.tremotesf.rpc.requests.serversettings.setScriptTorrentDoneRemoveTorrentEnabled
import org.equeim.tremotesf.ui.Destination
import org.equeim.tremotesf.ui.NavController
import org.equeim.tremotesf.ui.ScreenPreview
import org.equeim.tremotesf.ui.components.NON_NEGATIVE_DECIMALS_RANGE
import org.equeim.tremotesf.ui.components.TremotesfDecimalNumberInputFieldState
import org.equeim.tremotesf.ui.components.TremotesfIntegerNumberInputFieldState
import org.equeim.tremotesf.ui.components.TremotesfNumberInputField
import org.equeim.tremotesf.ui.components.TremotesfSwitchWithText
import org.equeim.tremotesf.ui.components.UNSIGNED_16BIT_RANGE
import org.equeim.tremotesf.ui.components.rememberTremotesfDecimalNumberInputFieldState
import org.equeim.tremotesf.ui.components.rememberTremotesfIntegerNumberInputFieldState
import kotlin.time.Duration.Companion.minutes

@Parcelize
data object ServerSettingsSeedingDestination : Destination {
    @Composable
    override fun Content(navController: NavController) {
        val model = viewModel<SeedingScreenViewModel>()
        ServerSettingsSeedingScreen(
            settingsRequestState = model.settings.collectAsStateWithLifecycle(),
            navigateUp = navController::popBackStack,
            ratioLimited = model.ratioLimited,
            ratioLimit = model.ratioLimit,
            idleSeedingLimited = model.idleSeedingLimited,
            idleSeedingLimit = model.idleSeedingLimit,
            scriptTorrentDoneRemoveTorrentEnabled = model.scriptTorrentDoneRemoveTorrentEnabled,
            scriptTorrentDoneRemoveTempFilesEnabled = model.scriptTorrentDoneRemoveTempFilesEnabled,
            scriptTorrentDoneRemoveAllFilesEnabled = model.scriptTorrentDoneRemoveAllFilesEnabled,
            backgroundRpcRequestsErrors = GlobalRpcClient.backgroundRpcRequestsErrors
        )
    }
}

class SeedingScreenViewModel(application: Application) : AndroidViewModel(application) {
    val settings: StateFlow<RpcRequestState<Any>> =
        GlobalRpcClient.performRecoveringRequestIntoStateFlow(viewModelScope) { setInitialState(getSeedingServerSettings()) }

    val ratioLimited: ServerSettingsProperty<Boolean> = ServerSettingsBooleanProperty(RpcClient::setServerRatioLimited)
    val ratioLimit: TremotesfDecimalNumberInputFieldState =
        ServerSettingsDecimalNumberInputFieldState { setServerRatioLimit((it)) }
    val idleSeedingLimited: ServerSettingsProperty<Boolean> =
        ServerSettingsBooleanProperty(RpcClient::setServerIdleSeedingLimited)
    val idleSeedingLimit: TremotesfIntegerNumberInputFieldState =
        ServerSettingsIntegerNumberInputFieldState { setServerIdleSeedingLimit(it.minutes) }
    val scriptTorrentDoneRemoveTorrentEnabled: ServerSettingsProperty<Boolean> =
        ServerSettingsBooleanProperty(RpcClient::setScriptTorrentDoneRemoveTorrentEnabled)
    val scriptTorrentDoneRemoveTempFilesEnabled: ServerSettingsProperty<Boolean> =
        ServerSettingsBooleanProperty(RpcClient::setScriptTorrentDoneRemoveTempFilesEnabled)
    val scriptTorrentDoneRemoveAllFilesEnabled: ServerSettingsProperty<Boolean> =
        ServerSettingsBooleanProperty(RpcClient::setScriptTorrentDoneRemoveAllFilesEnabled)

    private fun setInitialState(settings: SeedingServerSettings) {
        ratioLimited.reset(settings.ratioLimited)
        ratioLimit.reset(settings.ratioLimit)
        idleSeedingLimited.reset(settings.idleSeedingLimited)
        idleSeedingLimit.reset(settings.idleSeedingLimit.inWholeMinutes)
        scriptTorrentDoneRemoveTorrentEnabled.reset(settings.scriptTorrentDoneRemoveTorrentEnabled)
        scriptTorrentDoneRemoveTempFilesEnabled.reset(settings.scriptTorrentDoneRemoveTempFilesEnabled)
        scriptTorrentDoneRemoveAllFilesEnabled.reset(settings.scriptTorrentDoneRemoveAllFilesEnabled)
    }
}

@Composable
private fun ServerSettingsSeedingScreen(
    settingsRequestState: State<RpcRequestState<Any>>,
    navigateUp: () -> Unit,
    ratioLimited: ServerSettingsProperty<Boolean>,
    ratioLimit: TremotesfDecimalNumberInputFieldState,
    idleSeedingLimited: ServerSettingsProperty<Boolean>,
    idleSeedingLimit: TremotesfIntegerNumberInputFieldState,
    scriptTorrentDoneRemoveTorrentEnabled: ServerSettingsProperty<Boolean>,
    scriptTorrentDoneRemoveTempFilesEnabled: ServerSettingsProperty<Boolean>,
    scriptTorrentDoneRemoveAllFilesEnabled: ServerSettingsProperty<Boolean>,
    backgroundRpcRequestsErrors: ReceiveChannel<GlobalRpcClient.BackgroundRpcRequestError>
) {
    ServerSettingsCategory(
        title = R.string.server_settings_seeding,
        settingsRequestState = settingsRequestState,
        navigateUp = navigateUp,
        backgroundRpcRequestsErrors = backgroundRpcRequestsErrors
    ) { horizontalPadding ->
        TremotesfSwitchWithText(
            checked = ratioLimited.value,
            text = R.string.stop_seeding_at_ratio,
            onCheckedChange = ratioLimited::update,
            modifier = Modifier.fillMaxWidth(),
            horizontalContentPadding = horizontalPadding
        )
        TremotesfNumberInputField(
            state = ratioLimit,
            range = NON_NEGATIVE_DECIMALS_RANGE,
            modifier = Modifier.fillMaxWidth().padding(horizontal = horizontalPadding),
            enabled = ratioLimited.value
        )
        TremotesfSwitchWithText(
            checked = idleSeedingLimited.value,
            text = R.string.stop_seeding_if_idle_for,
            onCheckedChange = idleSeedingLimited::update,
            modifier = Modifier.fillMaxWidth(),
            horizontalContentPadding = horizontalPadding
        )
        TremotesfNumberInputField(
            state = idleSeedingLimit,
            // Transmission processes this value as unsigned 16-bit integer
            range = UNSIGNED_16BIT_RANGE,
            modifier = Modifier.fillMaxWidth().padding(horizontal = horizontalPadding),
            enabled = idleSeedingLimited.value,
            suffix = R.string.text_field_suffix_minutes
        )
        TremotesfSwitchWithText(
            checked = scriptTorrentDoneRemoveTorrentEnabled.value,
            text = R.string.script_torrent_done_remove_torrent,
            onCheckedChange = scriptTorrentDoneRemoveTorrentEnabled::update,
            modifier = Modifier.fillMaxWidth(),
            horizontalContentPadding = horizontalPadding
        )
        TremotesfSwitchWithText(
            checked = scriptTorrentDoneRemoveTempFilesEnabled.value,
            text = R.string.script_torrent_done_remove_temp_files,
            onCheckedChange = scriptTorrentDoneRemoveTempFilesEnabled::update,
            modifier = Modifier.fillMaxWidth(),
            horizontalContentPadding = horizontalPadding
        )
        TremotesfSwitchWithText(
            checked = scriptTorrentDoneRemoveAllFilesEnabled.value,
            text = R.string.script_torrent_done_remove_all_files,
            onCheckedChange = scriptTorrentDoneRemoveAllFilesEnabled::update,
            modifier = Modifier.fillMaxWidth(),
            horizontalContentPadding = horizontalPadding
        )
    }
}

@Preview
@Composable
private fun ServerSettingsSeedingScreenPreview() = ScreenPreview {
    ServerSettingsSeedingScreen(
        settingsRequestState = remember { mutableStateOf(RpcRequestState.Loaded(Unit)) },
        navigateUp = {},
        ratioLimited = remember { ServerSettingsBooleanProperty {} },
        ratioLimit = rememberTremotesfDecimalNumberInputFieldState(),
        idleSeedingLimited = remember { ServerSettingsBooleanProperty {} },
        idleSeedingLimit = rememberTremotesfIntegerNumberInputFieldState(),
        scriptTorrentDoneRemoveTorrentEnabled = remember { ServerSettingsBooleanProperty {} },
        scriptTorrentDoneRemoveTempFilesEnabled = remember { ServerSettingsBooleanProperty {} },
        scriptTorrentDoneRemoveAllFilesEnabled = remember { ServerSettingsBooleanProperty {} },
        backgroundRpcRequestsErrors = remember { Channel() }
    )
}
