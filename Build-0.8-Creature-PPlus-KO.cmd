@echo off
setlocal
cd /d "%~dp0"
call build.cmd || exit /b 1
if not exist "build\release\0_OStrands.dll" (
  echo DLL not found at build\release\0_OStrands.dll
  exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Package-DLL-Only-0.8.ps1"
exit /b %ERRORLEVEL%
