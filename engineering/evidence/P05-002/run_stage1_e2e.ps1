#requires -Version 7.0
<#
.SYNOPSIS
  P05-002 Stage1 真实数据端到端验证 (v1.1 开发包) - v2 重写脚本
  使用 Start-Process -RedirectStandardOutput/-RedirectStandardError 可靠捕获 stdout/stderr
.NOTES
  工作目录: f:\Astro dev\Astro CS Normalization Database
  强制使用 PowerShell 7 运行环境
#>

$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$projectRoot = "f:\Astro dev\Astro CS Normalization Database"
# 注意: orchestrator.exe init_dlls() (orchestrator.cpp line 862) 硬编码假设 exe 位于
# <root>/lib/orchestrator/cpp/ 路径下 (向上 4 级得到 <root>).
# build/artifacts/orchestrator.exe 路径深度不同, 会导致项目根目录推导到 f:\Astro dev (错误),
# 进而 DLL 加载失败. P05-002 是端到端验证任务, 不修改业务源码, 故使用 lib/orchestrator/cpp/orchestrator.exe
# (最新编译版本 2026-07-25 20:03:27, 比 build/artifacts/ 19:03:11 更新).
$orchExe = Join-Path $projectRoot "lib\orchestrator\cpp\orchestrator.exe"
$evidenceDir = Join-Path $projectRoot "engineering\evidence\P05-002"
$framesDir = Join-Path $evidenceDir "frames"
$configsDir = Join-Path $evidenceDir "configs"
$hissDir = Join-Path $evidenceDir "hiss"

# DLL 依赖路径 (orchestrator.exe 同目录 + mingw64)
$dllPaths = @(
    (Split-Path $orchExe -Parent)
    "C:\msys64\mingw64\bin"
)
$env:Path = "$($dllPaths -join ';');$env:Path"

# 7 帧 canonical 数据集 (来自 P05-001)
$frames = @(
    @{ id = "P05-001-C001"; tel = "T4"; cfg = "stage1_config_T4.json"; filter = "Red"; exposure = 180.0;
       path = "testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts";
       out = "P05-001-C001_Galaxy_Center_T4_Red_180s.hiss" },
    @{ id = "P05-001-C002"; tel = "T2"; cfg = "stage1_config_T2.json"; filter = "Lum"; exposure = 600.0;
       path = "testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts";
       out = "P05-001-C002_LDN43_T2_Lum_600s.hiss" },
    @{ id = "P05-001-C003"; tel = "T2"; cfg = "stage1_config_T2.json"; filter = "Red"; exposure = 600.0;
       path = "testdata\NGC1727_T2_flying_dutchman\lights\NGC1727_RGBHO_T2_flying_dutchman-20251031@064517-600S-Red.fts";
       out = "P05-001-C003_NGC1727_T2_Red_600s.hiss" },
    @{ id = "P05-001-C004"; tel = "T2"; cfg = "stage1_config_T2.json"; filter = "Lum"; exposure = 600.0;
       path = "testdata\NGC247_T2_flying_dutchman\lights\NGC247_T2_flying_dutchman-20250816@033428-600S-Lum.fts";
       out = "P05-001-C004_NGC247_T2_Lum_600s.hiss" },
    @{ id = "P05-001-C005"; tel = "T3"; cfg = "stage1_config_T3.json"; filter = "Red"; exposure = 600.0;
       path = "testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts";
       out = "P05-001-C005_NGC55_T3_Red_600s.hiss" },
    @{ id = "P05-001-C006"; tel = "T3"; cfg = "stage1_config_T3.json"; filter = "Red"; exposure = 600.0;
       path = "testdata\NGC83_cluster_T3_Flying_Dutchman\lights\NGC90_2025wwk_T3_flying_dutchman-20251011@020846-600S-Red.fts";
       out = "P05-001-C006_NGC83_cluster_T3_Red_600s.hiss" },
    @{ id = "P05-001-C007"; tel = "T4"; cfg = "stage1_config_T4.json"; filter = "Lum"; exposure = 180.0;
       path = "testdata\Victory_Nebula_T4_Flying_Dutchman\lights\Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts";
       out = "P05-001-C007_Victory_Nebula_T4_Lum_180s.hiss" }
)

