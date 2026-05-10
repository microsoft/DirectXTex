<#

.NOTES
Copyright (c) Microsoft Corporation.
Licensed under the MIT License.

.SYNOPSIS
Updates the winget manifests for texassemble, texconv, and texdiag to match a GitHub release.

.DESCRIPTION
This script creates new winget manifest versions for texassemble, texconv, and texdiag under
D:\winget-pkgs based on the most recent release date in README.md. It copies the previous
version's manifests, then updates PackageVersion, ReleaseDate, InstallerSha256, InstallerUrl,
and ReleaseNotesUrl to match the new release.

.PARAMETER Tag
The GitHub release tag (e.g., 'may2026', 'mar2026'). Defaults to the latest tag.

.LINK
https://github.com/microsoft/DirectXTex/wiki

#>

param(
    [string]$Tag = ""
)

$repoRoot = Split-Path -Path $PSScriptRoot -Parent
$readme = Join-Path $repoRoot "README.md"
$wingetPkgs = "D:\winget-pkgs"

if (-Not (Test-Path $readme)) {
    Write-Error "ERROR: Cannot find README.md at $readme" -ErrorAction Stop
}

if (-Not (Test-Path $wingetPkgs)) {
    Write-Error "ERROR: Cannot find winget-pkgs at $wingetPkgs" -ErrorAction Stop
}

# Determine tag from latest git tag if not provided
if ($Tag.Length -eq 0) {
    $Tag = (git --no-pager -C $repoRoot tag --sort=-creatordate | Select-Object -First 1).Trim()
    if ($Tag.Length -eq 0) {
        Write-Error "ERROR: Failed to determine latest tag!" -ErrorAction Stop
    }
}

Write-Host "Release Tag: $Tag"

# Parse release date from README.md (format: "## Month Day, Year")
$rawReleaseDate = (Get-Content $readme) | Select-String -Pattern "^##\s+[A-Z][a-z]+\s+\d+,?\s+\d{4}" | Select-Object -First 1
if ([string]::IsNullOrEmpty($rawReleaseDate)) {
    Write-Error "ERROR: Failed to find release date in README.md!" -ErrorAction Stop
}

$releaseDateStr = ($rawReleaseDate -replace '^##\s+', '').Trim()
$releaseDate = [datetime]::Parse($releaseDateStr, [System.Globalization.CultureInfo]::InvariantCulture)

# winget version format: YYYY.M.D (no leading zeros)
$packageVersion = $releaseDate.ToString("yyyy.M.d")

# winget ReleaseDate format: YYYY-MM-DD
$releaseDateYaml = $releaseDate.ToString("yyyy-MM-dd")

Write-Host "Release Date: $releaseDateStr"
Write-Host "Package Version: $packageVersion"
Write-Host "Release Date (YAML): $releaseDateYaml"

# --- Tool definitions ---
# Each tool has a manifest base path and a list of release assets (name + architecture)
$tools = @(
    @{
        Name = "Texassemble"
        ManifestBase = Join-Path $wingetPkgs "manifests\m\Microsoft\DirectXTex\Texassemble"
        PackageId = "Microsoft.DirectXTex.Texassemble"
        Assets = @(
            @{ FileName = "texassemble.exe"; Arch = "x64" },
            @{ FileName = "texassemble_arm64.exe"; Arch = "arm64" }
        )
    },
    @{
        Name = "Texconv"
        ManifestBase = Join-Path $wingetPkgs "manifests\m\Microsoft\DirectXTex\Texconv"
        PackageId = "Microsoft.DirectXTex.Texconv"
        Assets = @(
            @{ FileName = "texconv.exe"; Arch = "x64" },
            @{ FileName = "texconv_arm64.exe"; Arch = "arm64" }
        )
    },
    @{
        Name = "Texdiag"
        ManifestBase = Join-Path $wingetPkgs "manifests\m\Microsoft\DirectXTex\Texdiag"
        PackageId = "Microsoft.DirectXTex.Texdiag"
        Assets = @(
            @{ FileName = "texdiag.exe"; Arch = "x64" },
            @{ FileName = "texdiag_arm64.exe"; Arch = "arm64" }
        )
    }
)

# --- Find previous version directory ---
function Get-LatestVersionDir {
    param([string]$BasePath)
    $dirs = Get-ChildItem -Path $BasePath -Directory | Sort-Object Name
    if ($dirs.Count -eq 0) {
        Write-Error "ERROR: No existing version directories found in $BasePath" -ErrorAction Stop
    }
    return $dirs[-1]
}

