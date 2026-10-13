@echo off
setlocal
cd /d "%~dp0"
echo ====================================================
echo  Variation Tree Analysis Editor - Universal Builder Launcher
echo ====================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0builder\build.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build encountered an error. Exit code: %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo [SUCCESS] Build process completed successfully!
pause
endlocal