# 辅助函数: 从 stderr 日志中提取关键指标
function Extract-MetricsFromStderr {
    param([string]$stderr)
    $metrics = @{
        platesolve_success = $null
        platesolve_rms = $null
        platesolve_n_pairs = $null
        psf_n_success = $null
        psf_n_total = $null
        photometric_n_matched = $null
        photometric_scale_factor = $null
        photometric_sigma_residual = $null
        snr_status = $null
        snr_median_psf = $null
        drizzle_n_healpix = $null
        drizzle_elapsed = $null
        calibrate_status = $null
        master_bias_mean = $null
        master_dark_mean = $null
        master_flat_mean = $null
        flat_missing = $false
        stage_success = @{}
        stage_durations = @{}
    }

    if (-not $stderr) { return $metrics }

    # PlateSolve: "PlateSolve: success=1 RMS=0.332865\" n_pairs=45" 或 "PlateSolve success=true RMS=..."
    if ($stderr -match 'PlateSolve[:\s]+success=(true|1|false|0)[\s,]+RMS=([0-9.eE+-]+)["\s,]+n_pairs=(\d+)') {
        $metrics.platesolve_success = ($matches[1] -in @('true','1'))
        $metrics.platesolve_rms = [double]$matches[2]
        $metrics.platesolve_n_pairs = [int]$matches[3]
    } elseif ($stderr -match 'RMS=([0-9.eE+-]+)["\s,]+n_pairs=(\d+)') {
        $metrics.platesolve_rms = [double]$matches[1]
        $metrics.platesolve_n_pairs = [int]$matches[2]
    }

    # PSF: "PSF: 1913/2000 stars" 或 "psf_fit: n_success=1913"
    if ($stderr -match 'PSF[:\s]+(\d+)\s*/\s*(\d+)\s*stars') {
        $metrics.psf_n_success = [int]$matches[1]
        $metrics.psf_n_total = [int]$matches[2]
    } elseif ($stderr -match 'psf[:\s]+n_success=(\d+)') {
        $metrics.psf_n_success = [int]$matches[1]
    }

    # PHOTOMETRIC: "n_matched=1606" "scale_factor=7.13e-03" "sigma_residual=0.171"
    if ($stderr -match 'N_MATCHED=(\d+)') { $metrics.photometric_n_matched = [int]$matches[1] }
    if ($stderr -match 'n_matched=(\d+)') { $metrics.photometric_n_matched = [int]$matches[1] }
    if ($stderr -match 'SCALE_FACTOR=([0-9.eE+-]+)') { $metrics.photometric_scale_factor = [double]$matches[1] }
    if ($stderr -match 'scale_factor=([0-9.eE+-]+)') { $metrics.photometric_scale_factor = [double]$matches[1] }
    if ($stderr -match 'SIGMA_RESIDUAL=([0-9.eE+-]+)') { $metrics.photometric_sigma_residual = [double]$matches[1] }
    if ($stderr -match 'sigma_residual=([0-9.eE+-]+)') { $metrics.photometric_sigma_residual = [double]$matches[1] }

    # SNR
    if ($stderr -match 'SNR_STATUS=(\w+)') { $metrics.snr_status = $matches[1] }
    if ($stderr -match 'snr[:\s]+(degraded|ok|success)') { $metrics.snr_status = $matches[1] }
    if ($stderr -match 'median SNR_psf=([0-9.eE+-]+)') { $metrics.snr_median_psf = [double]$matches[1] }
    if ($stderr -match 'SNR_phot=([0-9.eE+-]+)') { $metrics.snr_median_psf = [double]$matches[1] }

    # Drizzle
    if ($stderr -match 'n_healpix_pixels=(\d+)') { $metrics.drizzle_n_healpix = [int64]$matches[1] }
    if ($stderr -match 'drizzle.*elapsed_sec=([0-9.eE+-]+)') { $metrics.drizzle_elapsed = [double]$matches[1] }

    # CALIBRATE
    if ($stderr -match 'STATUS=(\w+)') { $metrics.calibrate_status = $matches[1] }
    if ($stderr -match 'MASTER_BIAS_MEAN=([0-9.eE+-]+)') { $metrics.master_bias_mean = [double]$matches[1] }
    if ($stderr -match 'MASTER_DARK_MEAN=([0-9.eE+-]+)') { $metrics.master_dark_mean = [double]$matches[1] }
    if ($stderr -match 'MASTER_FLAT_MEAN=([0-9.eE+-]+)') { $metrics.master_flat_mean = [double]$matches[1] }
    if ($stderr -match 'Master 文件不存在.*masterFlat.*Lum') { $metrics.flat_missing = $true }
    if ($stderr -match 'master_flat_path.*Lum.*不存在|FILTER-Lum.*不存在') { $metrics.flat_missing = $true }
    # 更通用: 找到 "Master 文件不存在: ...FILTER-Lum..." 行
    $noFlatLines = $stderr -split "`n" | Where-Object { $_ -match 'Master 文件不存在' -and $_ -match 'FILTER-Lum' }
    if ($noFlatLines) { $metrics.flat_missing = $true }

    # 各阶段成功标记 + 耗时
    # 格式: "stage 0 (READ_FITS) success=1 elapsed=0.123s" 或类似
    $stagePattern = 'stage\s+(\d+)\s*\((\w+)\)\s+(?:success|result)=?(true|false|1|0).*?elapsed=([0-9.eE+-]+)s'
    foreach ($line in ($stderr -split "`n")) {
        if ($line -match $stagePattern) {
            $stageNum = [int]$matches[1]
            $stageName = $matches[2]
            $success = $matches[3 -in @('true','1')
            $elapsed = [double]$matches[4]
            $metrics.stage_success[$stageName] = $success
            $metrics.stage_durations[$stageName] = $elapsed
        }
    }

    return $metrics
}

# 辅助函数: 用 inspect --hiss 验证 HISS 元数据
function Invoke-InspectHiss {
    param([string]$hissPath)
    if (-not (Test-Path $hissPath)) { return $null }

    $tmpOut = [System.IO.Path]::GetTempFileName()
    $tmpErr = [System.IO.Path]::GetTempFileName()
    try {
        $proc = Start-Process -FilePath $orchExe `
            -ArgumentList @("inspect", "--hiss", $hissPath) `
            -WorkingDirectory $projectRoot `
            -RedirectStandardOutput $tmpOut `
            -RedirectStandardError $tmpErr `
            -NoNewWindow -Wait -PassThru
        $stdout = Get-Content $tmpOut -Raw -ErrorAction SilentlyContinue
        $stderr = Get-Content $tmpErr -Raw -ErrorAction SilentlyContinue

        # stdout 含多个 JSONL 事件, 取最后一个 result 事件
        $inspectResult = $null
        if ($stdout) {
            $lines = $stdout -split "`n" | Where-Object { $_.Trim() -ne '' }
            foreach ($line in ($lines | Select-Object -Last 3)) {
                try {
                    $j = $line | ConvertFrom-Json
                    if ($j.type -eq 'result' -or $j.result) {
                        $inspectResult = $j
                        break
                    }
                } catch {}
            }
        }
        return @{
            exit_code = $proc.ExitCode
            stdout = $stdout
            stderr = $stderr
            result = $inspectResult
        }
    } finally {
        Remove-Item $tmpOut, $tmpErr -Force -ErrorAction SilentlyContinue
    }
}

