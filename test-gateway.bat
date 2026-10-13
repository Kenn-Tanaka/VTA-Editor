@echo off
cd /d "%~dp0"

echo ================================================================
echo    LLM Gateway Connection Test CLI (llm_test)
echo ================================================================
echo Testing Gateway connection...
echo.

if exist "%~dp0llm_test.exe" (
    "%~dp0llm_test.exe" --models
    goto :end
)

if exist "%~dp0tools\llm_test_cli\llm_test.exe" (
    "%~dp0tools\llm_test_cli\llm_test.exe" --models
    goto :end
)

echo [Error] llm_test.exe was not found.

:end
echo.
pause
