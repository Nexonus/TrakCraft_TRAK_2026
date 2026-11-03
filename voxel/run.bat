@echo off
cd /d "%~dp0"
if exist "build\voxel.exe" (
    "build\voxel.exe"
) else if exist "build\Release\voxel.exe" (
    "build\Release\voxel.exe"
) else (
    echo Najpierw zbuduj projekt: powershell -ExecutionPolicy Bypass -File build.ps1
    pause
)
