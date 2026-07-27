# P06-002 run_stage2.ps1 - 执行 stage2 测试矩阵
# 每个 stage2 运行设置明确超时 (180 秒)
# 捕获 stdout/stderr/exitcode, 并在成功时生成 hcsd_inspect.log

param(
    [Parameter(Mandatory=$true)][string]$TestId,
    [Parameter(Mandatory=$true)][string]$InputDir,
    [Parameter(Mandatory=$true)][string]$OutputHcsd,
    [Parameter(Mandatory=$true)][string]$Config,
    [Parameter(Mandatory=$true)][string]$LogsDir,
    [int]$TimeoutSec = 180
)

$ErrorActionPreference = "Stop"

# 设置 DLL 依赖路径
$env:Path = "f:\Astro dev\Astro CS Normalization Database\build\artifacts;C:\msys64\mingw64\bin;" + $env:Path

$orchestrator = "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp\orchestrator.exe"

# 确保日志目录存在
New-Item -ItemType Directory -Path $LogsDir -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path $OutputHcsd -Parent) -Force | Out-Null

$stdoutLog = Join-Path $LogsDir "stage2_stdout.log"
$stderrLog = Join-Path $LogsDir "stage2_stderr.log"
$exitcodeFile = Join-Path $LogsDir "stage2_stdout.log.exitcode.txt"

Write-Host "[$TestId] 开始 stage2 运行: InputDir=$InputDir Config=$Config Output=$OutputHcsd"

# 用 Start-Process 启动, 便于超时控制
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $orchestrator
$psi.Arguments = "stage2 --frames `"$InputDir`" --output `"$OutputHcsd`" --config `"$Config`" --log-level DEBUG"
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
$psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8

$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi

# 同步捕获 stdout/stderr
$outBuilder = New-Object System.Text.StringBuilder
$errBuilder = New-Object System.Text.StringBuilder
$eventOut = {
    if ($EventArgs.Data -ne $null) { $OutBuilder.AppendLine($EventArgs.Data) | Out-Null }
}
$eventErr = {
    if ($EventArgs.Data -ne $null) { $ErrBuilder.AppendLine($EventArgs.Data) | Out-Null }
}
Register-ObjectEvent -InputObject $proc -EventName OutputDataReceived -Action $eventOut | Out-Null
Register-ObjectEvent -InputObject $proc -EventName ErrorDataReceived -Action $eventErr | Out-Null

$proc.Start() | Out-Null
$proc.BeginOutputReadLine()
$proc.BeginErrorReadLine()

if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
    try { $proc.Kill() } catch {}
    $ec = -1
    $timeoutMsg = "[$TestId] TIMEOUT: stage2 在 ${TimeoutSec}s 内未完成, 已强制终止"
    Write-Host $timeoutMsg
    $errBuilder.AppendLine($timeoutMsg) | Out-Null
} else {
    $ec = $proc.ExitCode
}

try { $proc.CancelOutputRead() } catch {}
try { $proc.CancelErrorRead() } catch {}

# 写入日志文件 (UTF-8)
[System.IO.File]::WriteAllText($stdoutLog, $outBuilder.ToString(), [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($stderrLog, $errBuilder.ToString(), [System.Text.UTF8Encoding]::new($false))
"$ec" | Out-File -FilePath $exitcodeFile -Encoding ascii -NoNewline

Write-Host "[$TestId] exit_code=$ec stdout=$($outBuilder.Length) chars stderr=$($errBuilder.Length) chars"

# 如果成功, 运行 inspect
if ($ec -eq 0 -and (Test-Path $OutputHcsd)) {
    $inspectLog = Join-Path $LogsDir "hcsd_inspect.log"
    & $orchestrator inspect --hcsd $OutputHcsd 2>$null | Out-File -FilePath $inspectLog -Encoding utf8
    Write-Host "[$TestId] hcsd_inspect.log 已生成"
}

# 计算 HCSD SHA-256
if (Test-Path $OutputHcsd) {
    $hash = (Get-FileHash -Path $OutputHcsd -Algorithm SHA256).Hash
    $size = (Get-Item $OutputHcsd).Length
    Write-Host "[$TestId] HCSD: size=$size SHA256=$hash"
    "$hash  $size" | Out-File -FilePath (Join-Path $LogsDir "hcsd_sha256.txt") -Encoding ascii -NoNewline
}

return $ec
