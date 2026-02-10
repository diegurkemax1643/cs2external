@echo off
cd /d "%~dp0"

echo Finding Visual Studio...
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VS_PATH=%%i

if "%VS_PATH%"=="" (
    echo Visual Studio not found. Trying default paths...
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
    ) else (
        echo ERROR: MSBuild not found. Please build manually in Visual Studio.
        pause
        exit /b 1
    )
) else (
    set MSBUILD="%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe"
)

echo Building in Release mode...
%MSBUILD% "External base Counter-Strike2.sln" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build succeeded!
) else (
    echo.
    echo Build failed with errors. Check the output above.
)

pause

