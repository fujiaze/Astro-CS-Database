#requires -Version 7.0
<#
.SYNOPSIS
  P05-002 结果汇总与 HISS 验证脚本
  - 对每个 HISS 文件运行 inspect --hiss 并保存输出
  - 从 orchestrator_2026-07-25.log 提取每帧完整指标
  - 生成 stage1_e2e_results.json 结构化结果
#>

$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$projectRoot = "f:\Astro dev\Astro CS Normalization Database"
$orchExe = Join-Path $projectRoot "lib\orchestrator\cpp\orchestrator.exe"
$evidenceDir = Join-Path $projectRoot "engineering\evidence\P05-002"
$framesDir = Join-Path $evidenceDir "frames"
$hissDir = Join-Path $evidenceDir "hiss"
$logFile = Join-Path $projectRoot "lib\orchestrator\logs\orchestrator_2026-07-25.log"

# DLL 依赖路径
$env:Path = "$($orchExe | Split-Path -Parent);C:\msys64\mingw64\bin;$env:Path"

# 帧定义 (与 run_stage1_e2e.ps1 一致)
$frames = @(
    @{ id = "P05-001-C001"; tel = "T4"; cfg = "stage1_config_T4.json"; filter = "Red"; exposure = 180.0;
       path = "testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts";
       out = "P05-001-C001_Galaxy_Center_T4_Red_180s.hiss";
       startTime = "2026-07-25 21:14:49"; endTime = "2026-07-25 21:15:08" },
    @{ id = "P05-001-C002"; tel = "T2"; cfg = "stage1_config_T2.json"; filter = "Lum"; exposure = 600.0;
       path = "testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts";
       out = "P05-001-C002_LDN43_T2_Lum_600s.hiss";
       startTime = "2026-07-25 21:15:08"; endTime = "2026-07-25 21:15:12" },
    @{ id = "P05-001-C003"; tel = "T2"; cfg = "stage1_config_T2.json"; filter = "Red"; exposure = 600.0;
       path = "testdata\NGC1727_T2_flying_dutchman\lights\NGC1727_RGBHO_T2_flying_dutchman-20251031@064517-600S-Red.fts";
       out = "P05-001-C003_NGC1727_T2_Red_600s.hiss";
       startTime = "2026-07-25 21:15:12"; endTime = "2026-07-25 21:15:54" },
    @{ id = "P05-001-C004"; tel = "T2"; cfg = "stage1_config_T2.json"; filter = "Lum"; exposure = 600.0;
       path = "testdata\NGC247_T2_flying_dutchman\lights\NGC247_T2_flying_dutchman-20250816@033428-600S-Lum.fts";
       out = "P05-001-C004_NGC247_T2_Lum_600s.hiss";
       startTime = "2026-07-25 21:16:41"; endTime = "2026-07-25 21:17:05" },
    @{ id = "P05-001-C005"; tel = "T3"; cfg = "stage1_config_T3.json"; filter = "Red"; exposure = 600.0;
       path = "testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts";
       out = "P05-001-C005_NGC55_T3_Red_600s.hiss";
       startTime = "2026-07-25 21:17:06"; endTime = "2026-07-25 21:17:24" },
    @{ id = "P05-001-C006"; tel = "T3"; cfg = "stage1_config_T3.json"; filter = "Red"; exposure = 600.0;
       path = "testdata\NGC83_cluster_T3_Flying_Dutchman\lights\NGC90_2025wwk_T3_flying_dutchman-20251011@020846-600S-Red.fts";
       out = "P05-001-C006_NGC83_cluster_T3_Red_600s.hiss";
       startTime = "2026-07-25 21:17:24"; endTime = "2026-07-25 21:17:40" },
    @{ id = "P05-001-C007"; tel = "T4"; cfg = "stage1_config_T4.json"; filter = "Lum"; exposure = 180.0;
       path = "testdata\Victory_Nebula_T4_Flying_Dutchman\lights\Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts";
       out = "P05-001-C007_Victory_Nebula_T4_Lum_180s.hiss";
       startTime = "2026-07-25 21:17:40"; endTime = "2026-07-25 21:18:12" }
)

