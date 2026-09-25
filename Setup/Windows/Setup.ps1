# Copyright (c) 2026 Nelaric
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'Run this script on Windows.'
}
if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne [Runtime.InteropServices.Architecture]::X64) {
    throw 'The bundled Windows V8 backend requires x64.'
}
foreach ($command in @('node', 'npm', 'curl.exe', 'tar.exe')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "Required command is missing: $command"
    }
}

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$plugin = Join-Path $repo 'NelaricGameplay/Plugins/Puerts'
$backend = Join-Path $plugin 'ThirdParty/v8_9.4.146.24'
$cache = Join-Path $repo '.tools/puerts'
$archive = Join-Path $cache 'puerts_v8_94-Unreal_v1.0.9.tgz'
$expectedHash = '0ab80673f6bf59cf7f3f80ff17f254975f811813c4637ba2d85d6455fd1e4e24'
$url = 'https://github.com/Tencent/puerts/releases/download/Unreal_v1.0.9/puerts_v8_94.tgz'

if (-not (Test-Path -LiteralPath (Join-Path $plugin 'Puerts.uplugin'))) {
    throw 'PuerTS source is missing from NelaricGameplay/Plugins/Puerts.'
}
New-Item -ItemType Directory -Path $cache -Force | Out-Null

if (Test-Path -LiteralPath $backend) {
    foreach ($relative in @('Inc/v8.h', 'Lib/Win64MD/wee8.lib', 'Lib/Win64DLL/v8.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $backend $relative))) {
            throw "Existing V8 backend is incomplete: $relative"
        }
    }
    Write-Host 'V8 backend is already installed.'
} else {
    $validArchive = (Test-Path -LiteralPath $archive) -and
        ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expectedHash)
    if (-not $validArchive) {
        $partial = Join-Path $cache 'puerts_v8_94.download'
        try {
            Write-Host 'Downloading the official PuerTS V8 9.4 package...'
            & curl.exe --fail --location --retry 3 --silent --show-error --output $partial $url
            if ($LASTEXITCODE -ne 0) { throw 'V8 download failed.' }
            $actualHash = (Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($actualHash -ne $expectedHash) { throw 'V8 archive SHA-256 mismatch.' }
            Move-Item -LiteralPath $partial -Destination $archive -Force
        } finally {
            if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
        }
    }

    $stage = Join-Path $cache ([Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage | Out-Null
    try {
        Write-Host 'Extracting the V8 backend...'
        & tar.exe -xzf $archive -C $stage 'Puerts/ThirdParty/v8_9.4.146.24'
        if ($LASTEXITCODE -ne 0) { throw 'V8 extraction failed.' }
        $extracted = Join-Path $stage 'Puerts/ThirdParty/v8_9.4.146.24'
        foreach ($relative in @('Inc/v8.h', 'Lib/Win64MD/wee8.lib', 'Lib/Win64DLL/v8.dll')) {
            if (-not (Test-Path -LiteralPath (Join-Path $extracted $relative))) {
                throw "V8 package is missing: $relative"
            }
        }
        Move-Item -LiteralPath $extracted -Destination $backend
    } finally {
        $resolvedStage = [IO.Path]::GetFullPath($stage)
        $resolvedCache = [IO.Path]::GetFullPath($cache).TrimEnd('\') + '\'
        if (-not $resolvedStage.StartsWith($resolvedCache, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected extraction path; refusing to clean it up.'
        }
        if (Test-Path -LiteralPath $resolvedStage) {
            Remove-Item -LiteralPath $resolvedStage -Recurse -Force
        }
    }
}

Copy-Item -LiteralPath (Join-Path $repo 'Setup/V8-LICENSE') -Destination (Join-Path $backend 'LICENSE') -Force
Write-Host 'Configuring PuerTS TypeScript tooling...'
& node (Join-Path $repo 'Setup/PrepareEditor.js')
if ($LASTEXITCODE -ne 0) { throw 'Could not prepare PuerTS editor files.' }
& node (Join-Path $plugin 'enable_puerts_module.js')
if ($LASTEXITCODE -ne 0) { throw 'PuerTS TypeScript configuration failed.' }
& node (Join-Path $repo 'Setup/EnablePlugin.js')
if ($LASTEXITCODE -ne 0) { throw 'Could not enable PuerTS.' }
Write-Host 'PuerTS setup is complete.'
