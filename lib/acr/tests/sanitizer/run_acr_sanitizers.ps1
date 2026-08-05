# lib/acr/tests/sanitizer/run_acr_sanitizers.ps1 — 25 号计划 §8 Sanitizer 证据运行器
#
# 统一运行 ACR Sanitizer 证据并记录命令、timeout 与 exit code：
#   1. MSVC ASan（CPU 核心组件：SharedWorkPool / KernelRegistry / CpuExecutor
#      / CpuController + system_metrics；Dispatcher/ProfileBuilder 等因 MinGW
#      oneTBB ABI 依赖无法纳入 MSVC /fsanitize=address，如实记录限制）
#   2. CUDA compute-sanitizer --tool memcheck（覆盖缓冲独立扩缩容与多块卷积）
#   3. CUDA compute-sanitizer --tool racecheck（真实 CUDA kernel 数据竞争）
#
# 任何外部进程都带明确 timeout；工具缺失或未运行项记录为 SKIPPED，
# 绝不冒充通过。日志统一写入 run/logs/acr/sanitizer/<YYYYMMDD>/。
#
# 用法（PowerShell，禁止单引号）：
#   .\tests\sanitizer\run_acr_sanitizers.ps1

param(
    [string]$BuildDir = "F:\Astro dev\Astro CS Normalization Database\run\worktrees\acr\lib\acr\build",
    [string]$LogDir = "F:\Astro dev\Astro CS Normalization Database\run\logs\acr\sanitizer\20260805",
    [string]$MsvcAsanExe = "F:\Astro dev\Astro CS Normalization Database\run\temp\sanitizer_msvc\acr_sanitizer_msvc.exe",
    [string]$CudaBridgeDll = "F:\Astro dev\Astro CS Normalization Database\run\temp\cuda_bridge\acr_cuda_bridge.dll",
    [int]$TimeoutMsAsan = 300000,
    [int]$TimeoutMsCuda = 600000
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$results = [System.Collections.Generic.List[object]]::new()

# 被测试程序需要 MinGW 运行库，compute-sanitizer 需要 CUDA bin
$cudaBin = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin"
if (Test-Path -LiteralPath (Join-Path $cudaBin "compute-sanitizer.bat")) {
    $env:Path = "C:\msys64\mingw64\bin;" + $cudaBin + ";" + $env:Path
}

function Invoke-WithTimeout {
    param(
        [string]$Name,
        [string]$File,
        [string[]]$ArgList,
        [string]$LogPath,
        [int]$TimeoutMs,
        [string[]]$EnvPairs,
        [string]$WorkDir = ""
)
    $item = [ordered]@{
        name = $Name
        status = "SKIPPED"
        exit_code = -1
        timeout_ms = $TimeoutMs
        timed_out = $false
        log = $LogPath
    }
    if (-not (Test-Path -LiteralPath $File)) {
        $item.status = "SKIPPED"
        $results.Add([pscustomobject]$item)
        return
    }
    $workDir = if ($WorkDir) { $WorkDir } else { Split-Path -Parent $File }
    $envSnapshot = @{}
    foreach ($pair in $EnvPairs) {
        $parts = $pair -split "=", 2
        if ($parts.Count -eq 2) {
            $envSnapshot[$parts[0]] = [System.Environment]::GetEnvironmentVariable($parts[0])
            [System.Environment]::SetEnvironmentVariable($parts[0], $parts[1])
        }
    }
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $File
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $workDir
    $quotedArgs = @()
    foreach ($a in $ArgList) {
        if ($a -match "[\s]") { $quotedArgs += '"' + $a + '"' } else { $quotedArgs += $a }
    }
    $psi.Arguments = ($quotedArgs -join " ")
    $proc = [System.Diagnostics.Process]::new()
    $proc.StartInfo = $psi
    $outText = ""
    $errText = ""
    try {
        [void]$proc.Start()
        $outTask = $proc.StandardOutput.ReadToEndAsync()
        $errTask = $proc.StandardError.ReadToEndAsync()
        $exited = $proc.WaitForExit($TimeoutMs)
        if (-not $exited) {
            try { $proc.Kill($true) } catch {}
            $proc.WaitForExit()
            $item.timed_out = $true
            $item.status = "TIMEOUT"
        } else {
            $item.exit_code = $proc.ExitCode
            $item.status = if ($proc.ExitCode -eq 0) { "PASS" } else { "FAIL" }
        }
        $outText = $outTask.GetAwaiter().GetResult()
        $errText = $errTask.GetAwaiter().GetResult()
    } catch {
        $item.status = "FAILED_TO_START"
        $item.exit_code = -2
        $item.error = $_.Exception.Message
    } finally {
        try { $proc.Dispose() } catch {}
        foreach ($k in $envSnapshot.Keys) {
            [System.Environment]::SetEnvironmentVariable($k, $envSnapshot[$k])
        }
    }
    $content = "COMMAND: $File $($ArgList -join ' ')" +
               "`nTIMEOUT_MS: $TimeoutMs`nEXIT_CODE: $($item.exit_code)`n" +
               "STATUS: $($item.status)`n`n--- STDOUT ---`n$outText`n--- STDERR ---`n$errText"
    Set-Content -LiteralPath $LogPath -Value $content -Encoding UTF8
    $results.Add([pscustomobject]$item)
}

# ---- 1. MSVC ASan（CPU 核心组件）----
$asanLog = Join-Path $LogDir "msvc_asan_stress.log"
Invoke-WithTimeout -Name "msvc_asan_cpu_core" -File $MsvcAsanExe `
    -ArgList @("--stress") -LogPath $asanLog -TimeoutMs $TimeoutMsAsan `
    -EnvPairs @()

# ---- 2. CUDA compute-sanitizer memcheck（扩缩容 + 多块卷积）----
$chunkExe = Join-Path $BuildDir "bin\acr_test_cuda_bridge_chunk.exe"
$memcheckLog = Join-Path $LogDir "compute_sanitizer_memcheck_chunk.log"
$memcheckCsLog = Join-Path $LogDir "compute_sanitizer_memcheck_chunk.cslog"
$csExe = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\compute-sanitizer\compute-sanitizer.exe"
if (Test-Path -LiteralPath $chunkExe) {
    if (Test-Path -LiteralPath $csExe) {
        # compute-sanitizer 冷启动需要先 warm-up（驱动/JIT 缓存），否则
        # 报 "Target application terminated before first instrumented API call"
        $warmLog = Join-Path $LogDir "warmup_cuda_bridge_chunk.log"
        Invoke-WithTimeout -Name "warmup_cuda_bridge_chunk" -File $chunkExe `
            -ArgList @("--gtest_filter=CudaBridgeChunk.*") -LogPath $warmLog `
            -TimeoutMs $TimeoutMsCuda `
            -EnvPairs @("ACR_CUDA_BRIDGE_DLL=$CudaBridgeDll") -WorkDir $BuildDir
        Invoke-WithTimeout -Name "compute_sanitizer_memcheck" `
            -File $csExe `
            -ArgList @("--tool", "memcheck", "--log-file",
                       $memcheckCsLog, $chunkExe, "--gtest_filter=CudaBridgeChunk.*") `
            -LogPath $memcheckLog -TimeoutMs $TimeoutMsCuda `
            -EnvPairs @("ACR_CUDA_BRIDGE_DLL=$CudaBridgeDll") `
            -WorkDir $BuildDir
    } else {
        $results.Add([pscustomobject]@{
            name = "compute_sanitizer_memcheck"; status = "SKIPPED";
            exit_code = -1; timeout_ms = $TimeoutMsCuda; timed_out = $false;
            log = $memcheckLog
        })
    }
}

# ---- 3. CUDA compute-sanitizer racecheck（真实 kernel 竞争）----
$bridgeExe = Join-Path $BuildDir "bin\acr_test_cuda_bridge.exe"
$raceLog = Join-Path $LogDir "compute_sanitizer_racecheck.log"
$raceCsLog = Join-Path $LogDir "compute_sanitizer_racecheck.cslog"
if (Test-Path -LiteralPath $bridgeExe) {
    if (Test-Path -LiteralPath $csExe) {
        $warmLog2 = Join-Path $LogDir "warmup_cuda_bridge.log"
        Invoke-WithTimeout -Name "warmup_cuda_bridge" -File $bridgeExe `
            -ArgList @("--gtest_filter=CudaBridge.*") -LogPath $warmLog2 `
            -TimeoutMs $TimeoutMsCuda `
            -EnvPairs @("ACR_CUDA_BRIDGE_DLL=$CudaBridgeDll") -WorkDir $BuildDir
        Invoke-WithTimeout -Name "compute_sanitizer_racecheck" `
            -File $csExe `
            -ArgList @("--tool", "racecheck", "--log-file",
                       $raceCsLog, $bridgeExe, "--gtest_filter=CudaBridge.*") `
            -LogPath $raceLog -TimeoutMs $TimeoutMsCuda `
            -EnvPairs @("ACR_CUDA_BRIDGE_DLL=$CudaBridgeDll") `
            -WorkDir $BuildDir
    } else {
        $results.Add([pscustomobject]@{
            name = "compute_sanitizer_racecheck"; status = "SKIPPED";
            exit_code = -1; timeout_ms = $TimeoutMsCuda; timed_out = $false;
            log = $raceLog
        })
    }
}

# ---- 汇总 ----
$summaryJson = Join-Path $LogDir "sanitizer_summary.json"
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryJson -Encoding UTF8

$md = "# ACR Sanitizer Evidence Summary`n`n"
foreach ($r in $results) {
    $md += "- $($r.name): $($r.status) (exit=$($r.exit_code), timeout_ms=$($r.timeout_ms), timed_out=$($r.timed_out))`n"
}
$md += "`nLog dir: $LogDir`n"
Set-Content -LiteralPath (Join-Path $LogDir "sanitizer_summary.md") -Value $md -Encoding UTF8

$results | Format-Table -AutoSize
$failed = $results | Where-Object { $_.status -eq "FAIL" -or $_.status -eq "TIMEOUT" }
if ($failed) {
    Write-Output "SANITIZER EVIDENCE: $($failed.Count) FAILED/TIMEOUT"
    exit 1
}
Write-Output "SANITIZER EVIDENCE: all recorded ($($results.Count) items)"