# 读取 orchestrator 日志
Write-Host "读取 orchestrator 日志: $logFile"
$logLines = Get-Content -Path $logFile -Encoding UTF8
Write-Host "日志总行数: $($logLines.Count)"

# 辅助: 在日志中按时间范围查找匹配行
function Find-LogLines {
    param([string]$startPattern, [string]$endPattern, [string[]]$lines)
    $startIdx = -1
    $endIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match $startPattern -and $startIdx -eq -1) {
            $startIdx = $i
        }
        if ($startIdx -gt -1 -and $lines[$i] -match $endPattern -and $i -gt $startIdx) {
            $endIdx = $i
            break
        }
    }
    if ($startIdx -eq -1) { return @() }
    if ($endIdx -eq -1) { $endIdx = $lines.Count - 1 }
    return $lines[$startIdx..$endIdx]
}

# 辅助: 从一组日志行中提取指标
function Extract-MetricsFromLogLines {
    param([string[]]$logSlice)

    $m = @{
        read_fits_size = $null
        read_fits_keywords = $null
        read_fits_elapsed = $null
        calibrate_status = $null
        calibrate_light_mean = $null
        calibrate_out_mean = $null
        calibrate_bias_mean = $null
        calibrate_dark_mean = $null
        calibrate_flat_mean = $null
        calibrate_actual_k = $null
        calibrate_elapsed = $null
        platesolve_success = $false
        platesolve_rms = $null
        platesolve_n_pairs = 0
        platesolve_trans_order = $null
        platesolve_crval = $null
        platesolve_crpix = $null
        platesolve_sip_order = $null
        platesolve_star_det_written = $false
        platesolve_n_detected = 0
        platesolve_callback_copied = $false
        platesolve_elapsed = $null
        psf_n_total = 0
        psf_n_success = 0
        psf_success_rate = $null
        psf_elapsed = $null
        photometric_n_matched = 0
        photometric_scale = $null
        photometric_sigma_residual = $null
        photometric_filter = $null
        photometric_elapsed = $null
        snr_n_stars = 0
        snr_sigma_residual = $null
        snr_skipped = $false
        snr_elapsed = $null
        drizzle_nside = $null
        drizzle_n_healpix = 0
        drizzle_n_source = 0
        drizzle_elapsed = $null
        stage1_completed = $false
    }

    foreach ($line in $logSlice) {
        if ($line -match 'READ_FITS\] 图像尺寸: (\d+)x(\d+)') {
            $m.read_fits_size = "$($matches[1])x$($matches[2])"
        }
        if ($line -match 'READ_FITS\] FITS 关键字数: (\d+)') {
            $m.read_fits_keywords = [int]$matches[1]
        }
        if ($line -match '\[([0-9.]+)s\] READ_FITS 完成') {
            $m.read_fits_elapsed = [double]$matches[1]
        }
        if ($line -match 'CALIBRATION_STATUS=(\w+)') {
            $m.calibrate_status = $matches[1]
        }
        if ($line -match 'light_mean=([0-9.eE+-]+) -> out_mean=([0-9.eE+-]+), bias_mean=([0-9.eE+-]+), dark_mean=([0-9.eE+-]+), flat_mean=([0-9.eE+-]+), actual_k=([0-9.eE+-]+)') {
            $m.calibrate_light_mean = [double]$matches[1]
            $m.calibrate_out_mean = [double]$matches[2]
            $m.calibrate_bias_mean = [double]$matches[3]
            $m.calibrate_dark_mean = [double]$matches[4]
            $m.calibrate_flat_mean = [double]$matches[5]
            $m.calibrate_actual_k = [double]$matches[6]
        }
        if ($line -match '\[([0-9.]+)s\] CALIBRATE 完成') {
            $m.calibrate_elapsed = [double]$matches[1]
        }
        if ($line -match 'PLATESOLVE\] 求解成功: rms=([0-9.eE+-]+)arcsec, n_pairs=(\d+), n_detected=(\d+), n_catalog=(\d+), trans_order=(\d+)') {
            $m.platesolve_success = $true
            $m.platesolve_rms = [double]$matches[1]
            $m.platesolve_n_pairs = [int]$matches[2]
            $m.platesolve_n_detected = [int]$matches[3]
            $m.platesolve_trans_order = [int]$matches[5]
        }
        if ($line -match 'PLATESOLVE\] WCS 已写入: .*CRVAL=\(([0-9.eE+-]+),\s*([0-9.eE+-]+)\),\s*CRPIX=\(([0-9.eE+-]+),\s*([0-9.eE+-]+)\)') {
            $m.platesolve_crval = @([double]$matches[1], [double]$matches[2])
            $m.platesolve_crpix = @([double]$matches[3], [double]$matches[4])
        }
        if ($line -match 'PLATESOLVE\] SIP 已写入: order=(\d+), ap_order=(\d+)') {
            $m.platesolve_sip_order = [int]$matches[1]
        }
        if ($line -match 'PLATESOLVE\] star_det 块已写入') {
            $m.platesolve_star_det_written = $true
        }
        if ($line -match 'PLATESOLVE\] callback 导出: n_detected=(\d+), copied=(\w+)') {
            $m.platesolve_n_detected = [int]$matches[1]
            $m.platesolve_callback_copied = ($matches[2] -eq 'true')
        }
        if ($line -match '\[([0-9.]+)s\] PLATESOLVE 完成') {
            $m.platesolve_elapsed = [double]$matches[1]
        }
        if ($line -match 'PSF\] star_det: (\d+) 颗星') {
            $m.psf_n_total = [int]$matches[1]
        }
        if ($line -match 'PSF\] 拟合完成: (\d+)/(\d+) 成功 \((\d+)%\)') {
            $m.psf_n_success = [int]$matches[1]
            $m.psf_n_total = [int]$matches[2]
            $m.psf_success_rate = [int]$matches[3]
        }
        if ($line -match 'PSF\] 完成: (\d+) 颗星, (\d+) 成功') {
            $m.psf_n_total = [int]$matches[1]
            $m.psf_n_success = [int]$matches[2]
            if ($m.psf_n_total -gt 0) {
                $m.psf_success_rate = [int]([math]::Round(100.0 * $m.psf_n_success / $m.psf_n_total))
            }
        }
        if ($line -match '\[([0-9.]+)s\] PSF 完成') {
            $m.psf_elapsed = [double]$matches[1]
        }
        if ($line -match 'PHOTOMETRIC\] FILTER=''([^'']+)'' -> ''([^'']+)''') {
            $m.photometric_filter = $matches[2]
        }
        if ($line -match 'PHOTOMETRIC\] 完成: n_matched=(\d+), scale=([0-9.eE+-]+), sigma_residual=([0-9.eE+-]+)') {
            $m.photometric_n_matched = [int]$matches[1]
            $m.photometric_scale = [double]$matches[2]
            $m.photometric_sigma_residual = [double]$matches[3]
        }
        if ($line -match '\[([0-9.]+)s\] PHOTOMETRIC 完成') {
            $m.photometric_elapsed = [double]$matches[1]
        }
        if ($line -match 'SNR\] n_stars=(\d+) sigma_residual=([0-9.eE+-]+)') {
            $m.snr_n_stars = [int]$matches[1]
            $m.snr_sigma_residual = [double]$matches[2]
        }
        if ($line -match 'SNR\] sigma_residual<=0, 降级跳过 snr_model 块') {
            $m.snr_skipped = $true
        }
        if ($line -match '\[([0-9.]+)s\] SNR 完成') {
            $m.snr_elapsed = [double]$matches[1]
        }
        if ($line -match 'DRIZZLE\] nside=(\d+)') {
            $m.drizzle_nside = [int]$matches[1]
        }
        if ($line -match 'DRIZZLE\] 完成: n_healpix=(\d+) n_source=(\d+) 耗时=([0-9.eE+-]+)s') {
            $m.drizzle_n_healpix = [int64]$matches[1]
            $m.drizzle_n_source = [int64]$matches[2]
            $m.drizzle_elapsed = [double]$matches[3]
        }
        if ($line -match 'stage1 完成 \(成功\)') {
            $m.stage1_completed = $true
        }
    }
    return $m
}

