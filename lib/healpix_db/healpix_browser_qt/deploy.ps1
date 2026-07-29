# deploy.ps1 - HEALPix 浏览器部署脚本
# 功能: 编译 + windeployqt 部署 + 复制 MinGW runtime DLL
# 用途: 编译后自动部署所有依赖, 使 exe 可双击启动
# 用法: pwsh -File deploy.ps1
# 依赖: MSYS2 MinGW64, Qt6, CMake

# 全局强制UTF-8编码
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ErrorActionPreference = "Stop"

# 路径设置
$mingwBin = "C:\msys64\mingw64\bin"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = "$scriptDir\build"
$exePath = "$buildDir\healpix_browser_qt.exe"

Write-Host "=== 1. 编译 ===" -ForegroundColor Cyan
$env:Path = "$mingwBin;$env:Path"
cd $buildDir
cmake --build . --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "编译失败" -ForegroundColor Red; exit 1 }

Write-Host "`n=== 2. windeployqt 部署 Qt6 DLL ===" -ForegroundColor Cyan
& "$mingwBin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exePath
if ($LASTEXITCODE -ne 0) { Write-Host "windeployqt 失败" -ForegroundColor Red; exit 1 }

Write-Host "`n=== 3. 复制 MinGW runtime 和 Qt6 间接依赖 DLL ===" -ForegroundColor Cyan
# MinGW runtime + Qt6 间接依赖 (windeployqt 不部署这些)
$neededDlls = @(
    # MinGW runtime
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll",
    "libstdc++-6.dll",
    # astro_image_io.dll 依赖 (OpenMP + LZ4 压缩)
    "libgomp-1.dll",
    "liblz4.dll",
    # Qt6Core 依赖
    "libb2-1.dll",
    "libdouble-conversion.dll",
    "libicuin78.dll",
    "libicuuc78.dll",
    "libicudt78.dll",
    "libpcre2-16-0.dll",
    "zlib1.dll",
    "libzstd.dll",
    # Qt6Gui 依赖
    "libfreetype-6.dll",
    "libharfbuzz-0.dll",
    "libmd4c.dll",
    "libpng16-16.dll",
    "libbz2-1.dll",
    "libgraphite2.dll",
    "libglib-2.0-0.dll",
    # 间接依赖
    "libbrotlidec.dll",
    "libbrotlicommon.dll",
    "libintl-8.dll",
    "libpcre2-8-0.dll",
    "libiconv-2.dll"
)

foreach ($dll in $neededDlls) {
    $src = "$mingwBin\$dll"
    if (Test-Path $src) {
        Copy-Item $src "$buildDir\$dll" -Force
        Write-Host "  已复制: $dll"
    } else {
        Write-Host "  警告: 未找到 $dll (可能不需要)" -ForegroundColor Yellow
    }
}

# 复制 astro_image_io.dll (提供 healpix_io 兼容 API)
$aioDll = "f:\Astro dev\Astro CS Normalization Database\lib\astro_image_io\astro_image_io.dll"
if (Test-Path $aioDll) {
    Copy-Item $aioDll "$buildDir\astro_image_io.dll" -Force
    Write-Host "  已复制: astro_image_io.dll"
} else {
    Write-Host "  警告: 未找到 astro_image_io.dll (请先构建 lib/astro_image_io/)" -ForegroundColor Yellow
}

Write-Host "`n=== 4. 验证双击启动 ===" -ForegroundColor Cyan
# 用最小 PATH 模拟双击
$env:Path = "C:\Windows\System32;C:\Windows\System32\wbem;C:\Windows"
$env:QT_PLUGIN_PATH = $null
$p = Start-Process -FilePath $exePath -PassThru -NoNewWindow
Start-Sleep -Seconds 3
if ($p.HasExited) {
    Write-Host "失败: 退出码=$($p.ExitCode)" -ForegroundColor Red
    exit 1
} else {
    Write-Host "成功: 双击启动正常 (PID=$($p.Id))" -ForegroundColor Green
    Stop-Process -Id $p.Id -Force
}

Write-Host "`n=== 部署完成 ===" -ForegroundColor Green
Write-Host "exe 路径: $exePath"
Write-Host "双击 exe 即可启动, 不需要任何环境变量"
