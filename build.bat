@echo off
rem Builds Astral Throat (VST3 + Standalone + RenderTest) with the VS 2022 Build Tools.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist build\build.ninja (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build build %* || exit /b 1
set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if exist "%ISCC%" ("%ISCC%" /Q installer\AstralThroat.iss || exit /b 1) else (echo Inno Setup 6 not found: skipped the installer.)
echo BUILD OK
