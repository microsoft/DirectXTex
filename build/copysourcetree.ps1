<#

.NOTES
Copyright (c) Microsoft Corporation.
Licensed under the MIT License.

.SYNOPSIS
Copies the source tree excluding various .git and control files.

.DESCRIPTION
This script is used to extract a 'clean' source tree for deplying tests.

.PARAMETER FilePath
Indicates the root of the tree to copy to.

.PARAMETER Overwrite
Indicates overwrite of existing content.
#>

param(
    [Parameter(Mandatory)]
    [string]$Destination,
    [switch]$Overwrite
)

function Copy-Source {

    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$Destination
        )

    $filters = @("*.cpp",
        "*.h", "*.inl",
        "*.cmd",
        "*.hlsl", "*.fx",
        "*.sln", "*.vcxproj", "*.vcxproj.filters",
        "*.config", "*.mgc", "*.appxmanifest", "*.manifest",
        "*.dds", "*.png", "*.jpg", "*.tga")

    $excludefile = Split-Path -Path $PSScriptRoot -Parent
    $excludefile = Join-Path $excludefile -Child "build"
    $excludefile = Join-Path $excludefile -Child "copysourcetree.flt"

    $filters | ForEach-Object {
        $files = Join-Path -Path $Path -ChildPath $_
        xcopy /Y/S/Q /EXCLUDE:$excludefile $files $Destination
        if ($LastExitCode -ne 0) {
            Write-Error "Failed copying source files" -ErrorAction Stop
        }
    }
}

if (-Not (Test-Path $Destination)) {
    Write-Error "ERROR: -Destination folder does not exist" -ErrorAction Stop
}

$targetreadme = Join-Path -Path $Destination -ChildPath "README.md"

if ((Test-Path $targetreadme) -And (-Not $Overwrite)) {
    Write-Error "ERROR: Destination folder contains files. Use -Overwrite to proceed anyhow." -ErrorAction Stop
}

$sourcedir = Split-Path -Path $PSScriptRoot -Parent

$readme = Join-Path -Path $sourcedir -ChildPath "README.md"
$license = Join-Path -Path $sourcedir -ChildPath "LICENSE"

Copy-Item $readme -Destination $Destination
Copy-Item $license -Destination $Destination
Copy-Source -Path $sourcedir -Destination $Destination
