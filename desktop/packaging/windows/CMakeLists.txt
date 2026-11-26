# SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
#
# SPDX-License-Identifier: CC0-1.0

include("../CPackCommon.cmake")

set(CPACK_GENERATOR ZIP WIX)
set(CPACK_PACKAGE_NAME "Tremotesf")
set(CPACK_PACKAGE_VENDOR "SNBogdanov")
set(CPACK_PACKAGE_EXECUTABLES "tremotesf;Tremotesf")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Tremotesf")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/license.rtf")
set(CPACK_WIX_VERSION 4)
set(CPACK_WIX_INSTALL_SCOPE perMachine)
set(CPACK_WIX_UPGRADE_GUID "67e0511b-d9de-4b3c-a604-0dd47522d451")
# Stable ProductCode for a given version: rerunning a rebuilt installer of the
# same version enters maintenance mode (Repair/Remove), while a newer version
# gets a new ProductCode and is handled as a major upgrade.
string(
    UUID TREMOTESF_WIX_PRODUCT_GUID
    NAMESPACE "67e0511b-d9de-4b3c-a604-0dd47522d451"
    NAME "Tremotesf ${PROJECT_VERSION}"
    TYPE SHA1
)
set(CPACK_WIX_PRODUCT_GUID "${TREMOTESF_WIX_PRODUCT_GUID}")
set(CPACK_WIX_PRODUCT_ICON "${TREMOTESF_WINDOWS_ICON}")
set(CPACK_WIX_EXTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/registry.wxs"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktop_shortcut.wxs"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktop_shortcut_ui.wxs"
)
set(CPACK_WIX_UI_REF "TremotesfWixUI")
set(CPACK_WIX_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/registry_feature_patch.wxs"
)

include(CPack)
