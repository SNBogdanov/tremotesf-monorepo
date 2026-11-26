// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.ui.scripts

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import org.equeim.tremotesf.R
import org.equeim.tremotesf.rpc.GlobalRpcClient
import org.equeim.tremotesf.rpc.PeriodicServerStateUpdater
import org.equeim.tremotesf.rpc.requests.ScriptRunResponseArguments
import org.equeim.tremotesf.rpc.requests.ScriptRunnerDescriptor
import org.equeim.tremotesf.rpc.requests.ScriptRunnerParameter
import org.equeim.tremotesf.rpc.requests.ScriptRunnerScript
import org.equeim.tremotesf.rpc.requests.ScriptSelection
import org.equeim.tremotesf.rpc.requests.ScriptStatusResponseArguments
import org.equeim.tremotesf.rpc.requests.cancelScript
import org.equeim.tremotesf.rpc.requests.getScriptStatus
import org.equeim.tremotesf.rpc.requests.runScript
import org.equeim.tremotesf.ui.AppForegroundTracker
import org.equeim.tremotesf.ui.Settings
import java.util.UUID

private val scriptClientId = "tremotesf-" + UUID.randomUUID().toString()

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ScriptsBottomSheet(
    descriptor: ScriptRunnerDescriptor,
    selection: ScriptSelection,
    context: String,
    onDismiss: () -> Unit,
    onStarted: (ScriptRunnerScript, ScriptRunResponseArguments) -> Unit,
) {
    var selectedScript by remember { mutableStateOf<ScriptRunnerScript?>(null) }
    val scripts = remember(descriptor, selection, context) {
        descriptor.scripts
            .filter { it.hasContext(context) && (!it.requiresSelection || !selection.isEmpty) }
            .sortedWith(compareBy({ it.group }, { it.order }, { it.name }, { it.id }))
    }

    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(
            Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 24.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            Text(
                text = stringResource(R.string.script_scripts),
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            if (scripts.isEmpty()) {
                Text(stringResource(R.string.script_no_scripts))
            } else {
                var lastGroup: String? = null
                scripts.forEach { script ->
                    if (script.group.isNotEmpty() && script.group != lastGroup) {
                        Text(
                            text = script.group,
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.padding(top = 12.dp, bottom = 4.dp)
                        )
                        lastGroup = script.group
                    }
                    ListItem(
                        headlineContent = { Text(script.name) },
                        supportingContent = if (script.description.isNotEmpty()) {
                            { Text(script.description) }
                        } else {
                            null
                        },
                        modifier = Modifier.clickable { selectedScript = script }
                    )
                }
            }
        }
    }

    selectedScript?.let { script ->
        ScriptRunDialog(
            script = script,
            descriptor = descriptor,
            selection = selection,
            context = context,
            onDismiss = { selectedScript = null },
            onStarted = { response ->
                onStarted(script, response)
                selectedScript = null
                onDismiss()
            }
        )
    }
}

@Composable
private fun ScriptRunDialog(
    script: ScriptRunnerScript,
    descriptor: ScriptRunnerDescriptor,
    selection: ScriptSelection,
    context: String,
    onDismiss: () -> Unit,
    onStarted: (ScriptRunResponseArguments) -> Unit,
) {
    val values = remember(script.id) {
        mutableStateMapOf<String, String>().apply {
            script.parameters
                .filter { it.storage == "runtime" }
                .forEach { parameter -> put(parameter.key, defaultString(parameter)) }
        }
    }
    var error by remember { mutableStateOf<String?>(null) }
    var running by remember { mutableStateOf(false) }
    var rememberParameters by remember { mutableStateOf(true) }
    val scope = rememberCoroutineScope()

    LaunchedEffect(Unit) {
        rememberParameters = Settings.rememberScriptParameters.get()
    }
    LaunchedEffect(script.id, rememberParameters) {
        if (rememberParameters) {
            ScriptParameterHistory.load(script.id).forEach { (key, value) ->
                if (values.containsKey(key)) {
                    values[key] = value
                }
            }
        }
    }

    AlertDialog(
        onDismissRequest = { if (!running) onDismiss() },
        title = { Text(script.name) },
        text = {
            Column(
                Modifier
                    .fillMaxWidth()
                    .heightIn(max = 480.dp)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                if (script.description.isNotEmpty()) {
                    Text(script.description)
                }
                if (selection.files.isEmpty()) {
                    Text(stringResource(R.string.script_selected_torrents, selection.hashes.size))
                } else {
                    Text(pluralStringResource(R.plurals.files_selected, selection.files.size, selection.files.size))
                }
                script.parameters
                    .filter { it.storage == "runtime" }
                    .forEach { parameter ->
                        ParameterField(
                            parameter = parameter,
                            descriptor = descriptor,
                            value = values[parameter.key].orEmpty(),
                            onValueChange = { values[parameter.key] = it }
                        )
                    }
                error?.let {
                    Text(it, color = MaterialTheme.colorScheme.error)
                }
            }
        },
        confirmButton = {
            TextButton(
                enabled = !running,
                onClick = {
                    val parameters = buildParameters(script, values)
                    if (parameters == null) {
                        error = "Invalid script parameters"
                        return@TextButton
                    }
                    error = null
                    running = true
                    scope.launch {
                        try {
                            val response = GlobalRpcClient.runScript(
                                scriptId = script.id,
                                context = context,
                                selection = selection,
                                parameters = parameters,
                                clientId = scriptClientId
                            )
                            if (rememberParameters) {
                                ScriptParameterHistory.save(script.id, values.toMap())
                            }
                            onStarted(response)
                        } catch (e: Exception) {
                            error = e.message ?: e.toString()
                            running = false
                        }
                    }
                }
            ) {
                Text(stringResource(R.string.script_run))
            }
        },
        dismissButton = {
            TextButton(enabled = !running, onClick = onDismiss) {
                Text(stringResource(R.string.script_cancel))
            }
        }
    )
}

