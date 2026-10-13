@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ================================================================
echo    LLM Gateway Credential Manager
echo ================================================================
echo Starting Credential Manager...
echo.

if exist "llm-gateway-credential-manager.exe" (
    start "" "llm-gateway-credential-manager.exe"
) else if exist "tools\llm-gateway\llm-gateway-credential-manager.exe" (
    start "" "tools\llm-gateway\llm-gateway-credential-manager.exe"
) else (
    echo [Error] llm-gateway-credential-manager.exe was not found.
    pause
)
