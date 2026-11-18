// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.rpc.requests.serversettings

import kotlinx.serialization.Contextual
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.descriptors.elementNames
import org.equeim.tremotesf.rpc.RpcClient
import org.equeim.tremotesf.rpc.RpcRequestError
import org.equeim.tremotesf.rpc.requests.NormalizedRpcPath
import org.equeim.tremotesf.rpc.requests.NotNormalizedRpcPath
import org.equeim.tremotesf.rpc.requests.RequestWithFields
import org.equeim.tremotesf.rpc.requests.RpcMethod
import org.equeim.tremotesf.rpc.requests.createStaticRpcRequestBody

/**
 * @throws RpcRequestError
 */
suspend fun RpcClient.getDownloadingServerSettings(): DownloadingServerSettings =
    performRequest<DownloadingServerSettings>(
        DOWNLOADING_SERVER_SETTINGS_REQUEST_BODY,
        "getDownloadingServerSettings"
    ).arguments

/**
 * @throws RpcRequestError
 */
suspend fun RpcClient.setDownloadDirectory(value: String) =
    setSessionProperty("download-dir", NotNormalizedRpcPath(value))

/**
 * @throws RpcRequestError
 */
suspend fun RpcClient.setStartAddedTorrents(value: Boolean) =
    setSessionProperty("start-added-torrents", value)

/**
 * @throws RpcRequestError
 */
suspend fun RpcClient.setRenameIncompleteFiles(value: Boolean) =
    setSessionProperty("rename-partial-files", value)

/**
 * @throws RpcRequestError
 */
suspend fun RpcClient.setIncompleteDirectoryEnabled(value: Boolean) =
    setSessionProperty("incomplete-dir-enabled", value)

/**
 * @throws RpcRequestError
 */
suspend fun RpcClient.setIncompleteDirectory(value: String) =
    setSessionProperty("incomplete-dir", NotNormalizedRpcPath(value))

suspend fun RpcClient.setMinFreeSpace(value: List<String>) =
    setSessionProperty("min-free-space", value)

suspend fun RpcClient.setMinFreeSpaceResumeMargin(value: String) =
    setSessionProperty("min-free-space-resume-margin", value)

suspend fun RpcClient.setMoveCompletedTorrentsEnabled(value: Boolean) =
    setSessionProperty("move_completed_torrents_enabled", value)

suspend fun RpcClient.setMovePartialFilesEnabled(value: Boolean) =
    setSessionProperty("move_partial_files_enabled", value)

suspend fun RpcClient.setForceFinishPartialFilesEnabled(value: Boolean) =
    setSessionProperty("force_finish_partial_files_enabled", value)

@Serializable
data class DownloadingServerSettings(
    @Contextual
    @SerialName("download-dir")
    val downloadDirectory: NormalizedRpcPath,
    @SerialName("start-added-torrents")
    val startAddedTorrents: Boolean,
    @SerialName("rename-partial-files")
    val renameIncompleteFiles: Boolean,
    @SerialName("incomplete-dir-enabled")
    val incompleteDirectoryEnabled: Boolean,
    @Contextual
    @SerialName("incomplete-dir")
    val incompleteDirectory: NormalizedRpcPath,
    @SerialName("min-free-space")
    val minFreeSpace: List<String> = emptyList(),
    @SerialName("min-free-space-resume-margin")
    val minFreeSpaceResumeMargin: String = "",
    @SerialName("move_completed_torrents_enabled")
    val moveCompletedTorrentsEnabled: Boolean = false,
    @SerialName("move_partial_files_enabled")
    val movePartialFilesEnabled: Boolean = false,
    @SerialName("force_finish_partial_files_enabled")
    val forceFinishPartialFilesEnabled: Boolean = false,
)

private val DOWNLOADING_SERVER_SETTINGS_REQUEST_BODY =
    createStaticRpcRequestBody(
        RpcMethod.SessionGet,
        RequestWithFields(DownloadingServerSettings.serializer().descriptor.elementNames.toList())
    )
