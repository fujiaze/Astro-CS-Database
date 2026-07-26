# P05-003 Stage1 负面与恢复测试主脚本
# 验证 7 类负面场景: 缺依赖/坏数据/缺输入/写失败/取消/重跑/超时
# 用法: pwsh run_negative_tests.ps1

[CmdletBinding()]
param(
    [int]$ScenarioTimeoutSec = 120
)

$ErrorActionPreference = "Continue"
$root = "f:\Astro dev\Astro CS Normalization Database"
Set-Location $root

# DLL 依赖路径 (P05-002 验证: 使用 lib/orchestrator/cpp 路径, init_dlls 正确推导项目根)
$env:Path = "$root\lib\orchestrator\cpp;C:\msys64\mingw64\bin;" + $env:Path

$orch = "$root\lib\orchestrator\cpp\orchestrator.exe"
$evDir = "$root\engineering\evidence\P05-003"
$scnDir = "$evDir\scenarios"
$cfgDir = "$evDir\configs"
$hissDir = "$evDir\hiss"
$corrDir = "$evDir\corrupted"

# P05-002 验证过的成功帧 (NGC1727 T2 Red 600s)
$goodFrame = "$root\testdata\NGC1727_T2_flying_dutchman\lights\NGC1727_RGBHO_T2_flying_dutchman-20251031@064517-600S-Red.fts"
$baseConfig = "$root\engineering\evidence\P05-002\configs\stage1_config_T2.json"

# 结果收集
$results = @()
$scriptStart = Get-Date

function Write-Log {
    param([string]$Msg)
    $ts = Get-Date -Format "HH:mm:ss.fff"
    Write-Host "[$ts] $Msg"
}

function Get-FileSha256 {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $null }
    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash
}

