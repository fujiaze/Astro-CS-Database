# IPV DLL 编译脚本 (PowerShell)
# 用法: powershell -File build.ps1

# 全局强制UTF-8编码
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ErrorActionPreference = "Stop"

$GXX = "C:\msys64\mingw64\bin\g++.exe"
$env:Path = "C:\msys64\mingw64\bin;" + $env:Path

$IPV_DIR = $PSScriptRoot
Set-Location $IPV_DIR

$SRC_DIR = "src"
$OBJ_DIR = "obj"
if (-not (Test-Path $OBJ_DIR)) { New-Item -ItemType Directory -Path $OBJ_DIR | Out-Null }

$SRCS = @(
    "ipv_select", "ipv_kvector", "ipv_polygon", "ipv_ransac",
    "ipv_angle", "ipv_wcs", "ipv_sip", "ipv_distortion",
    "ipv_triangle", "ipv_itertrans",
    "ipv_robust_refine",
    "ipv_solver", "ipv_entry"
)

	$CXXFLAGS = "-std=c++17", "-O3", "-funroll-loops", "-march=native", "-Wall", "-Wextra", "-DIPV_EXPORTS", "-D__USE_MINGW_ANSI_STDIO=1", "-mstackrealign", "-fopenmp"
$INCLUDES = "-Iinclude"

Write-Host "=== 编译 IPV DLL ===" -ForegroundColor Cyan

# 编译每个源文件
$has_error = $false
foreach ($src in $SRCS) {
    $src_file = Join-Path $SRC_DIR "$src.cpp"
    $obj_file = Join-Path $OBJ_DIR "$src.o"
    Write-Host "  编译 $src.cpp ... " -NoNewline

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $GXX
    $psi.Arguments = "$CXXFLAGS $INCLUDES -c `"$src_file`" -o `"$obj_file`""
    $psi.WorkingDirectory = $IPV_DIR
    $psi.UseShellExecute = $false
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardOutput = $true

    $p = [System.Diagnostics.Process]::Start($psi)
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()

    if ($p.ExitCode -eq 0) {
        Write-Host "OK" -ForegroundColor Green
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host $err
        $has_error = $true
    }
}

if ($has_error) {
    Write-Host "=== 编译失败 ===" -ForegroundColor Red
    exit 1
}

# 链接 DLL
Write-Host "  链接 ipv_solver.dll ... " -NoNewline
$obj_files = $SRCS | ForEach-Object { Join-Path $OBJ_DIR "$_.o" }
$obj_args = $obj_files -join " "

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $GXX
	$psi.Arguments = "-std=c++17 -O3 -funroll-loops -march=native -D__USE_MINGW_ANSI_STDIO=1 -fopenmp -shared $obj_args -o ipv_solver.dll -lkernel32"
$psi.WorkingDirectory = $IPV_DIR
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$psi.RedirectStandardOutput = $true

$p = [System.Diagnostics.Process]::Start($psi)
$err = $p.StandardError.ReadToEnd()
$p.WaitForExit()

if ($p.ExitCode -eq 0) {
    Write-Host "OK" -ForegroundColor Green
    $dll_size = (Get-Item "ipv_solver.dll").Length
    Write-Host "=== DLL 编译完成: ipv_solver.dll ($('{0:N0}' -f $dll_size) bytes) ===" -ForegroundColor Cyan
} else {
    Write-Host "FAIL" -ForegroundColor Red
    Write-Host $err
    exit 1
}
