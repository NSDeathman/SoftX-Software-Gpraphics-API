@echo off
setlocal

set "GENERATED_DIR=%~dp0generated"
set "BUILD_DIR=%GENERATED_DIR%\VS2019_x64"
set "SOURCE_DIR=%~dp0.."

echo ========================================
echo Generating Visual Studio 2022 x64 solution
echo Source : %SOURCE_DIR%
echo Build  : %BUILD_DIR%
echo ========================================

cmake -G "Visual Studio 17 2022" -A x64 -S "%SOURCE_DIR%" -B "%BUILD_DIR%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Solution generated successfully!
    echo Open "%BUILD_DIR%\SoftXSolution.sln"

    set "CLANG_FORMAT_FILE=%~dp0..\.clang-format"
    if exist "%CLANG_FORMAT_FILE%" (
        copy /Y "%CLANG_FORMAT_FILE%" "%BUILD_DIR%\" >nul
        if !ERRORLEVEL! EQU 0 (
            echo Copied .clang-format to build directory.
        ) else (
            echo Failed to copy .clang-format.
        )
    ) else (
        echo Warning: .clang-format not found at "%CLANG_FORMAT_FILE%". Skipping copy.
    )
) else (
    echo.
    echo ERROR: CMake generation failed.
    pause
)
endlocal
pause