# 辅助: 运行 inspect --hiss 并返回结果
function Invoke-InspectHiss {
    param([string]$hissPath)
    if (-not (Test-Path $hissPath)) { return $null }

    $tmpOut = [System.IO.Path]::GetTempFileName()
    $tmpErr = [System.IO.Path]::GetTempFileName()
    try {
        # 使用单字符串 ArgumentList 并对含空格的路径加引号, 避免 Start-Process 拆分
        $argString = "inspect --hiss `"$hissPath`""
        $proc = Start-Process -FilePath $orchExe `
            -ArgumentList $argString `
            -WorkingDirectory $projectRoot `
            -RedirectStandardOutput $tmpOut `
            -RedirectStandardError $tmpErr `
            -NoNewWindow -Wait -PassThru
        $stdout = Get-Content $tmpOut -Raw -ErrorAction SilentlyContinue
        $stderr = Get-Content $tmpErr -Raw -ErrorAction SilentlyContinue

        # 解析 stdout 中的 JSONL 事件, 提取 result 事件
        # 注意: orchestrator inspect 输出 "snr_format":unknown (未加引号), 需要先修复 JSON
        $inspectResult = $null
        if ($stdout) {
            # 修复无效 JSON: 将 :unknown 替换为 :"unknown"
            $fixedStdout = $stdout -replace ':(\s*)unknown(\s*[,}])', ':"unknown"$2'
            $lines = $fixedStdout -split "`n" | Where-Object { $_.Trim() -ne '' }
            foreach ($line in $lines) {
                try {
                    $j = $line | ConvertFrom-Json
                    if ($j.type -eq 'result' -and $j.result) {
                        $inspectResult = $j.result
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

# 处理每帧
$results = @()
$summary = @{
    task_id = "P05-002"
    task_name = "Stage1 真实数据端到端验证 (v1.1 开发包)"
    started_at = "2026-07-25T21:14:49+08:00"
    completed_at = "2026-07-25T21:18:12+08:00"
    orchestrator_exe = $orchExe
    orchestrator_log = $logFile
    total_frames = $frames.Count
    frames = @()
}

foreach ($frame in $frames) {
    $frameId = $frame.id
    $frameLogDir = Join-Path $framesDir $frameId
    $hissPath = Join-Path $hissDir $frame.out
    $inspectOutFile = Join-Path $frameLogDir "inspect_hiss.jsonl"
    $inspectErrFile = Join-Path $frameLogDir "inspect_hiss_stderr.log"
    $fullLogFile = Join-Path $frameLogDir "stage1_full_log.txt"

    Write-Host "============================================================"
    Write-Host "[$frameId] 处理中 ..."
    Write-Host "============================================================"

    # 从主日志提取该帧的日志切片
    # 起始: 当前帧的 stage1: 单帧预处理
    # 结束: 下一帧的 "初始化编排器" 或 stage1 完成 (最后一帧)
    $startPattern = "$($frame.startTime).*stage1: 单帧预处理"
    $endPattern = "初始化编排器"
    if ($frameId -eq "P05-001-C007") {
        # 最后一帧, end pattern is stage1 完成
        $endPattern = "stage1 完成 \(成功\)"
    }
    $logSlice = Find-LogLines -startPattern $startPattern -endPattern $endPattern -lines $logLines
    # 对最后一帧, Find-LogLines 会找到第一个匹配, 但我们要找的是最后一个匹配
    if ($frameId -eq "P05-001-C007" -and $logSlice.Count -eq 0) {
        # fallback: 从 start 到文件末尾
        $startIdx = -1
        for ($i = 0; $i -lt $logLines.Count; $i++) {
            if ($logLines[$i] -match $startPattern) { $startIdx = $i; break }
        }
        if ($startIdx -gt -1) {
            $logSlice = $logLines[$startIdx..($logLines.Count - 1)]
        }
    }
    # 如果还是空, 用 startTime 到 endTime 范围
    if ($logSlice.Count -eq 0) {
        $startIdx = -1
        $endIdx = -1
        for ($i = 0; $i -lt $logLines.Count; $i++) {
            if ($logLines[$i] -match $startPattern -and $startIdx -eq -1) { $startIdx = $i }
            elseif ($startIdx -gt -1 -and $logLines[$i] -match $endPattern -and $i -gt $startIdx) {
                $endIdx = $i; break
            }
        }
        if ($startIdx -eq -1) { $logSlice = @() }
        elseif ($endIdx -eq -1) { $logSlice = $logLines[$startIdx..($logLines.Count - 1)] }
        else { $logSlice = $logLines[$startIdx..($endIdx - 1)] }
    }
    Write-Host "[$frameId] 日志切片行数: $($logSlice.Count)"

    # 保存切片到帧目录
    if ($logSlice.Count -gt 0) {
        $logSlice | Out-File -FilePath $fullLogFile -Encoding utf8
    }

    # 提取指标
    $metrics = Extract-MetricsFromLogLines -logSlice $logSlice

    # HISS 文件信息
    $hissExists = Test-Path $hissPath
    $hissSize = 0
    $hissHash = ""
    if ($hissExists) {
        $hissFile = Get-Item $hissPath
        $hissSize = $hissFile.Length
        $hissHash = (Get-FileHash $hissPath -Algorithm SHA256).Hash
    }

    # 运行 inspect --hiss
    $inspect = $null
    if ($hissExists) {
        Write-Host "[$frameId] 运行 inspect --hiss ..."
        $inspectOutput = Invoke-InspectHiss -hissPath $hissPath
        if ($inspectOutput) {
            $inspectOutput.stdout | Out-File -FilePath $inspectOutFile -Encoding utf8 -NoNewline
            $inspectOutput.stderr | Out-File -FilePath $inspectErrFile -Encoding utf8
            $inspect = $inspectOutput.result
        }
    }

    # 构建结构化结果
    $frameResult = [ordered]@{
        dataset_id = $frameId
        telescope = $frame.tel
        filter = $frame.filter
        exposure_s = $frame.exposure
        config_file = $frame.cfg
        frame_path = $frame.path
        frame_file = (Split-Path $frame.path -Leaf)
        output_hiss_relative = "engineering/evidence/P05-002/hiss/$($frame.out)"
        output_hiss_path = $hissPath
        success = $metrics.stage1_completed
        stage1_completed = $metrics.stage1_completed
        hiss_exists = $hissExists
        hiss_size_bytes = $hissSize
        hiss_size_kb = [math]::Round($hissSize / 1KB, 2)
        hiss_sha256 = $hissHash
        metrics = $metrics
        inspect = $null
    }

    if ($inspect) {
        $frameResult.inspect = [ordered]@{
            format = $inspect.format
            magic = $inspect.magic
            file_size = $inspect.file_size
            nside = $inspect.nside
            nested = $inspect.nested
            n_pix = $inspect.n_pix
            has_snr = $inspect.has_snr
            snr_format = $inspect.snr_format
            filter = $inspect.meta_json.filter
            exposure_s = $inspect.meta_json.exposure_s
            obs_time = $inspect.meta_json.obs_time
            pixfrac = $inspect.meta_json.pixfrac
            sip_order = $inspect.meta_json.wcs.sip_order
            crval = $inspect.meta_json.wcs.crval
            crpix = $inspect.meta_json.wcs.crpix
            cd = $inspect.meta_json.wcs.cd
            n_healpix_pixels = $inspect.meta_json.drizzle.n_healpix_pixels
            drizzle_elapsed_sec = $inspect.meta_json.drizzle.elapsed_sec
            fits_object = $inspect.meta_json.fits_meta.OBJECT
            fits_instrume = $inspect.meta_json.fits_meta.INSTRUME
            fits_xpixsz = $inspect.meta_json.fits_meta.XPIXSZ
        }
    }

    # 数值范围验证
    $validation = [ordered]@{
        platesolve_rms_lt_1arcsec = if ($metrics.platesolve_rms) { $metrics.platesolve_rms -lt 1.0 } else { $false }
        platesolve_n_pairs_gt_10 = $metrics.platesolve_n_pairs -gt 10
        psf_effective = $metrics.psf_n_success -gt 0
        hiss_size_gt_10kb = $hissSize -gt 10KB
        has_snr_true = if ($inspect) { $inspect.has_snr } else { $false }
        wcs_complete = if ($inspect) {
            ($null -ne $inspect.meta_json.wcs.crval) -and
            ($null -ne $inspect.meta_json.wcs.crpix) -and
            ($null -ne $inspect.meta_json.wcs.cd)
        } else { $false }
        star_det_written = $metrics.platesolve_star_det_written
    }
    $frameResult.validation = $validation

    # 失败原因 (针对 C002)
    if (-not $frameResult.success) {
        if ($frameId -eq "P05-001-C002") {
            $frameResult.error_msg = "stage1 exit_code=3. T2 校准目录缺少 masterFlat_FILTER-Lum_mono.xisf (Lum 滤镜 flat 文件缺失), orchestrator 在 CALIBRATE 阶段前预检查失败直接退出."
            $frameResult.failure_root_cause = "missing_master_flat_Lum"
        } else {
            $frameResult.error_msg = "stage1 未完成, 详见日志"
        }
    }

    Write-Host "[$frameId] success=$($frameResult.success), HISS=$($frameResult.hiss_size_kb)KB"
    if ($metrics.platesolve_rms) {
        Write-Host "[$frameId] PlateSolve: RMS=$($metrics.platesolve_rms) arcsec n_pairs=$($metrics.platesolve_n_pairs)"
    }
    if ($metrics.psf_n_success) {
        $rateStr = "$($metrics.psf_success_rate) percent"
        Write-Host "[$frameId] PSF: $($metrics.psf_n_success)/$($metrics.psf_n_total) ($rateStr)"
    }
    Write-Host "[$frameId] Photometric: n_matched=$($metrics.photometric_n_matched) scale=$($metrics.photometric_scale)"
    Write-Host "[$frameId] SNR: skipped=$($metrics.snr_skipped)"
    if ($inspect) {
        Write-Host "[$frameId] HISS: nside=$($inspect.nside) n_pix=$($inspect.n_pix) has_snr=$($inspect.has_snr)"
    }

    $results += $frameResult
    $summary.frames += $frameResult

    # 更新帧 meta 文件
    $frameResult | ConvertTo-Json -Depth 10 | Out-File -FilePath (Join-Path $frameLogDir "stage1_meta.json") -Encoding utf8
}

# 汇总 (手动计数, 避免 Where-Object 的 Count 在单元素时的怪异行为)
$successCount = 0
$failedCount = 0
$hissCount = 0
foreach ($r in $results) {
    if ($r.success) { $successCount++ } else { $failedCount++ }
    if ($r.hiss_exists) { $hissCount++ }
}
$summary.success_count = $successCount
$summary.failed_count = $failedCount
$summary.hiss_count = $hissCount

# 验证汇总 (手动计数)
$psPass = 0; $psfPass = 0; $hissSizePass = 0; $hasSnrPass = 0; $wcsComplete = 0; $starDetWritten = 0
foreach ($r in $results) {
    if ($r.validation.platesolve_rms_lt_1arcsec -and $r.validation.platesolve_n_pairs_gt_10) { $psPass++ }
    if ($r.validation.psf_effective) { $psfPass++ }
    if ($r.validation.hiss_size_gt_10kb) { $hissSizePass++ }
    if ($r.validation.has_snr_true) { $hasSnrPass++ }
    if ($r.validation.wcs_complete) { $wcsComplete++ }
    if ($r.validation.star_det_written) { $starDetWritten++ }
}
$summary.validation_summary = [ordered]@{
    platesolve_pass_count = $psPass
    psf_pass_count = $psfPass
    hiss_size_pass_count = $hissSizePass
    has_snr_true_count = $hasSnrPass
    wcs_complete_count = $wcsComplete
    star_det_written_count = $starDetWritten
}

# 数值范围总览
$summary.metrics_summary = [ordered]@{
    platesolve_rms_range = @{
        min = ($results | Where-Object { $_.metrics.platesolve_rms } | ForEach-Object { $_.metrics.platesolve_rms } | Measure-Object -Minimum).Minimum
        max = ($results | Where-Object { $_.metrics.platesolve_rms } | ForEach-Object { $_.metrics.platesolve_rms } | Measure-Object -Maximum).Maximum
    }
    platesolve_n_pairs_range = @{
        min = ($results | Where-Object { $_.metrics.platesolve_n_pairs -gt 0 } | ForEach-Object { $_.metrics.platesolve_n_pairs } | Measure-Object -Minimum).Minimum
        max = ($results | Where-Object { $_.metrics.platesolve_n_pairs -gt 0 } | ForEach-Object { $_.metrics.platesolve_n_pairs } | Measure-Object -Maximum).Maximum
    }
    psf_success_rate_range = @{
        min = ($results | Where-Object { $_.metrics.psf_success_rate } | ForEach-Object { $_.metrics.psf_success_rate } | Measure-Object -Minimum).Minimum
        max = ($results | Where-Object { $_.metrics.psf_success_rate } | ForEach-Object { $_.metrics.psf_success_rate } | Measure-Object -Maximum).Maximum
    }
    photometric_n_matched_values = ($results | Where-Object { $_.metrics.photometric_n_matched -ne $null } | ForEach-Object { $_.metrics.photometric_n_matched })
    snr_skipped_count = ($results | Where-Object { $_.metrics.snr_skipped }).Count
    has_snr_true_count = ($results | Where-Object { $_.inspect -and $_.inspect.has_snr }).Count
}

# 保存汇总结果
$resultsFile = Join-Path $evidenceDir "stage1_e2e_results.json"
$summary | ConvertTo-Json -Depth 12 | Out-File -FilePath $resultsFile -Encoding utf8

Write-Host ""
Write-Host "============================================================"
Write-Host "P05-002 结果汇总完成"
Write-Host "============================================================"
Write-Host "总帧数: $($summary.total_frames)"
Write-Host "成功帧数: $($summary.success_count)"
Write-Host "失败帧数: $($summary.failed_count)"
Write-Host "HISS 文件数: $($summary.hiss_count)"
$psPass = $summary.validation_summary.platesolve_pass_count
Write-Host "PlateSolve 通过: $psPass/7 (RMS<1.0 arcsec and n_pairs>10)"
Write-Host "PSF 有效: $($summary.validation_summary.psf_pass_count)/7"
Write-Host "HISS >10KB: $($summary.validation_summary.hiss_size_pass_count)/7"
Write-Host "WCS 完整: $($summary.validation_summary.wcs_complete_count)/7"
Write-Host "star_det 已写入: $($summary.validation_summary.star_det_written_count)/7"
Write-Host "has_snr=true: $($summary.validation_summary.has_snr_true_count)/7 (P03-004 SNR 降级)"
Write-Host ""
Write-Host "结果文件: $resultsFile"