@Composable
internal fun ParameterField(
    parameter: ScriptRunnerParameter,
    descriptor: ScriptRunnerDescriptor,
    value: String,
    onValueChange: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(
            text = parameter.name + if (parameter.required) " *" else "",
            style = MaterialTheme.typography.labelLarge
        )
        if (parameter.description.isNotEmpty()) {
            Text(parameter.description, style = MaterialTheme.typography.bodySmall)
        }
        when {
            parameter.type == "boolean" && !parameter.array -> {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Switch(
                        checked = value.equals("true", ignoreCase = true),
                        onCheckedChange = { onValueChange(it.toString()) }
                    )
                }
            }
            parameter.type == "host" && !parameter.array -> {
                HostSelector(
                    descriptor = descriptor,
                    hosts = parameter.hosts,
                    value = value,
                    onValueChange = onValueChange
                )
            }
            else -> {
                OutlinedTextField(
                    value = value,
                    onValueChange = onValueChange,
                    label = { Text(parameter.name) },
                    placeholder = if (parameter.array) {
                        { Text(stringResource(R.string.script_parameter_hint_array)) }
                    } else {
                        null
                    },
                    keyboardOptions = when (parameter.type) {
                        "integer" -> KeyboardOptions(keyboardType = KeyboardType.Number)
                        "number" -> KeyboardOptions(keyboardType = KeyboardType.Decimal)
                        else -> KeyboardOptions.Default
                    },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
}

@Composable
private fun HostSelector(
    descriptor: ScriptRunnerDescriptor,
    hosts: List<String>,
    value: String,
    onValueChange: (String) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Box {
        OutlinedButton(onClick = { expanded = true }, modifier = Modifier.fillMaxWidth()) {
            Text(descriptor.externalHostName(value))
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            hosts.forEach { hostId ->
                DropdownMenuItem(
                    text = { Text(descriptor.externalHostName(hostId)) },
                    onClick = {
                        onValueChange(hostId)
                        expanded = false
                    }
                )
            }
        }
    }
}

@Composable
fun ScriptJobDialog(
    scriptName: String,
    job: ScriptRunResponseArguments,
    onDismiss: () -> Unit,
) {
    var state by remember { mutableStateOf(job.state) }
    var stdoutText by remember { mutableStateOf("") }
    var stderrText by remember { mutableStateOf("") }
    var stdoutTruncated by remember { mutableStateOf(false) }
    var stderrTruncated by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    var closeOnCompletion by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    val currentCloseOnCompletion by rememberUpdatedState(closeOnCompletion)
    val outputScrollState = rememberScrollState()

    LaunchedEffect(Unit) {
        closeOnCompletion = Settings.closeScriptDialogOnCompletion.get()
    }
    LaunchedEffect(stdoutText, stderrText) {
        withFrameNanos { }
        outputScrollState.animateScrollTo(outputScrollState.maxValue)
    }

    LaunchedEffect(job.jobId) {
        var stdoutOffset = 0L
        var stderrOffset = 0L
        while (isActive) {
            val status: ScriptStatusResponseArguments = try {
                GlobalRpcClient.getScriptStatus(job.jobId, job.jobToken, stdoutOffset, stderrOffset)
            } catch (e: Exception) {
                error = e.message ?: e.toString()
                break
            }
            state = status.state
            error = status.error
            stdoutText += status.stdout
            stderrText += status.stderr
            stdoutOffset = status.stdoutOffset + status.stdout.toByteArray().size
            stderrOffset = status.stderrOffset + status.stderr.toByteArray().size
            stdoutTruncated = stdoutTruncated || status.stdoutTruncated
            stderrTruncated = stderrTruncated || status.stderrTruncated
            if (status.state == "finished" ||
                status.state == "failed" ||
                status.state == "canceled" ||
                status.state == "timed_out"
            ) {
                if (!AppForegroundTracker.appInForeground.value) {
                    PeriodicServerStateUpdater.notificationsController.showScriptJobFinishedNotification(
                        scriptName = scriptName,
                        state = status.state,
                        exitCode = status.exitCode
                    )
                }
                if (currentCloseOnCompletion) {
                    onDismiss()
                }
                break
            }
            delay(1000)
        }
    }

    val active = state == "queued" || state == "running"

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(scriptName) },
        text = {
            Column(
                Modifier
                    .fillMaxWidth()
                    .heightIn(max = 480.dp)
                    .verticalScroll(outputScrollState),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(stringResource(R.string.script_state, scriptStateText(state)))
                error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                Text(stringResource(R.string.script_stdout), style = MaterialTheme.typography.labelMedium)
                Text(
                    text = ansiToAnnotatedString(
                        stdoutText + if (stdoutTruncated) "\n" + stringResource(R.string.script_output_truncated) else ""
                    ),
                    fontFamily = FontFamily.Monospace,
                    style = MaterialTheme.typography.bodySmall
                )
                Text(stringResource(R.string.script_stderr), style = MaterialTheme.typography.labelMedium)
                Text(
                    text = ansiToAnnotatedString(
                        stderrText + if (stderrTruncated) "\n" + stringResource(R.string.script_output_truncated) else ""
                    ),
                    fontFamily = FontFamily.Monospace,
                    style = MaterialTheme.typography.bodySmall
                )
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.close))
            }
        },
        dismissButton = {
            if (active) {
                TextButton(onClick = {
                    scope.launch {
                        runCatching { GlobalRpcClient.cancelScript(job.jobId, job.jobToken) }
                    }
                }) {
                    Text(stringResource(R.string.script_cancel))
                }
            }
        }
    )
}

