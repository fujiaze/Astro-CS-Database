# build.ps1 - HEALPix 浏览器 C++ 后端构建脚本
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1
# 功能: 编译 browser_cpp.exe, 自动复制 healpix_io.dll 到输出目录

# 全局强制UTF-8编码（覆盖系统默认GBK）
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try { [System.Text.Encoding]::Default = [System.Text.Encoding]::UTF8 } catch {}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$gpp = "C:\msys64\mingw64\bin\g++.exe"
$mingwBin = "C:\msys64\mingw64\bin"

if (-not (Test-Path $gpp)) {
    Write-Host "Error: g++ not found. Please install MSYS2 MinGW64." -ForegroundColor Red
    Write-Host "Expected path: $gpp" -ForegroundColor Yellow
    exit 1
}

# 把 mingw64/bin 加入 PATH (g++ 需要找到 cc1plus/ld 等子进程)
if (Test-Path $mingwBin) {
    $env:Path = "$mingwBin;" + $env:Path
}

Write-Host "=== HEALPix 浏览器 C++ 后端构建 ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

Set-Location $scriptDir

# 检查 healpix_io.dll 是否存在
$hioDir = Join-Path $scriptDir "..\healpix_io"
$hioDll = Join-Path $hioDir "healpix_io.dll"
$hioInc = Join-Path $hioDir "include\healpix_io.h"

if (-not (Test-Path $hioDll)) {
    Write-Host "Error: healpix_io.dll 未找到, 请先编译 healpix_io 模块" -ForegroundColor Red
    Write-Host "Expected: $hioDll" -ForegroundColor Yellow
    Write-Host "请在 $hioDir 目录下执行 build.ps1" -ForegroundColor Yellow
    exit 1
}
if (-not (Test-Path $hioInc)) {
    Write-Host "Error: healpix_io.h 未找到" -ForegroundColor Red
    Write-Host "Expected: $hioInc" -ForegroundColor Yellow
    exit 1
}

Write-Host "依赖检查通过:"
Write-Host "  healpix_io.dll: $hioDll"
Write-Host "  healpix_io.h:   $hioInc"
Write-Host ""

$outputExe = "browser_cpp.exe"

# 源文件列表
$srcs = @(
    "src\browser_backend.cpp",
    "src\http_server.cpp",
    "src\browser_main.cpp"
)

# 检查所有源文件存在
foreach ($src in $srcs) {
    $fullSrc = Join-Path $scriptDir $src
    if (-not (Test-Path $fullSrc)) {
        Write-Host "Error: 源文件不存在: $src" -ForegroundColor Red
        exit 1
    }
}

Write-Host "编译目标: $outputExe"
Write-Host "源文件:"
foreach ($src in $srcs) { Write-Host "  - $src" }
Write-Host ""

# healpix_io 头文件目录
$hioIncludeDir = Join-Path $hioDir "include"

# 调用 g++ 编译
# 注意: -lhealpix_io 必须在源文件之后, 否则链接器找不到符号
# -lws2_32 链接 winsock2
$output = & $gpp -O2 -std=c++17 -Wall -Wextra `
    -Iinclude `
    -I"$hioIncludeDir" `
    -o $outputExe `
    $srcs `
    -L"$hioDir" -lhealpix_io -lws2_32 2>&1

$exitCode = $LASTEXITCODE

if ($output) {
    Write-Host "---- 编译器输出 ----" -ForegroundColor Yellow
    $output | ForEach-Object { Write-Host $_ }
}

if ($exitCode -eq 0 -and (Test-Path (Join-Path $scriptDir $outputExe))) {
    $exeInfo = Get-Item (Join-Path $scriptDir $outputExe)
    Write-Host ""
    Write-Host "编译成功!" -ForegroundColor Green
    Write-Host "  EXE: $($exeInfo.FullName)"
    Write-Host "  Size: $([math]::Round($exeInfo.Length / 1KB, 1)) KB"

    # 复制 healpix_io.dll 到当前目录 (运行时需要)
    $destDll = Join-Path $scriptDir "healpix_io.dll"
    Write-Host ""
    Write-Host "复制 healpix_io.dll 到构建目录..." -ForegroundColor Cyan
    Copy-Item -Path $hioDll -Destination $destDll -Force
    if (Test-Path $destDll) {
        $dllInfo = Get-Item $destDll
        Write-Host "  DLL 已复制: $($dllInfo.FullName)" -ForegroundColor Green
        Write-Host "  Size: $([math]::Round($dllInfo.Length / 1KB, 1)) KB" -ForegroundColor Green
    } else {
        Write-Host "  警告: DLL 复制失败, 运行时可能找不到 healpix_io.dll" -ForegroundColor Yellow
    }

    # 复制 mingw 运行时 DLL 到构建目录 (使 browser_cpp.exe 可独立运行)
    Write-Host ""
    Write-Host "复制 mingw 运行时 DLL 到构建目录..." -ForegroundColor Cyan
    $mingwDlls = @(
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    )
    foreach ($dllName in $mingwDlls) {
        $srcMingwDll = Join-Path $mingwBin $dllName
        $destMingwDll = Join-Path $scriptDir $dllName
        if (Test-Path $srcMingwDll) {
            Copy-Item -Path $srcMingwDll -Destination $destMingwDll -Force
            Write-Host "  $dllName 已复制" -ForegroundColor Green
        } else {
            Write-Host "  警告: $dllName 未找到 (来源: $srcMingwDll)" -ForegroundColor Yellow
        }
    }

    Write-Host ""
    Write-Host "构建完成!" -ForegroundColor Green
    Write-Host '用法: .\browser_cpp.exe <file.hiss|file.hcsd>' -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "构建失败! Exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}