$results = @()
$summary = @{
    task_id = "P05-002"
    started_at = (Get-Date).ToString("o")
    total_frames = $frames.Count
    orchestrator_exe = $orchExe
    frames = @()
}

Write-Host "============================================================"
Write-Host "P05-002 Stage1 真实数据端到端验证 (v2 - Start-Process 重定向)"
Write-Host "============================================================"
Write-Host "项目根目录: $projectRoot"
Write-Host "orchestrator.exe: $orchExe"
Write-Host "canonical 帧数量: $($frames.Count)"
Write-Host ""

# 逐帧运行 stage1
foreach ($frame in $frames) {
    $frameId = $frame.id
    $cfgFile = $frame.cfg
    $framePath = Join-Path $projectRoot $frame.path
    $cfgPath = Join-Path $configsDir $cfgFile
    $outputHiss = Join-Path $hissDir $frame.out
    $frameLogDir = Join-Path $framesDir $frameId

    if (-not (Test-Path $frameLogDir)) {
        New-Item -ItemType Directory -Path $frameLogDir -Force | Out-Null
    }

    $stdoutFile = Join-Path $frameLogDir "stage1_stdout.jsonl"
    $stderrFile = Join-Path $frameLogDir "stage1_stderr.log"
    $metaFile = Join-Path $frameLogDir "stage1_meta.json"
    $inspectFile = Join-Path $frameLogDir "inspect_hiss.jsonl"
    $inspectErrFile = Join-Path $frameLogDir "inspect_hiss_stderr.log"

    Write-Host "------------------------------------------------------------"
    Write-Host "[$frameId] 望远镜: $($frame.tel) | 滤镜: $($frame.filter) | 曝光: $($frame.exposure)s"
    Write-Host "[$frameId] 帧文件: $(Split-Path $frame.path -Leaf)"
    Write-Host "[$frameId] 配置: $cfgFile"
    Write-Host "[$frameId] 输出 HISS: $outputHiss"
    Write-Host "------------------------------------------------------------"

    # 检查帧文件存在
    if (-not (Test-Path $framePath)) {
        Write-Error "[$frameId] 帧文件不存在: $framePath"
        $result = @{
            dataset_id = $frameId
            success = $false
            exit_code = 8
            error_msg = "帧文件不存在: $framePath"
            elapsed_sec = 0
            hiss_exists = $false
        }
        $results += $result
        $summary.frames += $result
        $result | ConvertTo-Json -Depth 5 | Out-File -FilePath $metaFile -Encoding utf8
        continue
    }

    # 删除已存在 HISS (避免旧文件干扰)
    if (Test-Path $outputHiss) { Remove-Item $outputHiss -Force }

    # 清空旧日志文件
    if (Test-Path $stdoutFile) { Remove-Item $stdoutFile -Force }
    if (Test-Path $stderrFile) { Remove-Item $stderrFile -Force }

    $args = @(
        "stage1",
        "--frame", $framePath,
        "--output", $outputHiss,
        "--config", $cfgPath,
        "--log-level", "INFO"
    )

    Write-Host "[$frameId] 命令: orchestrator.exe $($args -join ' ')"
    Write-Host "[$frameId] 开始运行 stage1 ..."

    $startTime = Get-Date

    # 用 Start-Process 可靠重定向 stdout/stderr 到文件
    try {
        $proc = Start-Process -FilePath $orchExe `
            -ArgumentList $args `
            -WorkingDirectory $projectRoot `
            -RedirectStandardOutput $stdoutFile `
            -RedirectStandardError $stderrFile `
            -NoNewWindow -Wait -PassThru
        $exitCode = $proc.ExitCode
    } catch {
        $exitCode = -1
        "异常: $_" | Out-File -FilePath $stderrFile -Encoding utf8 -Append
    }

    $endTime = Get-Date
    $elapsed = ($endTime - $startTime).TotalSeconds

    # 读取 stdout/stderr 内容
    $stdoutContent = ""
    $stderrContent = ""
    if (Test-Path $stdoutFile) {
        $stdoutContent = Get-Content $stdoutFile -Raw -ErrorAction SilentlyContinue
    }
    if (Test-Path $stderrFile) {
        $stderrContent = Get-Content $stderrFile -Raw -ErrorAction SilentlyContinue
    }

    # 从 stderr 提取关键指标
    $metrics = Extract-MetricsFromStderr -stderr $stderrContent

    # 计算 HISS 文件 SHA-256 + 大小
    $hissHash = ""
    $hissSize = 0
    $hissExists = Test-Path $outputHiss
    if ($hissExists) {
        $hissFile = Get-Item $outputHiss
        $hissSize = $hissFile.Length
        $hissHash = (Get-FileHash $outputHiss -Algorithm SHA256).Hash
    }

    # 用 inspect --hiss 验证 HISS 元数据
    $inspectResult = $null
    if ($hissExists) {
        Write-Host "[$frameId] 调用 inspect --hiss 验证 HISS 元数据 ..."
        $inspectOutput = Invoke-InspectHiss -hissPath $outputHiss
        if ($inspectOutput) {
            $inspectOutput.stdout | Out-File -FilePath $inspectFile -Encoding utf8 -NoNewline
            $inspectOutput.stderr | Out-File -FilePath $inspectErrFile -Encoding utf8
            $inspectResult = $inspectOutput.result
        }
    }

    # 构建帧结果
    $frameResult = @{
        dataset_id = $frameId
        telescope = $frame.tel
        filter = $frame.filter
        exposure_s = $frame.exposure
        config_file = $cfgFile
        frame_path = $frame.path
        frame_path_absolute = $framePath
        output_hiss_path = $outputHiss
        output_hiss_relative = "engineering\evidence\P05-002\hiss\$($frame.out)"
        success = ($exitCode -eq 0)
        exit_code = $exitCode
        elapsed_sec = [math]::Round($elapsed, 3)
        hiss_exists = $hissExists
        hiss_size_bytes = $hissSize
        hiss_size_kb = [math]::Round($hissSize / 1KB, 2)
        hiss_sha256 = $hissHash
        error_msg = ""
        stdout_lines = ($stdoutContent -split "`n").Count
        stderr_lines = ($stderrContent -split "`n").Count
        metrics = $metrics
        inspect = $null
    }

    if (-not $frameResult.success) {
        # 从 stderr 末尾提取错误信息
        $errLines = $stderrContent -split "`n" | Where-Object { $_ -match '\[ERROR\]' } | Select-Object -Last 5
        $frameResult.error_msg = ($errLines -join " | ")
        if (-not $frameResult.error_msg) {
            $frameResult.error_msg = "stage1 失败 (exit_code=$exitCode), 详见 stderr 日志"
        }
    }

    if ($inspectResult) {
        $r = $inspectResult.result
        if ($r) {
            $frameResult.inspect = @{
                file_size = $r.file_size
                magic = $r.magic
                nside = $r.nside
                nested = $r.nested
                n_pix = $r.n_pix
                has_snr = $r.has_snr
                snr_format = $r.snr_format
                filter = $r.meta_json.filter
                exposure_s = $r.meta_json.exposure_s
                pixfrac = $r.meta_json.pixfrac
                wcs = $r.meta_json.wcs
                sip_order = $r.meta_json.wcs.sip_order
                crval = $r.meta_json.wcs.crval
                crpix = $r.meta_json.wcs.crpix
                cd = $r.meta_json.wcs.cd
                n_healpix_pixels = $r.meta_json.drizzle.n_healpix_pixels
            }
        }
    }

    Write-Host "[$frameId] 完成: success=$($frameResult.success), exit=$exitCode, elapsed=$($frameResult.elapsed_sec)s"
    if ($hissExists) {
        Write-Host "[$frameId] HISS: $($frame.out) ($($frameResult.hiss_size_kb) KB, SHA256=$($hissHash.Substring(0,16))...)"
    } else {
        Write-Warning "[$frameId] HISS 文件未生成"
    }
    if ($metrics.platesolve_rms) {
        Write-Host "[$frameId] PlateSolve: RMS=$($metrics.platesolve_rms)\" n_pairs=$($metrics.platesolve_n_pairs)"
    }
    if ($metrics.psf_n_success) {
        Write-Host "[$frameId] PSF: $($metrics.psf_n_success)/$($metrics.psf_n_total) stars"
    }
    if ($metrics.photometric_n_matched) {
        Write-Host "[$frameId] Photometric: n_matched=$($metrics.photometric_n_matched) scale=$($metrics.photometric_scale_factor)"
    }
    if ($inspectResult -and $inspectResult.result.has_snr) {
        Write-Host "[$frameId] HISS has_snr=true (P03-004 修复生效)"
    }
    Write-Host ""

    $results += $frameResult
    $summary.frames += $frameResult

    # 保存每帧元数据
    $frameResult | ConvertTo-Json -Depth 8 | Out-File -FilePath $metaFile -Encoding utf8
}

