// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.rpc.requests

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import org.equeim.tremotesf.rpc.RpcClient
import java.security.SecureRandom
import java.util.UUID

@Serializable
data class ScriptRunnerDescriptor(
    val schema: String = "",
    val revision: String = "",
    @SerialName("external_hosts")
    val externalHosts: List<ScriptRunnerExternalHost> = emptyList(),
    @SerialName("restart_daemon")
    val restartDaemon: ScriptRunnerRestartDaemon = ScriptRunnerRestartDaemon(),
    val scripts: List<ScriptRunnerScript> = emptyList(),
) {
    fun scriptById(id: String): ScriptRunnerScript? = scripts.firstOrNull { it.id == id }
    fun externalHostById(id: String): ScriptRunnerExternalHost? = externalHosts.firstOrNull { it.id == id }
    fun externalHostName(id: String): String = externalHostById(id)?.name?.takeIf { it.isNotEmpty() } ?: id
}

@Serializable
data class ScriptRunnerExternalHost(
    val id: String,
    val name: String = "",
    val description: String = "",
)

@Serializable
data class ScriptRunnerRestartDaemon(
    val id: String = "restart-daemon",
    val name: String = "",
    val context: String = "system",
    val enabled: Boolean = false,
)

@Serializable
data class ScriptRunnerScript(
    val id: String,
    val name: String,
    val description: String = "",
    val group: String = "",
    val order: Int = 0,
    val platform: String = "any",
    val contexts: List<String> = emptyList(),
    @SerialName("requires_selection")
    val requiresSelection: Boolean = true,
    val confirm: Boolean = false,
    val parameters: List<ScriptRunnerParameter> = emptyList(),
    @SerialName("timeout_seconds")
    val timeoutSeconds: Int = 300,
    @SerialName("max_output_bytes")
    val maxOutputBytes: Long = 0,
) {
    fun hasContext(context: String): Boolean = context in contexts
    fun hasRuntimeParameters(): Boolean = parameters.any { it.storage == "runtime" }
    fun hasHostParameters(): Boolean = parameters.any { it.storage == "host" }
}

@Serializable
data class ScriptRunnerParameter(
    val key: String,
    val name: String,
    val description: String = "",
    val type: String,
    val storage: String = "runtime",
    val array: Boolean = false,
    val required: Boolean = false,
    @SerialName("default")
    val defaultValue: JsonElement? = null,
    val min: Double? = null,
    val max: Double? = null,
    val hosts: List<String> = emptyList(),
)

@Serializable
data class ScriptSelection(
    val hashes: List<String> = emptyList(),
    val files: List<ScriptFileSelection> = emptyList(),
) {
    val isEmpty: Boolean get() = hashes.isEmpty() && files.isEmpty()
    val itemCount: Int get() = hashes.size + files.size
}

@Serializable
data class ScriptFileSelection(
    val hash: String,
    @SerialName("file_index")
    val fileIndex: Int,
)

@Serializable
data class ScriptRunRequestArguments(
    val id: String,
    @SerialName("client_id")
    val clientId: String,
    @SerialName("client_request_id")
    val clientRequestId: String,
    @SerialName("client_secret")
    val clientSecret: String,
    val payload: ScriptRunPayload,
)

@Serializable
data class ScriptRunPayload(
    val schema: String = "transgui.script/v1",
    val context: String,
    val parameters: JsonObject = JsonObject(emptyMap()),
    val selection: ScriptSelection = ScriptSelection(),
)

@Serializable
data class ScriptRunResponseArguments(
    @SerialName("job_id")
    val jobId: String,
    @SerialName("job_token")
    val jobToken: String,
    @SerialName("script_id")
    val scriptId: String = "",
    val state: String = "queued",
    val deduplicated: Boolean = false,
)

@Serializable
data class ScriptStatusRequestArguments(
    @SerialName("job_id")
    val jobId: String,
    @SerialName("job_token")
    val jobToken: String,
    @SerialName("stdout_offset")
    val stdoutOffset: Long = 0,
    @SerialName("stdout_limit")
    val stdoutLimit: Int = 65536,
    @SerialName("stderr_offset")
    val stderrOffset: Long = 0,
    @SerialName("stderr_limit")
    val stderrLimit: Int = 65536,
)

@Serializable
data class ScriptStatusResponseArguments(
    @SerialName("job_id")
    val jobId: String = "",
    @SerialName("script_id")
    val scriptId: String = "",
    val state: String = "",
    @SerialName("exit_code")
    val exitCode: Int? = null,
    @SerialName("term_signal")
    val termSignal: Int? = null,
    val error: String? = null,
    val stdout: String = "",
    val stderr: String = "",
    @SerialName("stdout_offset")
    val stdoutOffset: Long = 0,
    @SerialName("stdout_total_bytes")
    val stdoutTotalBytes: Long = 0,
    @SerialName("stdout_stored_bytes")
    val stdoutStoredBytes: Long = 0,
    @SerialName("stdout_truncated")
    val stdoutTruncated: Boolean = false,
    @SerialName("stderr_offset")
    val stderrOffset: Long = 0,
    @SerialName("stderr_total_bytes")
    val stderrTotalBytes: Long = 0,
    @SerialName("stderr_stored_bytes")
    val stderrStoredBytes: Long = 0,
    @SerialName("stderr_truncated")
    val stderrTruncated: Boolean = false,
    val truncated: Boolean = false,
    @SerialName("timed_out")
    val timedOut: Boolean = false,
    @SerialName("queued_at")
    val queuedAt: Long? = null,
    @SerialName("started_at")
    val startedAt: Long? = null,
    @SerialName("finished_at")
    val finishedAt: Long? = null,
)

