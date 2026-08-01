@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM ai-commander - Open Solution Script
REM Calls dist setup scripts and opens ai-commander.slnx in Visual Studio
REM ============================================================

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR%"=="" (
    echo [Error] Failed to resolve script directory.
    exit /b 1
)

if "%SCRIPT_DIR:~-1%"=="\" (
    set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
)

REM This project lives in its own repository, not under the release tree's
REM dev\samples\sim\, so the tree is located by environment variable rather than by
REM walking up a fixed number of parent directories. Override N8RO_RELEASE_ROOT to
REM build against a release installed somewhere other than the default.
if not defined N8RO_RELEASE_ROOT set "N8RO_RELEASE_ROOT=C:\N8RO"

set "N8RO_RELEASE_SETUP=%N8RO_RELEASE_ROOT%\setup.cmd"
set "N8RO_RELEASE_DEV_SETUP=%N8RO_RELEASE_ROOT%\dev\setup-dev.cmd"
set "SOLUTION_FILE=%SCRIPT_DIR%\ai-commander.slnx"

if not exist "%N8RO_RELEASE_SETUP%" (
    echo [Error] dist setup script not found: %N8RO_RELEASE_SETUP%
    echo [Error] Set N8RO_RELEASE_ROOT to your N8RO release root and retry.
    exit /b 1
)

if not exist "%N8RO_RELEASE_DEV_SETUP%" (
    echo [Error] dev setup script not found: %N8RO_RELEASE_DEV_SETUP%
    exit /b 1
)

if not exist "%SOLUTION_FILE%" (
    echo [Error] Solution file not found: %SOLUTION_FILE%
    exit /b 1
)

call "%N8RO_RELEASE_SETUP%"
if errorlevel 1 (
    echo [Error] setup.cmd failed.
    exit /b 1
)

if not defined N8RO_SETUP_DONE (
    echo [Error] N8RO_SETUP_DONE is not set after setup.cmd.
    exit /b 1
)

if not defined N8RO_RELEASE (
    echo [Error] N8RO_RELEASE is not set after setup.cmd.
    exit /b 1
)

call "%N8RO_RELEASE_DEV_SETUP%"
if errorlevel 1 (
    echo [Error] setup-dev.cmd failed.
    exit /b 1
)

if not defined N8RO_RELEASE_DEVENV_CMD (
    echo [Error] Visual Studio executable not found.
    echo [Error] Set N8RO_RELEASE_DEVENV_CMD to a valid devenv.exe path, or install Visual Studio 18 2026 with the C++ workload.
    exit /b 1
)

if not exist "%N8RO_RELEASE_DEVENV_CMD%" (
    echo [Error] Visual Studio path is invalid: %N8RO_RELEASE_DEVENV_CMD%
    exit /b 1
)

echo [OK] Opening solution with: %N8RO_RELEASE_DEVENV_CMD%
start "" "%N8RO_RELEASE_DEVENV_CMD%" "%SOLUTION_FILE%"
if errorlevel 1 (
    echo [Error] Failed to launch Visual Studio.
    exit /b 1
)

echo [SUCCESS] ai-commander solution launch command sent.
exit /b 0