# 保存汇总结果
$summary.completed_at = (Get-Date).ToString("o")
$summary.success_count = ($results | Where-Object { $_.success }).Count
$summary.failed_count = ($results | Where-Object { -not $_.success }).Count
$summary.results = $results

$summaryFile = Join-Path $evidenceDir "stage1_e2e_raw.json"
$summary | ConvertTo-Json -Depth 10 | Out-File -FilePath $summaryFile -Encoding utf8

Write-Host "============================================================"
Write-Host "P05-002 Stage1 端到端运行完成"
Write-Host "============================================================"
Write-Host "总帧数: $($summary.total_frames)"
Write-Host "成功帧数: $($summary.success_count)"
Write-Host "失败帧数: $($summary.failed_count)"
Write-Host "汇总结果: $summaryFile"
Write-Host ""
Write-Host "帧结果详情:"
foreach ($r in $results) {
    $status = if ($r.success) { "PASS" } else { "FAIL" }
    $hissInfo = if ($r.hiss_exists) { "HISS=$($r.hiss_size_kb)KB" } else { "无HISS" }
    $rms = if ($r.metrics.platesolve_rms) { "RMS=$($r.metrics.platesolve_rms)\"" } else { "" }
    $npairs = if ($r.metrics.platesolve_n_pairs) { "n_pairs=$($r.metrics.platesolve_n_pairs)" } else { "" }
    $psf = if ($r.metrics.psf_n_success) { "PSF=$($r.metrics.psf_n_success)/$($r.metrics.psf_n_total)" } else { "" }
    $matched = if ($r.metrics.photometric_n_matched) { "n_matched=$($r.metrics.photometric_n_matched)" } else { "" }
    Write-Host ("  {0}: {1} exit={2} elapsed={3}s {4} {5} {6} {7} {8}" -f $r.dataset_id, $status, $r.exit_code, $r.elapsed_sec, $hissInfo, $rms, $npairs, $psf, $matched)
}
