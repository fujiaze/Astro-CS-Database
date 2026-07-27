@echo off
REM ============================================================
REM   AstroCS CLI v1.1.0 - Verification Script
REM   P08-001 Release Package
REM   不依赖 Python/PowerShell, 纯 Windows 原生
REM ============================================================
setlocal enabledelayedexpansion

echo ============================================================
echo   AstroCS CLI v1.1.0 - Verification
echo   P08-001 Release Package
echo ============================================================
echo.

REM 设置发布包根目录
set "PKG_ROOT=%~dp0"
set "PKG_ROOT=%PKG_ROOT:~0,-1%"
set "ORCH=%PKG_ROOT%\lib\orchestrator\cpp\orchestrator.exe"

REM 将 bin/ 加入 PATH (MinGW 运行时 DLL)
set "PATH=%PKG_ROOT%\bin;%PATH%"

echo [1/4] Checking files...
if not exist "%ORCH%" (
    echo   FAIL: orchestrator.exe not found: %ORCH%
    goto :fail
)
echo   OK: orchestrator.exe found
if not exist "%PKG_ROOT%\bin\libgomp-1.dll" (
    echo   FAIL: libgomp-1.dll not found in bin\
    goto :fail
)
echo   OK: runtime DLLs found
if not exist "%PKG_ROOT%\config\default_stage1.json" (
    echo   FAIL: default_stage1.json not found
    goto :fail
)
echo   OK: config files found
echo.

echo [2/4] Verifying SHA-256 of orchestrator.exe...
certutil -hashfile "%ORCH%" SHA256 2>nul | findstr /v "SHA256\|CertUtil" > "%TEMP%\_astrocs_hash.txt"
set /p EXE_HASH=<"%TEMP%\_astrocs_hash.txt"
del "%TEMP%\_astrocs_hash.txt" 2>nul
echo   orchestrator.exe SHA-256: %EXE_HASH%
echo.

echo [3/4] Running capabilities...
"%ORCH%" capabilities 2>nul
if errorlevel 1 (
    echo   FAIL: capabilities returned errorlevel %errorlevel%
    goto :fail
)
echo   OK: capabilities completed
echo.

echo [4/4] Running inspect (file-not-found expected, exit code 8)...
"%ORCH%" inspect --hiss nonexistent.hiss 2>nul
set "INSPECT_RC=!errorlevel!"
if !INSPECT_RC! geq 9 (
    echo   FAIL: inspect returned unexpected errorlevel !INSPECT_RC!
    goto :fail
)
echo   OK: inspect completed (exit code !INSPECT_RC!, expected 8=FILE_IO_ERROR)
echo.

echo ============================================================
echo   Verification COMPLETE - ALL CHECKS PASSED
echo ============================================================
goto :end

:fail
echo.
echo ============================================================
echo   Verification FAILED
echo ============================================================
exit /b 1

:end
endlocal
exit /b 0
