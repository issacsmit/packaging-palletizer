@echo off
setlocal
chcp 65001 >nul

if not exist bin mkdir bin

g++ -std=c++17 -O2 -Wall -Wextra -mconsole -finput-charset=UTF-8 -fexec-charset=UTF-8 src\main.cpp -o bin\palletizer.exe -lgdi32 -luser32
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Build success: bin\palletizer.exe
