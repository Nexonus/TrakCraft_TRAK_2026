param([switch]$DownloadOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $root '.tools'
New-Item -ItemType Directory -Force -Path $tools | Out-Null
function Download($url, $name) {
    $target = Join-Path $tools $name
    if (!(Test-Path -LiteralPath $target)) {
        Write-Host "Downloading $name ..."
        & curl.exe --fail --location --retry 3 --output $target $url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $url" }
    }
}
Download 'https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-windows-x86_64.zip' 'cmake.zip'
Download 'https://github.com/skeeto/w64devkit/releases/download/v2.4.0/w64devkit-x64-2.4.0.7z.exe' 'w64devkit.exe'
Download 'https://www.7-zip.org/a/7zr.exe' '7zr.exe'
if ($DownloadOnly) { exit 0 }
if ((Get-FileHash "$tools/w64devkit.exe" -Algorithm SHA256).Hash -ne '80678c29a56982d7d3b342f12a0060157df5654c38d82d4adc40704070d6aafb') {
    throw 'Compiler archive checksum mismatch'
}
if (!(Test-Path "$tools/cmake-3.31.8-windows-x86_64/bin/cmake.exe")) {
    Expand-Archive -LiteralPath "$tools/cmake.zip" -DestinationPath $tools -Force
}
if (!(Test-Path "$tools/w64devkit/bin/g++.exe")) {
    & "$tools/7zr.exe" x "$tools/w64devkit.exe" "-o$tools" -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Compiler extraction failed' }
}
Write-Host 'Portable CMake and GCC are ready in .tools. Run .\build.ps1'