# --- Download release assets and compute SHA256 hashes ---
$ProgressPreference = 'SilentlyContinue'
$tempDir = Join-Path $Env:Temp $(New-Guid)
New-Item -Type Directory -Path $tempDir | Out-Null

$hashes = @{}

# Collect all unique asset filenames across tools
$allAssets = $tools | ForEach-Object { $_.Assets } | ForEach-Object { $_ }

foreach ($asset in $allAssets) {
    $url = "https://github.com/microsoft/DirectXTex/releases/download/$Tag/$($asset.FileName)"
    $outPath = Join-Path $tempDir $asset.FileName
    Write-Host "`nDownloading $($asset.FileName) from $url..."
    try {
        Invoke-WebRequest -Uri $url -OutFile $outPath -ErrorAction Stop
    }
    catch {
        Write-Error "ERROR: Failed to download $($asset.FileName)!" -ErrorAction Stop
    }
    $hash = (Get-FileHash -Path $outPath -Algorithm SHA256).Hash.ToLower()
    $hashes[$asset.FileName] = $hash
    Write-Host "  SHA256: $hash"
}

# --- Create new manifests for each tool ---
$newDirs = @()

foreach ($tool in $tools) {
    $prevDir = Get-LatestVersionDir $tool.ManifestBase
    $newDir = Join-Path $tool.ManifestBase $packageVersion

    Write-Host "`nPrevious $($tool.Name) version: $($prevDir.Name)"

    if (Test-Path $newDir) {
        Write-Error "ERROR: $($tool.Name) version $packageVersion already exists at $newDir" -ErrorAction Stop
    }

    Write-Host "Creating $($tool.Name) $packageVersion manifests..."
    Copy-Item -Path $prevDir.FullName -Destination $newDir -Recurse

    foreach ($file in Get-ChildItem -Path $newDir -Filter "*.yaml") {
        $content = Get-Content $file.FullName -Raw
        $content = $content -replace "PackageVersion:\s+\S+", "PackageVersion: $packageVersion"

        if ($file.Name -match "installer") {
            $content = $content -replace "ReleaseDate:\s+\S+", "ReleaseDate: $releaseDateYaml"

            # Update installer URLs
            foreach ($asset in $tool.Assets) {
                $escapedFileName = [regex]::Escape($asset.FileName)
                $content = $content -replace "(InstallerUrl:\s+).+$escapedFileName", "`${1}https://github.com/microsoft/DirectXTex/releases/download/$Tag/$($asset.FileName)"
            }

            # Update SHA256 hashes per architecture block
            $lines = $content -split "`n"
            $currentArch = ""
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match "Architecture:\s+(\S+)") {
                    $currentArch = $Matches[1]
                }

                if ($lines[$i] -match "InstallerSha256:") {
                    $matchingAsset = $tool.Assets | Where-Object { $_.Arch -eq $currentArch }
                    if ($matchingAsset) {
                        $lines[$i] = "  InstallerSha256: $($hashes[$matchingAsset.FileName])"
                    }
                }
            }
            $content = $lines -join "`n"
        }

        if ($file.Name -match "locale") {
            $content = $content -replace "(ReleaseNotesUrl:\s+).+", "`${1}https://github.com/microsoft/DirectXTex/releases/tag/$Tag"
        }

        Set-Content -Path $file.FullName -Value $content -NoNewline
    }

    foreach ($file in Get-ChildItem -Path $newDir -Filter "*.yaml") {
        Write-Host "  $($file.Name)"
    }

    $newDirs += $newDir
}

# --- Cleanup ---
Remove-Item -Recurse -Force $tempDir

Write-Host "`nwinget manifests created successfully!"
Write-Host "`nNew manifest directories:"
foreach ($dir in $newDirs) {
    Write-Host "  $dir"
}
Write-Host "`nNext steps:"
Write-Host "  1. Review the generated manifest files"
foreach ($dir in $newDirs) {
    Write-Host "  2. Validate with: winget validate $dir"
}
foreach ($dir in $newDirs) {
    Write-Host "  3. Test with: winget install --manifest $dir"
}
Write-Host "  4. Submit PRs to the winget-pkgs repository"
Write-Host ""
Write-Host "  NOTE: Each tool must be submitted as its own separate PR per winget-pkgs policy."
