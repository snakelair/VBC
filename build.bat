@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo VBC Native Compiler Build Script (Pure C99 Bootstrap)
echo ===================================================

if "%1"=="clean" (
    echo Cleaning build artifacts...
    if exist vbc.exe del /f /q vbc.exe
    if exist *.pdb del /f /q *.pdb
    if exist temp_* del /f /q temp_*
    echo Done.
    exit /b 0
)

if "%1"=="rebuild" (
    if not exist vbc.exe (
        echo Error: vbc.exe not found to perform self-hosted rebuild.
        echo Run 'build.bat' first to build from bootstrap.
        exit /b 1
    )
    echo Performing self-hosted recompilation of src/ ...
    vbc compile src -o vbc_new.exe --emit-c
    if %errorlevel% neq 0 (
        echo Self-hosted compilation failed.
        exit /b %errorlevel%
    )
    move /y vbc_new.exe vbc.exe >nul
    if exist vbc_new.exe.c (
        copy /y vbc_new.exe.c bootstrap\vbc_bootstrap.c >nul
        del /f /q vbc_new.exe.c
    )
    echo Self-hosted rebuild complete: vbc.exe and bootstrap/vbc_bootstrap.c updated!
    exit /b 0
)

echo Checking for C compiler (clang / gcc)...
where clang >nul 2>nul
if %errorlevel% equ 0 (
    echo Building vbc.exe with Clang from bootstrap/vbc_bootstrap.c...
    clang -O2 -w bootstrap/vbc_bootstrap.c runtime/src/*.c -I runtime/include -lws2_32 -lkernel32 -o vbc.exe
    if %errorlevel% neq 0 (
        echo Clang build failed!
        exit /b %errorlevel%
    )
    echo Build successful: vbc.exe
    exit /b 0
)

where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Building vbc.exe with GCC from bootstrap/vbc_bootstrap.c...
    gcc -O2 -w bootstrap/vbc_bootstrap.c runtime/src/*.c -I runtime/include -lws2_32 -lkernel32 -o vbc.exe
    if %errorlevel% neq 0 (
        echo GCC build failed!
        exit /b %errorlevel%
    )
    echo Build successful: vbc.exe
    exit /b 0
)

echo Error: Neither clang nor gcc found in PATH. Please install Clang (LLVM) or GCC.
exit /b 1
