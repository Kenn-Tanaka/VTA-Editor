@echo off
cd /d "%~dp0"

if exist "%~dp0vta-starter.exe" (
    "%~dp0vta-starter.exe"
    goto :end
)

if exist "%~dp0tools\vta_starter\vta-starter.exe" (
    "%~dp0tools\vta_starter\vta-starter.exe"
    goto :end
)

where node >nul 2>nul
if %ERRORLEVEL% equ 0 (
    node "%~dp0scripts\start-all.js"
    goto :end
)

if exist "C:\Program Files\nodejs\node.exe" (
    "C:\Program Files\nodejs\node.exe" "%~dp0scripts\start-all.js"
    goto :end
)

if exist "%LocalAppData%\Programs\node\node.exe" (
    "%LocalAppData%\Programs\node\node.exe" "%~dp0scripts\start-all.js"
    goto :end
)

echo [Error] vta-starter.exe or Node.js not found.
pause

:end
