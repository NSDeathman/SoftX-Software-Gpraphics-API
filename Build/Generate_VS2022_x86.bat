@echo off
setlocal

set "GENERATED_DIR=%~dp0generated"
set "BUILD_DIR=%GENERATED_DIR%\VS2022_x86"
set "SOURCE_DIR=%~dp0.."

echo ========================================
echo Generating Visual Studio 2022 Win32 (x86) solution
echo Source : %SOURCE_DIR%
echo Build  : %BUILD_DIR%
echo ========================================

cmake -G "Visual Studio 17 2022" -A Win32 -S "%SOURCE_DIR%" -B "%BUILD_DIR%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Solution generated successfully!
    echo Open "%BUILD_DIR%\SoftXSolution.sln"
) else (
    echo.
    echo ERROR: CMake generation failed.
    pause
)
endlocal
pause
