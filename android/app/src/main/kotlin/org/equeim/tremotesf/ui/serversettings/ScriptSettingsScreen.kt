// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.serversettings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import kotlinx.parcelize.Parcelize
import org.equeim.tremotesf.R
import org.equeim.tremotesf.rpc.GlobalRpcClient
import org.equeim.tremotesf.rpc.requests.ScriptSettingsResponseArguments
import org.equeim.tremotesf.rpc.requests.ScriptSettingsSetScript
import org.equeim.tremotesf.rpc.requests.getScriptSettings
import org.equeim.tremotesf.rpc.requests.setScriptSettings
import org.equeim.tremotesf.ui.Dimens
import org.equeim.tremotesf.ui.NavController
import org.equeim.tremotesf.ui.components.TremotesfIconButtonWithTooltip
import org.equeim.tremotesf.ui.components.TremotesfTopAppBar
import org.equeim.tremotesf.ui.scripts.ParameterField
import org.equeim.tremotesf.ui.scripts.buildParameters
import org.equeim.tremotesf.ui.scripts.defaultString
import org.equeim.tremotesf.ui.scripts.jsonElementToString

@Parcelize
data object ServerSettingsScriptsDestination : org.equeim.tremotesf.ui.Destination {
    @Composable
    override fun Content(navController: NavController) {
        ScriptSettingsScreen(navigateUp = navController::popBackStack)
    }
}

private fun valueKey(scriptId: String, parameterKey: String) = "$scriptId\u0000$parameterKey"

@Composable
private fun ScriptSettingsScreen(navigateUp: () -> Unit) {
    val descriptor = GlobalRpcClient.serverCapabilities?.scriptRunner
    val snackbarHostState = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()
    val context = LocalContext.current

    var settings by remember { mutableStateOf<ScriptSettingsResponseArguments?>(null) }
    var loading by remember { mutableStateOf(true) }
    var saving by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    val values = remember { mutableStateMapOf<String, String>() }

    fun loadValues(response: ScriptSettingsResponseArguments) {
        values.clear()
        response.scripts.forEach { script ->
            script.parameters.forEach { (key, value) ->
                values[valueKey(script.id, key)] = jsonElementToString(value)
            }
        }
        descriptor?.scripts?.filter { it.hasHostParameters() }?.forEach { script ->
            script.parameters.filter { it.storage == "host" }.forEach { parameter ->
                val key = valueKey(script.id, parameter.key)
                if (!values.containsKey(key)) {
                    values[key] = defaultString(parameter)
                }
            }
        }
    }

    LaunchedEffect(descriptor?.revision) {
        if (descriptor == null) {
            loading = false
            error = context.getString(R.string.script_runner_unavailable)
            return@LaunchedEffect
        }
        loading = true
        error = null
        try {
            val response = GlobalRpcClient.getScriptSettings()
            settings = response
            loadValues(response)
        } catch (e: Exception) {
            error = e.message ?: e.toString()
        } finally {
            loading = false
        }
    }

    val save = {
        val currentDescriptor = descriptor
        if (currentDescriptor != null && settings != null && !saving) {
            scope.launch {
                saving = true
                try {
                    val scripts = currentDescriptor.scripts
                        .filter { it.hasHostParameters() }
                        .mapNotNull { script ->
                            val scriptValues = script.parameters
                                .filter { it.storage == "host" }
                                .associate { it.key to values[valueKey(script.id, it.key)].orEmpty() }
                            buildParameters(script, scriptValues, "host")?.let {
                                ScriptSettingsSetScript(script.id, it)
                            }
                        }
                    val previous = settings
                    val changed = scripts.filter { script ->
                        previous?.scripts?.find { it.id == script.id }?.parameters != script.parameters
                    }
                    if (changed.isEmpty()) {
                        snackbarHostState.showSnackbar(context.getString(R.string.script_settings_no_changes))
                        return@launch
                    }
                    val response = GlobalRpcClient.setScriptSettings(previous?.revision, changed)
                    settings = response
                    loadValues(response)
                    snackbarHostState.showSnackbar(context.getString(R.string.script_settings_saved))
                } catch (e: Exception) {
                    snackbarHostState.showSnackbar(e.message ?: e.toString())
                } finally {
                    saving = false
                }
            }
        }
        Unit
    }

    Scaffold(
        topBar = {
            TremotesfTopAppBar(
                title = stringResource(R.string.script_parameters),
                navigateUp = navigateUp,
                actions = {
                    TremotesfIconButtonWithTooltip(
                        icon = Icons.Filled.Check,
                        textId = R.string.save,
                        enabled = !saving && settings != null,
                        onClick = save
                    )
                }
            )
        },
        snackbarHost = { SnackbarHost(snackbarHostState) }
    ) { innerPadding ->
        Column(
            Modifier
                .fillMaxSize()
                .consumeWindowInsets(innerPadding)
                .padding(innerPadding)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = Dimens.screenContentPaddingHorizontal(), vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(Dimens.SpacingSmall)
        ) {
            when {
                loading -> Text(stringResource(R.string.loading))
                error != null -> Text(error.orEmpty(), color = MaterialTheme.colorScheme.error)
                descriptor == null -> Text(stringResource(R.string.script_runner_unavailable))
                else -> {
                    descriptor.scripts.filter { it.hasHostParameters() }.forEach { script ->
                        Text(script.name, style = MaterialTheme.typography.titleMedium)
                        if (script.description.isNotEmpty()) {
                            Text(script.description, style = MaterialTheme.typography.bodySmall)
                        }
                        script.parameters.filter { it.storage == "host" }.forEach { parameter ->
                            ParameterField(
                                parameter = parameter,
                                descriptor = descriptor,
                                value = values[valueKey(script.id, parameter.key)].orEmpty(),
                                onValueChange = { values[valueKey(script.id, parameter.key)] = it }
                            )
                        }
                        val scriptSettings = settings?.scripts?.find { it.id == script.id }
                        if (scriptSettings != null && (!scriptSettings.ready || scriptSettings.missing.isNotEmpty())) {
                            Text(
                                text = stringResource(R.string.required_parameters_missing),
                                color = MaterialTheme.colorScheme.error,
                                style = MaterialTheme.typography.bodySmall
                            )
                        }
                    }
                }
            }
        }
    }
}