@Composable
private fun scriptStateText(state: String): String = when (state) {
    "queued" -> stringResource(R.string.script_queued)
    "running" -> stringResource(R.string.script_running)
    "finished" -> stringResource(R.string.script_finished)
    "failed" -> stringResource(R.string.script_failed)
    "canceled" -> stringResource(R.string.script_canceled)
    "timed_out" -> stringResource(R.string.script_timed_out)
    else -> state
}

internal fun defaultString(parameter: ScriptRunnerParameter): String {
    val value = parameter.defaultValue ?: return ""
    return when (value) {
        is JsonArray -> value.joinToString(",") { jsonElementToString(it) }
        is JsonPrimitive -> if (parameter.type == "boolean") value.content else value.content
        else -> ""
    }
}

internal fun jsonElementToString(element: JsonElement): String =
    (element as? JsonPrimitive)?.content ?: ""

internal fun buildParameters(script: ScriptRunnerScript, values: Map<String, String>, storage: String = "runtime"): JsonObject? {
    val result = mutableMapOf<String, JsonElement>()
    for (parameter in script.parameters) {
        if (parameter.storage != storage) {
            continue
        }
        val raw = values[parameter.key]?.trim().orEmpty()
        if (raw.isEmpty()) {
            if (parameter.required) {
                return null
            }
            continue
        }

        if (parameter.array) {
            val elements = raw.split(',').map { it.trim() }.filter { it.isNotEmpty() }.map { item ->
                valueElement(parameter, item) ?: return null
            }
            result[parameter.key] = JsonArray(elements)
        } else {
            result[parameter.key] = valueElement(parameter, raw) ?: return null
        }
    }
    return JsonObject(result)
}

private fun valueElement(parameter: ScriptRunnerParameter, raw: String): JsonElement? {
    val min = parameter.min
    val max = parameter.max
    return when (parameter.type) {
        "string", "host" -> JsonPrimitive(raw)
        "integer" -> raw.toLongOrNull()?.takeIf { number ->
            (min == null || number >= min) && (max == null || number <= max)
        }?.let { JsonPrimitive(it) }
        "number" -> raw.toDoubleOrNull()?.takeIf { number ->
            (min == null || number >= min) && (max == null || number <= max)
        }?.let { JsonPrimitive(it) }
        "boolean" -> when (raw.lowercase()) {
            "true" -> JsonPrimitive(true)
            "false" -> JsonPrimitive(false)
            else -> null
        }
        else -> null
    }
}
