<#
.SYNOPSIS
    P11-004 诊断: 对 8 帧 gate 失败帧运行 WCS 重投影可视化
.DESCRIPTION
    使用 visualize_reproject.py (--save-solved-fits) 对 P11-003 gate 失败的 8 帧
    生成 Gaia 逆向投影 PNG + 带 WCS 的 FITS, 输出到项目根目录。
    每帧运行后重命名为 {frame_id}_reproject.png / {frame_id}_solved.fits。
.NOTES
    8 帧失败帧:
      T2: RED/GREEN/BLUE/HA_LDN43 (4)
      T3: RED/GREEN/BLUE/LUM_NGC55 (4)
#>

$ErrorActionPreference = "Continue"
$root = "f:\Astro dev\Astro CS Normalization Database"
$scriptPath = "$root\lib\plate_solve\python\visualize_reproject.py"

# 8 帧失败帧 (frame_id -> light_path 相对路径)
$frames = @(
    @{ id = "T2_RED_LDN43";   path = "testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts" }
    @{ id = "T2_GREEN_LDN43"; path = "testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts" }
    @{ id = "T2_BLUE_LDN43";  path = "testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts" }
    @{ id = "T2_HA_LDN43";    path = "testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts" }
    @{ id = "T3_RED_NGC55";   path = "testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts" }
    @{ id = "T3_GREEN_NGC55"; path = "testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@075153-600S-Green.fts" }
    @{ id = "T3_BLUE_NGC55";  path = "testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@080333-600S-Blue.fts" }
    @{ id = "T3_LUM_NGC55";   path = "testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts" }
)

$logFile = "$root\engineering_v1.2\evidence\P11-004\raw_logs\run_diagnosis.log"
$logDir = Split-Path $logFile -Parent
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }

function Write-Log {
    param([string]$msg)
    $line = "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] $msg"
    Write-Host $line
    Add-Content -Path $logFile -Value $line -Encoding UTF8
}

Write-Log "============================================================"
Write-Log "P11-004 WCS 重投影诊断启动"
Write-Log "输出目录: $root"
Write-Log "脚本: $scriptPath"
Write-Log "待处理帧数: $($frames.Count)"
Write-Log "============================================================"

$results = @()
$i = 0
foreach ($f in $frames) {
    $i++
    $frameId = $f.id
    $fitsPath = Join-Path $root $f.path
    $basename = [System.IO.Path]::GetFileNameWithoutExtension($fitsPath)

    Write-Log ""
    Write-Log "[$i/$($frames.Count)] $frameId"
    Write-Log "  源 FITS: $fitsPath"

    if (-not (Test-Path $fitsPath)) {
        Write-Log "  !!! 源文件不存在, 跳过"
        $results += @{ frame_id = $frameId; success = $false; error = "源文件不存在" }
        continue
    }

    # 运行 visualize_reproject.py
    $args = @("--single", "--input", $fitsPath, "--output-dir", $root, "--save-solved-fits")
    Write-Log "  运行: python visualize_reproject.py $args"
    $output = & python $scriptPath @args 2>&1
    $output | ForEach-Object { Write-Log "    $_" }

    # 重命名输出文件
    $srcPng = Join-Path $root "$basename`_reproject.png"
    $srcFits = Join-Path $root "$basename`_solved.fits"
    $dstPng = Join-Path $root "$frameId`_reproject.png"
    $dstFits = Join-Path $root "$frameId`_solved.fits"

    if (Test-Path $srcPng) {
        if (Test-Path $dstPng) { Remove-Item $dstPng -Force }
        Rename-Item $srcPng $dstPng -Force
        Write-Log "  PNG: $dstPng"
    } else {
        Write-Log "  !!! PNG 未生成: $srcPng"
    }

    if (Test-Path $srcFits) {
        if (Test-Path $dstFits) { Remove-Item $dstFits -Force }
        Rename-Item $srcFits $dstFits -Force
        Write-Log "  FITS: $dstFits"
    } else {
        Write-Log "  !!! solved FITS 未生成: $srcFits"
    }

    $success = (Test-Path $dstPng)
    $results += @{ frame_id = $frameId; success = $success; png = $dstPng; fits = $dstFits }
}

Write-Log ""
Write-Log "============================================================"
Write-Log "诊断完成: $($results.Where({ $_.success }).Count)/$($results.Count) 成功"
Write-Log "输出文件列表:"
foreach ($r in $results) {
    $status = if ($r.success) { "OK" } else { "FAIL" }
    Write-Log "  [$status] $($r.frame_id)"
}
Write-Log "============================================================"

# 输出 summary JSON
$summary = @{
    task = "P11-004 diagnosis"
    timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    total_frames = $results.Count
    n_success = $results.Where({ $_.success }).Count
    frames = $results
}
$summaryPath = "$root\engineering_v1.2\evidence\P11-004\reports\diagnosis_summary.json"
$summaryDir = Split-Path $summaryPath -Parent
if (-not (Test-Path $summaryDir)) { New-Item -ItemType Directory -Path $summaryDir -Force | Out-Null }
$summary | ConvertTo-Json -Depth 5 | Out-File -FilePath $summaryPath -Encoding UTF8
Write-Log "Summary: $summaryPath"
