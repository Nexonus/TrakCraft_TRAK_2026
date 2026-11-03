param([switch]$Run, [switch]$CoreOnly)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$localCmake = Join-Path $PSScriptRoot '.tools/cmake-3.31.8-windows-x86_64/bin/cmake.exe'
$localGcc = Join-Path $PSScriptRoot '.tools/w64devkit/bin'
if (Test-Path $localGcc) { $env:PATH = "$localGcc;$env:PATH" }
$cmake = if (Test-Path $localCmake) { $localCmake } else { 'cmake' }
$ctest = if (Test-Path $localCmake) { Join-Path (Split-Path $localCmake) 'ctest.exe' } else { 'ctest' }
$appSetting = if ($CoreOnly) { 'OFF' } else { 'ON' }
$options = @('-S', '.', '-B', 'build', '-DCMAKE_BUILD_TYPE=Release', "-DVOXEL_BUILD_APP=$appSetting")
if (Test-Path $localGcc) { $options += @('-G', 'MinGW Makefiles', '-DCMAKE_MAKE_PROGRAM=make') }
& $cmake @options
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& $cmake --build build --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& $ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
if ($Run -and !$CoreOnly) {
    $exe = if (Test-Path 'build/Release/voxel.exe') { 'build/Release/voxel.exe' } else { 'build/voxel.exe' }
    & $exe
}