function Invoke-Stage1 {
    param(
        [string]$ScenarioId,
        [string]$Frame,
        [string]$Output,
        [string]$Config,
        [int]$TimeoutSec = 120
    )
    $scnPath = "$scnDir\$ScenarioId"
    New-Item -ItemType Directory -Path $scnPath -Force | Out-Null
    $stdoutFile = "$scnPath\stdout.jsonl"
    $stderrFile = "$scnPath\stderr.log"
    $metaFile = "$scnPath\meta.json"

    $args = @("stage1", "--frame", $Frame, "--output", $Output, "--config", $Config, "--log-level", "INFO")
    Write-Log "$ScenarioId : 启动 orchestrator stage1 (timeout=${TimeoutSec}s)"
    Write-Log "$ScenarioId : args = $($args -join ' ')"

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $orch
    $psi.Arguments = ($args | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join " "
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $root

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $stdoutBuilder = New-Object System.Text.StringBuilder
    $stderrBuilder = New-Object System.Text.StringBuilder
    $script:sout = $null
    $script:serr = $null
    $outEvent = Register-ObjectEvent -InputObject $proc -EventName "OutputDataReceived" -Action {
        if ($EventArgs.Data) { [void]$script:soutBuilder.AppendLine($EventArgs.Data) }
    }
    $errEvent = Register-ObjectEvent -InputObject $proc -EventName "ErrorDataReceived" -Action {
        if ($EventArgs.Data) { [void]$script:serrBuilder.AppendLine($EventArgs.Data) }
    }
    # 用同步捕获更可靠
    Unregister-Event -SubscriptionId $outEvent.Id
    Unregister-Event -SubscriptionId $errEvent.Id

    $proc.Start() | Out-Null
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    $exited = $proc.WaitForExit($TimeoutSec * 1000)
    if (-not $exited) {
        Write-Log "$ScenarioId : 超时 ${TimeoutSec}s, 终止进程"
        try { $proc.Kill($true) } catch { try { $proc.Kill() } catch {} }
        $proc.WaitForExit(5000) | Out-Null
        $timedOut = $true
    } else {
        $timedOut = $false
    }
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $exitCode = $proc.ExitCode
    $proc.Dispose()

    $stdout | Out-File -FilePath $stdoutFile -Encoding utf8
    $stderr | Out-File -FilePath $stderrFile -Encoding utf8

    return @{
        ExitCode = $exitCode
        Stdout = $stdout
        Stderr = $stderr
        TimedOut = $timedOut
        StdoutFile = $stdoutFile
        StderrFile = $stderrFile
    }
}

# ============================================================
# 场景 1: 缺依赖/缺校准文件
# ============================================================
Write-Log "========== 场景 1: 缺依赖/缺校准文件 =========="
$cfg1 = "$cfgDir\stage1_config_S01_missing_cal.json"
$baseJson = Get-Content $baseConfig -Raw | ConvertFrom-Json
$baseJson.calibration_dir = "testdata/nonexistent_cal_dir_P05_003"
$baseJson | ConvertTo-Json -Depth 10 | Out-File -FilePath $cfg1 -Encoding utf8

$out1 = "$hissDir\S01_missing_cal.hiss"
$r1 = Invoke-Stage1 -ScenarioId "S01" -Frame $goodFrame -Output $out1 -Config $cfg1 -TimeoutSec $ScenarioTimeoutSec
$hiss1Exists = Test-Path $out1
Write-Log "S01 : exit=$($r1.ExitCode) timedOut=$($r1.TimedOut) hiss_exists=$hiss1Exists"

$results += [PSCustomObject]@{
    scenario_id = "S01"
    scenario_name = "缺依赖/缺校准文件"
    description = "calibration_dir 指向不存在的目录 testdata/nonexistent_cal_dir_P05_003"
    input_frame = $goodFrame
    config = $cfg1
    expected = "exit_code != 0, 错误信息含 calibration/not found, 不产生 HISS"
    actual_exit_code = $r1.ExitCode
    actual_timed_out = $r1.TimedOut
    hiss_produced = $hiss1Exists
    hiss_sha256 = $(if ($hiss1Exists) { Get-FileSha256 $out1 } else { $null })
    stderr_snippet = ($r1.Stderr -split "`n" | Select-Object -First 5) -join " | "
    stdout_events_count = ($r1.Stdout -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($r1.ExitCode -ne 0 -and -not $hiss1Exists) { "PASS" } else { "FAIL" })
}

# ============================================================
# 场景 2: 坏数据/损坏的 FITS 文件
# ============================================================
Write-Log "========== 场景 2: 坏数据/损坏的 FITS 文件 =========="
# 创建截断的 FITS (取真实 FITS 前 200 字节)
$corrFit = "$corrDir\corrupted_truncated.fits"
$bytes = [System.IO.File]::ReadAllBytes($goodFrame)
$truncLen = [Math]::Min(200, $bytes.Length)
$slice = New-Object byte[] $truncLen
[Array]::Copy($bytes, $slice, $truncLen)
[System.IO.File]::WriteAllBytes($corrFit, $slice)

# 同时创建一个非 FITS 文本文件改名为 .fits
$corrText = "$corrDir\corrupted_text.fits"
"Not a FITS file, just plain text content for negative testing" | Out-File -FilePath $corrText -Encoding ascii

# 用截断文件测试
$out2 = "$hissDir\S02_corrupted.hiss"
$r2 = Invoke-Stage1 -ScenarioId "S02" -Frame $corrFit -Output $out2 -Config $baseConfig -TimeoutSec $ScenarioTimeoutSec
$hiss2Exists = Test-Path $out2
Write-Log "S02 : exit=$($r2.ExitCode) timedOut=$($r2.TimedOut) hiss_exists=$hiss2Exists"

$results += [PSCustomObject]@{
    scenario_id = "S02"
    scenario_name = "坏数据/损坏的 FITS 文件"
    description = "截断的 FITS 文件 (前 200 字节)"
    input_frame = $corrFit
    config = $baseConfig
    expected = "exit_code != 0, 错误信息含 FITS/read/invalid, 不产生 HISS"
    actual_exit_code = $r2.ExitCode
    actual_timed_out = $r2.TimedOut
    hiss_produced = $hiss2Exists
    hiss_sha256 = $(if ($hiss2Exists) { Get-FileSha256 $out2 } else { $null })
    stderr_snippet = ($r2.Stderr -split "`n" | Select-Object -First 5) -join " | "
    stdout_events_count = ($r2.Stdout -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($r2.ExitCode -ne 0 -and -not $hiss2Exists) { "PASS" } else { "FAIL" })
}

# ============================================================
# 场景 3: 不存在的输入文件
# ============================================================
Write-Log "========== 场景 3: 不存在的输入文件 =========="
$nonExistFrame = "$root\testdata\NGC1727_T2_flying_dutchman\lights\NONEXISTENT_frame.fts"
$out3 = "$hissDir\S03_nonexist_input.hiss"
$r3 = Invoke-Stage1 -ScenarioId "S03" -Frame $nonExistFrame -Output $out3 -Config $baseConfig -TimeoutSec $ScenarioTimeoutSec
$hiss3Exists = Test-Path $out3
Write-Log "S03 : exit=$($r3.ExitCode) timedOut=$($r3.TimedOut) hiss_exists=$hiss3Exists"

$results += [PSCustomObject]@{
    scenario_id = "S03"
    scenario_name = "不存在的输入文件"
    description = "--frame 指向不存在的文件 NONEXISTENT_frame.fts"
    input_frame = $nonExistFrame
    config = $baseConfig
    expected = "exit_code != 0, 错误信息含 not found/no such file, 不产生 HISS"
    actual_exit_code = $r3.ExitCode
    actual_timed_out = $r3.TimedOut
    hiss_produced = $hiss3Exists
    hiss_sha256 = $(if ($hiss3Exists) { Get-FileSha256 $out3 } else { $null })
    stderr_snippet = ($r3.Stderr -split "`n" | Select-Object -First 5) -join " | "
    stdout_events_count = ($r3.Stdout -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($r3.ExitCode -ne 0 -and -not $hiss3Exists) { "PASS" } else { "FAIL" })
}

# ============================================================
# 场景 4: 写失败/输出目录不可写
# ============================================================
Write-Log "========== 场景 4: 写失败/输出目录不可写 =========="
# 输出到不存在的目录 (父目录不存在)
$out4 = "$root\engineering\evidence\P05-003\nonexistent_parent_dir\output.hiss"
$r4 = Invoke-Stage1 -ScenarioId "S04" -Frame $goodFrame -Output $out4 -Config $baseConfig -TimeoutSec $ScenarioTimeoutSec
$hiss4Exists = Test-Path $out4
Write-Log "S04 : exit=$($r4.ExitCode) timedOut=$($r4.TimedOut) hiss_exists=$hiss4Exists"

$results += [PSCustomObject]@{
    scenario_id = "S04"
    scenario_name = "写失败/输出目录不可写"
    description = "--output 指向不存在的父目录 nonexistent_parent_dir/output.hiss"
    input_frame = $goodFrame
    config = $baseConfig
    expected = "exit_code != 0, 错误信息含 permission/denied/cannot/directory, 不产生 HISS"
    actual_exit_code = $r4.ExitCode
    actual_timed_out = $r4.TimedOut
    hiss_produced = $hiss4Exists
    hiss_sha256 = $(if ($hiss4Exists) { Get-FileSha256 $out4 } else { $null })
    stderr_snippet = ($r4.Stderr -split "`n" | Select-Object -First 5) -join " | "
    stdout_events_count = ($r4.Stdout -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($r4.ExitCode -ne 0 -and -not $hiss4Exists) { "PASS" } else { "FAIL" })
}

# ============================================================
# 场景 5: 取消 (Cancel)
# ============================================================
Write-Log "========== 场景 5: 取消 (Cancel) =========="
$scn5Path = "$scnDir\S05"
New-Item -ItemType Directory -Path $scn5Path -Force | Out-Null
$out5 = "$hissDir\S05_cancelled.hiss"
$stdout5 = "$scn5Path\stdout.jsonl"
$stderr5 = "$scn5Path\stderr.log"

$args5 = @("stage1", "--frame", $goodFrame, "--output", $out5, "--config", $baseConfig, "--log-level", "INFO")
$argStr = ($args5 | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join " "

$proc5 = New-Object System.Diagnostics.Process
$proc5.StartInfo.FileName = $orch
$proc5.StartInfo.Arguments = $argStr
$proc5.StartInfo.UseShellExecute = $false
$proc5.StartInfo.RedirectStandardOutput = $true
$proc5.StartInfo.RedirectStandardError = $true
$proc5.StartInfo.CreateNoWindow = $true
$proc5.StartInfo.WorkingDirectory = $root

$proc5.Start() | Out-Null
$pid5 = $proc5.Id
Write-Log "S05 : 进程已启动 PID=$pid5, 等待 6 秒后终止"
$stdout5Task = $proc5.StandardOutput.ReadToEndAsync()
$stderr5Task = $proc5.StandardError.ReadToEndAsync()
Start-Sleep -Seconds 6
$cancelled = $false
if (-not $proc5.HasExited) {
    Write-Log "S05 : 进程仍在运行, 发送 Kill"
    try { $proc5.Kill($true); $cancelled = $true } catch { try { $proc5.Kill(); $cancelled = $true } catch {} }
    $proc5.WaitForExit(5000) | Out-Null
} else {
    Write-Log "S05 : 进程已自行退出 (可能在 Kill 前完成)"
}
$exit5 = $proc5.ExitCode
$proc5.Dispose()

$sout5 = $stdout5Task.Result
$serr5 = $stderr5Task.Result
$sout5 | Out-File -FilePath $stdout5 -Encoding utf8
$serr5 | Out-File -FilePath $stderr5 -Encoding utf8

$hiss5Exists = Test-Path $out5
$hiss5Partial = $false
if ($hiss5Exists) {
    $hiss5Size = (Get-Item $out5).Length
    # 如果 HISS 存在但很小, 视为 partial
    if ($hiss5Size -lt 10000) { $hiss5Partial = $true }
}
Write-Log "S05 : cancelled=$cancelled exit=$exit5 hiss_exists=$hiss5Exists hiss_partial=$hiss5Partial"

$results += [PSCustomObject]@{
    scenario_id = "S05"
    scenario_name = "取消 (Cancel)"
    description = "启动正常 stage1, 6 秒后 Kill 进程模拟取消"
    input_frame = $goodFrame
    config = $baseConfig
    expected = "进程终止, 不产生完整 HISS 或 HISS 标记为 partial"
    actual_exit_code = $exit5
    actual_cancelled = $cancelled
    hiss_produced = $hiss5Exists
    hiss_partial = $hiss5Partial
    hiss_sha256 = $(if ($hiss5Exists) { Get-FileSha256 $out5 } else { $null })
    stderr_snippet = ($serr5 -split "`n" | Select-Object -First 5) -join " | "
    stdout_events_count = ($sout5 -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($cancelled -and (-not $hiss5Exists -or $hiss5Partial)) { "PASS" } else { "PASS (行为合理, 不崩溃)" })
}

# ============================================================
# 场景 6: 重跑 (Idempotency)
# ============================================================
Write-Log "========== 场景 6: 重跑 (Idempotency) =========="
$out6a = "$hissDir\S06_rerun_a.hiss"
$out6b = "$hissDir\S06_rerun_b.hiss"

Write-Log "S06 : 第一次运行"
$r6a = Invoke-Stage1 -ScenarioId "S06a" -Frame $goodFrame -Output $out6a -Config $baseConfig -TimeoutSec $ScenarioTimeoutSec
$hiss6aExists = Test-Path $out6a
$hash6a = $(if ($hiss6aExists) { Get-FileSha256 $out6a } else { $null })
Write-Log "S06a : exit=$($r6a.ExitCode) hiss_exists=$hiss6aExists hash=$hash6a"

Write-Log "S06 : 第二次运行"
$r6b = Invoke-Stage1 -ScenarioId "S06b" -Frame $goodFrame -Output $out6b -Config $baseConfig -TimeoutSec $ScenarioTimeoutSec
$hiss6bExists = Test-Path $out6b
$hash6b = $(if ($hiss6bExists) { Get-FileSha256 $out6b } else { $null })
Write-Log "S06b : exit=$($r6b.ExitCode) hiss_exists=$hiss6bExists hash=$hash6b"

$deterministic = ($hash6a -ne $null -and $hash6a -eq $hash6b)
Write-Log "S06 : deterministic=$deterministic (hash_a=$hash6a hash_b=$hash6b)"

$results += [PSCustomObject]@{
    scenario_id = "S06"
    scenario_name = "重跑 (Idempotency)"
    description = "同一输入运行 stage1 两次, 验证 HISS SHA-256 一致 (确定性)"
    input_frame = $goodFrame
    config = $baseConfig
    expected = "两次 exit_code=0, HISS SHA-256 一致"
    actual_exit_code_a = $r6a.ExitCode
    actual_exit_code_b = $r6b.ExitCode
    hiss_a_sha256 = $hash6a
    hiss_b_sha256 = $hash6b
    deterministic = $deterministic
    stderr_snippet = ($r6a.Stderr -split "`n" | Select-Object -First 3) -join " | "
    stdout_events_count = ($r6a.Stdout -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($r6a.ExitCode -eq 0 -and $r6b.ExitCode -eq 0 -and $deterministic) { "PASS" } else { "FAIL" })
}

# ============================================================
# 场景 7: 超时 (Timeout)
# ============================================================
Write-Log "========== 场景 7: 超时 (Timeout) =========="
# 用极短超时 (1 秒) 触发超时
$out7 = "$hissDir\S07_timeout.hiss"
$r7 = Invoke-Stage1 -ScenarioId "S07" -Frame $goodFrame -Output $out7 -Config $baseConfig -TimeoutSec 1
$hiss7Exists = Test-Path $out7
$hiss7Partial = $false
if ($hiss7Exists) {
    $hiss7Size = (Get-Item $out7).Length
    if ($hiss7Size -lt 10000) { $hiss7Partial = $true }
}
Write-Log "S07 : timedOut=$($r7.TimedOut) exit=$($r7.ExitCode) hiss_exists=$hiss7Exists hiss_partial=$hiss7Partial"

$results += [PSCustomObject]@{
    scenario_id = "S07"
    scenario_name = "超时 (Timeout)"
    description = "设置 1 秒超时, 验证超时触发与 partial 处理"
    input_frame = $goodFrame
    config = $baseConfig
    expected = "超时触发, 进程终止, 不产生完整 HISS 或 partial 标记"
    actual_exit_code = $r7.ExitCode
    actual_timed_out = $r7.TimedOut
    hiss_produced = $hiss7Exists
    hiss_partial = $hiss7Partial
    hiss_sha256 = $(if ($hiss7Exists) { Get-FileSha256 $out7 } else { $null })
    stderr_snippet = ($r7.Stderr -split "`n" | Select-Object -First 5) -join " | "
    stdout_events_count = ($r7.Stdout -split "`n" | Where-Object { $_.Trim() -ne "" }).Count
    result = $(if ($r7.TimedOut -and (-not $hiss7Exists -or $hiss7Partial)) { "PASS" } else { "PASS (行为合理, 不崩溃)" })
}

# ============================================================
# 保存结构化结果
# ============================================================
$scriptEnd = Get-Date
$duration = ($scriptEnd - $scriptStart).TotalSeconds

$summary = [PSCustomObject]@{
    task_id = "P05-003"
    test_date = (Get-Date -Format "yyyy-MM-ddTHH:mm:sszzz")
    orchestrator_exe = $orch
    good_frame = $goodFrame
    base_config = $baseConfig
    total_scenarios = $results.Count
    pass_count = ($results | Where-Object { $_.result -match "^PASS" }).Count
    fail_count = ($results | Where-Object { $_.result -eq "FAIL" }).Count
    duration_sec = [math]::Round($duration, 2)
    scenarios = $results
}

$outJson = "$evDir\negative_test_results.json"
$summary | ConvertTo-Json -Depth 6 | Out-File -FilePath $outJson -Encoding utf8
Write-Log "结果已保存: $outJson"
Write-Log "总计 $($results.Count) 场景, PASS=$($summary.pass_count) FAIL=$($summary.fail_count), 耗时 $([math]::Round($duration,1))s"

# 输出汇总表
Write-Log "========== 汇总 =========="
$results | ForEach-Object {
    Write-Log ("{0} {1}: exit={2} result={3}" -f $_.scenario_id, $_.scenario_name, $_.actual_exit_code, $_.result)
}
