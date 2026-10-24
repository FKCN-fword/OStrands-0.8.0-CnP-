@echo off
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
	echo Visual Studio C++ build tools not found.
	exit /b 1
)
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not defined VCPKG_ROOT set "VCPKG_ROOT=%VSPATH%\VC\vcpkg"

if defined SCCACHE_GHA_ENABLED (
	cmake --preset release -DCMAKE_CXX_COMPILER_LAUNCHER=sccache || exit /b 1
) else (
	cmake --preset release -DCMAKE_CXX_COMPILER_LAUNCHER= || exit /b 1
)
cmake --build --preset release || exit /b 1
echo.
echo Built: %~dp0build\release\0_OStrands.dll
