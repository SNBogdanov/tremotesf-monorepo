// SPDX-FileCopyrightText: 2017-2026 Alexey Rochev <equeim@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

package org.equeim.tremotesf.rpc

import org.equeim.tremotesf.rpc.requests.FreeSpaceRequestArgumentsWithNormalizedPath
import org.equeim.tremotesf.rpc.requests.RpcMethod
import org.equeim.tremotesf.rpc.requests.ServerVersionResponseArguments
import org.equeim.tremotesf.rpc.requests.createStaticRpcRequestBody

class ServerCapabilities internal constructor(
    private val rpcVersion: Int,
    val serverOs: ServerOs,
    val forkFeatures: List<String> = emptyList(),
) {
    val hasTableMode: Boolean get() = rpcVersion >= 16
    val hasTrackerListProperty: Boolean get() = rpcVersion >= 17
    val supportsLabels: Boolean get() = rpcVersion >= 16
    val supportsStatusV2: Boolean get() = STATUS_V2 in forkFeatures

    enum class ServerOs {
        UnixLike,
        Windows
    }

    override fun toString(): String =
        "ServerCapabilities(hasTableMode=$hasTableMode, serverOs=$serverOs, forkFeatures=$forkFeatures)"

    private companion object {
        const val STATUS_V2 = "status/v2"
    }
}

internal val ServerVersionResponseArguments.isSupported: Boolean
    get() = MINIMUM_RPC_VERSION in minimumRpcVersion..rpcVersion

internal const val MINIMUM_RPC_VERSION = 15

internal val UNIX_ROOT_FREE_SPACE_REQUEST = createStaticRpcRequestBody(
    RpcMethod.FreeSpace,
    FreeSpaceRequestArgumentsWithNormalizedPath("/")
)