@Serializable
data class ScriptCancelRequestArguments(
    @SerialName("job_id")
    val jobId: String,
    @SerialName("job_token")
    val jobToken: String,
)

@Serializable
data class ScriptCancelResponseArguments(
    @SerialName("job_id")
    val jobId: String = "",
    val canceled: Boolean = false,
    val state: String = "",
)

@Serializable
data class ScriptSettingsResponseArguments(
    val schema: String = "",
    val revision: String = "",
    val scripts: List<ScriptSettingsScript> = emptyList(),
)

@Serializable
data class ScriptSettingsScript(
    val id: String,
    val parameters: JsonObject = JsonObject(emptyMap()),
    val ready: Boolean = true,
    val missing: List<String> = emptyList(),
)

@Serializable
data class ScriptSettingsSetRequestArguments(
    @SerialName("expected_revision")
    val expectedRevision: String? = null,
    val scripts: List<ScriptSettingsSetScript>,
)

@Serializable
data class ScriptSettingsSetScript(
    val id: String,
    val parameters: JsonObject,
)

@Serializable
data class ScriptRestartDaemonRequestArguments(
    @SerialName("client_id")
    val clientId: String? = null,
)

@Serializable
data class ScriptRestartDaemonResponseArguments(
    val accepted: Boolean = false,
    @SerialName("restart_id")
    val restartId: String = "",
    val state: String = "",
    @SerialName("scheduled_at")
    val scheduledAt: Long? = null,
)

suspend fun RpcClient.runScript(
    scriptId: String,
    context: String,
    selection: ScriptSelection,
    parameters: JsonObject,
    clientId: String,
): ScriptRunResponseArguments {
    val random = SecureRandom()
    val secret = ByteArray(32).also(random::nextBytes).joinToString("") { "%02x".format(it) }
    return performRequest<ScriptRunResponseArguments, _>(
        RpcMethod.ScriptRun,
        ScriptRunRequestArguments(
            id = scriptId,
            clientId = clientId,
            clientRequestId = UUID.randomUUID().toString(),
            clientSecret = secret,
            payload = ScriptRunPayload(
                context = context,
                parameters = parameters,
                selection = selection,
            ),
        ),
        "runScript"
    ).arguments
}

suspend fun RpcClient.getScriptStatus(
    jobId: String,
    jobToken: String,
    stdoutOffset: Long,
    stderrOffset: Long,
): ScriptStatusResponseArguments = performRequest<ScriptStatusResponseArguments, _>(
    RpcMethod.ScriptStatus,
    ScriptStatusRequestArguments(
        jobId = jobId,
        jobToken = jobToken,
        stdoutOffset = stdoutOffset,
        stderrOffset = stderrOffset,
    ),
    "getScriptStatus"
).arguments

suspend fun RpcClient.cancelScript(jobId: String, jobToken: String): ScriptCancelResponseArguments =
    performRequest<ScriptCancelResponseArguments, _>(
        RpcMethod.ScriptCancel,
        ScriptCancelRequestArguments(jobId = jobId, jobToken = jobToken),
        "cancelScript"
    ).arguments

suspend fun RpcClient.getScriptSettings(): ScriptSettingsResponseArguments =
    performRequest<ScriptSettingsResponseArguments, _>(
        RpcMethod.ScriptSettingsGet,
        JsonObject(emptyMap()),
        "getScriptSettings"
    ).arguments

suspend fun RpcClient.setScriptSettings(
    expectedRevision: String?,
    scripts: List<ScriptSettingsSetScript>,
): ScriptSettingsResponseArguments = performRequest<ScriptSettingsResponseArguments, _>(
    RpcMethod.ScriptSettingsSet,
    ScriptSettingsSetRequestArguments(expectedRevision = expectedRevision, scripts = scripts),
    "setScriptSettings"
).arguments

suspend fun RpcClient.restartDaemon(clientId: String? = null): ScriptRestartDaemonResponseArguments =
    performRequest<ScriptRestartDaemonResponseArguments, _>(
        RpcMethod.ScriptRestartDaemon,
        ScriptRestartDaemonRequestArguments(clientId = clientId),
        "restartDaemon"
    ).arguments

internal fun ScriptRunnerParameter.defaultAsJson(): JsonElement? = defaultValue

internal fun scriptParameterString(value: JsonElement?): String =
    (value as? JsonPrimitive)?.content ?: ""
