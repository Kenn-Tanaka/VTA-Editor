@echo off
setlocal
echo Compiling LLM Test CLI (C++)...
g++ -O2 -std=c++17 main.cpp -static -static-libgcc -static-libstdc++ -lwinhttp -o llm_test.exe
if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] llm_test.exe has been compiled successfully.
    echo.
    echo Usage:
    echo   llm_test.exe --models
    echo   llm_test.exe --model openrouter/anthropic/claude-sonnet-5
    echo   llm_test.exe --model lmstudio/local-model
) else (
    echo [ERROR] Compilation failed.
)
endlocal
