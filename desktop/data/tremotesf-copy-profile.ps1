# SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
#
# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
Copies Tremotesf profile settings between profiles.

.DESCRIPTION
Copies servers.ini (server settings) and/or tremotesf.ini (program settings)
from one Tremotesf profile to another.

Profiles are stored in:
  main profile    - %APPDATA%\tremotesf
  named profile   - %APPDATA%\tremotesf-<name>

Close Tremotesf before copying settings, otherwise the running instance may
overwrite the copied files on exit.

.PARAMETER Profiles
Target profile name, or source and target profile names:
  one value  - copy from the main profile to the target profile
  two values - copy from the source profile to the target profile

.PARAMETER ServerSettings
Copy servers.ini only. Alias: -Servers.

.PARAMETER ProgramSettings
Copy tremotesf.ini only. Alias: -Settings.

.EXAMPLE
.\tremotesf-copy-profile.ps1 alfa
Copies both settings files from the main profile to profile "alfa".

.EXAMPLE
.\tremotesf-copy-profile.ps1 alfa beta
Copies both settings files from profile "alfa" to profile "beta".

.EXAMPLE
.\tremotesf-copy-profile.ps1 beta -ServerSettings
Copies only servers.ini from the main profile to profile "beta".
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Profiles,

    [Alias('Servers')]
    [switch]$ServerSettings,

    [Alias('Settings', 'AppSettings')]
    [switch]$ProgramSettings,

    [switch]$Help
)

$ErrorActionPreference = 'Stop'

function Show-Usage {
    Write-Host @'
Usage:
  tremotesf-copy-profile.ps1 <target-profile> [-ServerSettings] [-ProgramSettings]
  tremotesf-copy-profile.ps1 <source-profile> <target-profile> [-ServerSettings] [-ProgramSettings]
  tremotesf-copy-profile.ps1 [-Help]

Copies Tremotesf settings between profiles.

  1 value   copy from the main profile to <target-profile>
  2 values  copy from <source-profile> to <target-profile>

Options:
  -ServerSettings, -Servers    copy servers.ini (server settings)
  -ProgramSettings, -Settings  copy tremotesf.ini (program settings)
  -Help                        show this help

If no option is specified, both files are copied.

Profiles:
  main profile    %APPDATA%\tremotesf
  named profile   %APPDATA%\tremotesf-<name>

Examples:
  .\tremotesf-copy-profile.ps1 alfa
  .\tremotesf-copy-profile.ps1 alfa beta
  .\tremotesf-copy-profile.ps1 beta -Servers
'@
}

function Get-AppDataPath {
    $path = [Environment]::GetFolderPath([Environment+SpecialFolder]::ApplicationData)
    if ([string]::IsNullOrWhiteSpace($path)) {
        $path = $env:APPDATA
    }
    if ([string]::IsNullOrWhiteSpace($path)) {
        throw 'Cannot determine the Application Data directory'
    }
    return $path
}

function Get-ProfileDirectory([string]$Name) {
    if ([string]::IsNullOrEmpty($Name)) {
        return Join-Path (Get-AppDataPath) 'tremotesf'
    }
    if ($Name -notmatch '^[A-Za-z0-9._-]{1,64}$' -or $Name -eq '.' -or $Name -eq '..') {
        throw "Invalid profile name '$Name'. Use 1-64 characters: A-Z, a-z, 0-9, '.', '_', '-'"
    }
    return Join-Path (Get-AppDataPath) "tremotesf-$Name"
}

try {
    $profileValues = @($Profiles | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

    if ($Help -or $profileValues.Count -eq 0) {
        Show-Usage
        exit 0
    }
    if ($profileValues.Count -gt 2) {
        Show-Usage
        throw 'Too many arguments'
    }

    $sourceProfile = if ($profileValues.Count -eq 2) { $profileValues[0] } else { '' }
    $targetProfile = $profileValues[-1]

    if ($profileValues.Count -eq 2 -and $sourceProfile -eq $targetProfile) {
        throw 'Source and target profiles must be different'
    }

    if (-not $ServerSettings -and -not $ProgramSettings) {
        $ServerSettings = $true
        $ProgramSettings = $true
    }

    $files = @()
    if ($ServerSettings) {
        $files += 'servers.ini'
    }
    if ($ProgramSettings) {
        $files += 'tremotesf.ini'
    }

    $sourceDirectory = Get-ProfileDirectory $sourceProfile
    $targetDirectory = Get-ProfileDirectory $targetProfile

    if (-not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
        throw "Source profile directory not found: $sourceDirectory"
    }

    New-Item -ItemType Directory -Force -Path $targetDirectory | Out-Null

    $copied = 0
    foreach ($file in $files) {
        $sourceFile = Join-Path $sourceDirectory $file
        if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
            Write-Warning "File not found, skipped: $sourceFile"
            continue
        }
        Copy-Item -LiteralPath $sourceFile -Destination (Join-Path $targetDirectory $file) -Force
        Write-Host "Copied: $file"
        ++$copied
    }

    if ($copied -eq 0) {
        throw 'No settings files were copied'
    }

    Write-Host "Done: $sourceDirectory -> $targetDirectory"
    exit 0
} catch {
    Write-Error $_
    exit 1
}
