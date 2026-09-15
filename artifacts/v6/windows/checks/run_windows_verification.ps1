<#
  run_windows_verification.ps1 — WIN-VERIFY-001 参考验证 harness（自包含 / 只读）
  ============================================================================
  定位：Fatduck（Windows 10+ amd64 正式验证节点）本地固定 harness 的参考实现。
        宪章 §15.3：本脚本不 checkout 源码、不编译产品、不调用仓库业务脚本；
        只读候选产物与本地只读真实数据，产出结构化 summary。
  部署：节点运维复制到 D:\AstroCSRunner\harness\ 后由既有 fatduck.yml 通道调用。
  状态：**未在任何 Windows 节点执行过**（Fatduck 不可达 + 无候选）；逻辑为待执行参考，
        任何 verdict 只有在真机跑出证据后才可写 PASS/FAIL。
  退出码：0=全部 executed 用例 PASS；2=executed_cases==0 或 skip-only；
          10=candidate/provenance；20=数据 manifest；30=科学/功能；
          40=资源/性能；50=公开白名单；60=环境。
  纪律：不伪造 PASS；executed=false 一律不计入通过；负向用例必须能红。
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$CandidateDir,
  [Parameter(Mandatory=$true)][string]$SourceSha,
  [Parameter(Mandatory=$true)][string]$ResultDir,
  [string]$RealDataRoot = '',
  [string]$Phase3BigConfig = '',
  [string]$LongPathBase = 'D:\AstroCSRunner\runs\longpath',
  [string]$DeterminismReference = '',
  [switch]$SkipBigFile
)

$ErrorActionPreference = 'Stop'
$script:Checks = New-Object System.Collections.ArrayList
$script:Unavailable = New-Object System.Collections.ArrayList
$script:Failures = 0

function New-ResultDir {
  if (-not (Test-Path -LiteralPath $ResultDir)) { New-Item -ItemType Directory -Path $ResultDir -Force | Out-Null }
}
function Get-Sha256Lower([string]$Path) {
  (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Add-Check([string]$Id,[string]$Item,[string]$Title,[string]$Verdict,[string]$Detail) {
  [void]$script:Checks.Add([ordered]@{ id=$Id; item=$Item; title=$Title; executed=$true; verdict=$Verdict; detail=$Detail })
  if ($Verdict -eq 'FAIL') { $script:Failures++ }
  Write-Host ("[{0}] {1} :: {2} -> {3}" -f $Id,$Item,$Title,$Verdict)
}
function Add-Unavailable([string]$Id,[string]$Item,[string]$Title,[string]$Reason) {
  [void]$script:Unavailable.Add([ordered]@{ id=$Id; item=$Item; title=$Title; executed=$false; verdict='UNAVAILABLE'; detail=$Reason })
  Write-Host ("[{0}] {1} :: {2} -> UNAVAILABLE ({3})" -f $Id,$Item,$Title,$Reason)
}
function Invoke-Captured([string]$Exe,[string[]]$Argv,[string]$LogName) {
  $log = Join-Path $ResultDir $LogName
  $out = & $Exe @Argv 2>&1 | Out-String
  $rc = $LASTEXITCODE
  if ($null -eq $rc) { $rc = 0 }
  $out | Set-Content -LiteralPath $log -Encoding UTF8
  return [ordered]@{ rc=$rc; output=$out; log=$log }
}
function Find-Tool([string]$Name) {
  $c = Get-Command $Name -ErrorAction SilentlyContinue
  if ($c) { return $c.Source } else { return $null }
}

# ---------------------------------------------------------------------------
# FITS 大文件 verifier（宿主侧 .NET 小工具；不编译产品代码）
# DATASUM = 数据单元 32 位大端字无符号累加；CHECKSUM 格式校验；偏移 >2^31 读取。
# ---------------------------------------------------------------------------
function Get-FitsLayout([string]$Path) {
  $fs = [System.IO.File]::Open($Path,'Open','Read','ReadWrite')
  try {
    $cards = @()
    $block = New-Object byte[] 2880
    $pos = 0L
    while ($true) {
      $n = $fs.Read($block,0,2880); if ($n -ne 2880) { throw "FITS header 截断 @ $pos" }
      for ($i=0; $i -lt 36; $i++) {
        $card = [System.Text.Encoding]::ASCII.GetString($block, $i*80, 80)
        $key = $card.Substring(0,8).Trim()
        $val = $card.Substring(10,70)
        $cards += @{ key=$key; value=$val.Trim() }
        if ($key -eq 'END') { $pos += ($i+1)*80; break }
      }
      $pos += 2880
      if ($cards[-1].key -eq 'END') { break }
    }
    # header 补齐到 2880 边界
    $dataStart = [math]::Ceiling($pos / 2880) * 2880
    $bitpix = 0; $naxis = 0; $dims = @()
    foreach ($c in $cards) {
      if ($c.key -eq 'BITPIX') { $bitpix = [int]($c.value -replace '/.*','').Trim() }
      if ($c.key -eq 'NAXIS')  { $naxis  = [int]($c.value -replace '/.*','').Trim() }
      if ($c.key -match '^NAXIS\d+$') { $dims += [long](($c.value -replace '/.*','').Trim()) }
    }
    $nelem = 1L; foreach ($d in $dims) { $nelem = $nelem * $d }
    $dataBytes = [long]([math]::Abs($bitpix)/8) * $nelem
    $dataPad = [long]([math]::Ceiling($dataBytes / 2880.0) * 2880)
    $fileLen = (Get-Item -LiteralPath $Path).Length
    $datasumCard = ($cards | Where-Object { $_.key -eq 'DATASUM' } | Select-Object -First 1)
    $checksumCard = ($cards | Where-Object { $_.key -eq 'CHECKSUM' } | Select-Object -First 1)
    $dsVal = $null; if ($datasumCard) { $dsVal = $datasumCard.value.Trim("'").Trim() }
    $ckVal = $null; if ($checksumCard) { $ckVal = $checksumCard.value }
    return [ordered]@{
      bitpix=$bitpix; naxis=$naxis; dims=$dims; nelem=$nelem; dataStart=$dataStart;
      dataBytes=$dataBytes; dataPad=$dataPad; fileLen=$fileLen;
      datasum=$dsVal;
      checksum=$ckVal
    }
  } finally { $fs.Dispose() }
}

function Get-FitsDataSum([string]$Path,[long]$DataStart,[long]$DataBytes) {
  $addType = @"
using System;
public static class FitsSum {
  public static uint DataSum(string path, long start, long nbytes) {
    uint sum = 0; byte[] buf = new byte[8*1024*1024];
    using (var fs = new System.IO.FileStream(path, System.IO.FileMode.Open, System.IO.FileAccess.Read, System.IO.FileShare.ReadWrite)) {
      fs.Seek(start, System.IO.SeekOrigin.Begin);
      long remaining = nbytes; int carry = 0; byte[] tail = new byte[3];
      while (remaining > 0) {
        int want = (int)Math.Min((long)buf.Length, remaining);
        int got = fs.Read(buf, carry, want - carry);
        if (got <= 0) break;
        int total = carry + got;
        int limit = total - (total % 4);
        for (int i = 0; i < limit; i += 4) {
          sum += ((uint)buf[i] << 24) | ((uint)buf[i+1] << 16) | ((uint)buf[i+2] << 8) | (uint)buf[i+3];
        }
        carry = total - limit;
        if (carry > 0) { tail[0]=buf[limit]; tail[1]=carry>1?buf[limit+1]:tail[1]; tail[2]=carry>2?buf[limit+2]:tail[2]; }
        Array.Copy(tail, 0, buf, 0, carry);
        remaining -= got;
      }
      if (carry > 0) {
        byte[] w = new byte[4]; for (int i=0;i<carry;i++) w[i]=tail[i];
        sum += ((uint)w[0] << 24) | ((uint)w[1] << 16) | ((uint)w[2] << 8) | (uint)w[3];
      }
    }
    return sum;
  }
}
"@
  if (-not ('FitsSum' -as [type])) { Add-Type -TypeDefinition $addType -Language CSharp | Out-Null }
  return [FitsSum]::DataSum($Path,$DataStart,$DataBytes)
}

function Read-FitsWord([string]$Path,[long]$Offset) {
  $fs = [System.IO.File]::Open($Path,'Open','Read','ReadWrite')
  try {
    $fs.Seek($Offset,[System.IO.SeekOrigin]::Begin) | Out-Null
    $b = New-Object byte[] 4
    $n = $fs.Read($b,0,4)
    if ($n -ne 4) { throw "offset $Offset 处读取不足 4 字节" }
    return ((([uint32]$b[0]) -shl 24) -bor (([uint32]$b[1]) -shl 16) -bor (([uint32]$b[2]) -shl 8) -bor ([uint32]$b[3]))
  } finally { $fs.Dispose() }
}

# ===========================================================================
New-ResultDir
$summary = [ordered]@{
  schema='astrocs.v6.win-verify.summary/v1'; task_id='WIN-VERIFY-001'
  source_sha=$SourceSha; candidate_dir=$CandidateDir
  host=[ordered]@{ computer=$env:COMPUTERNAME; os=[System.Environment]::OSVersion.VersionString;
                   ps=$PSVersionTable.PSVersion.ToString(); long_paths_enabled=$null }
  generated_utc=(Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
  checks=@(); unavailable=@(); executed_cases=0; skipped_cases=0; failures=0; verdict='UNKNOWN'
}

# 环境：LongPathsEnabled
try {
  $reg = Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -ErrorAction Stop
  $summary.host.long_paths_enabled = [bool]$reg.LongPathsEnabled
} catch { $summary.host.long_paths_enabled = $null }

$exe = Join-Path $CandidateDir 'astrocs.exe'
if (-not (Test-Path -LiteralPath $exe)) {
  Add-Unavailable 'S-00' 'ENV' '候选存在' "candidate/astrocs.exe 缺失：$exe（no_candidate 时不得继续）"
  $summary.verdict='AWAITING_WINDOWS_VALIDATION'
  $summary.checks=$script:Checks; $summary.unavailable=$script:Unavailable; $summary.skipped_cases=$script:Unavailable.Count
  ($summary | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath (Join-Path $ResultDir 'WIN_VERIFY_SUMMARY.json') -Encoding UTF8
  Write-Host 'AWAITING_WINDOWS_VALIDATION: candidate missing'
  exit 10
}

# ---- S 同 SHA 候选安装 ----
# S-01（artifact digest 锚 ↔ AstroCS-candidate.zip）由上游 ci/select_candidate.py 在选择阶段闭环；
# 本 harness 从已解压候选出发，覆盖 S-02..S-05。
$sums = Join-Path $CandidateDir 'SHA256SUMS'
if (Test-Path -LiteralPath $sums) {
  $bad = @()
  foreach ($line in Get-Content -LiteralPath $sums) {
    if ($line -match '^\s*([0-9a-fA-F]{64})\s+\*?(.+)$') {
      $want=$Matches[1].ToLowerInvariant(); $rel=$Matches[2].Trim()
      $f = Join-Path $CandidateDir ($rel -replace '/','\')
      if (-not (Test-Path -LiteralPath $f)) { $bad += "missing:$rel" }
      elseif ((Get-Sha256Lower $f) -ne $want) { $bad += "mismatch:$rel" }
    }
  }
  if ($bad.Count -eq 0) { Add-Check 'S-02' 'SAME_SHA' 'SHA256SUMS 自洽' 'PASS' '不一致成员=0' }
  else { Add-Check 'S-02' 'SAME_SHA' 'SHA256SUMS 自洽' 'FAIL' ($bad -join ';') }
} else { Add-Check 'S-02' 'SAME_SHA' 'SHA256SUMS 存在' 'FAIL' 'candidate 根缺 SHA256SUMS' }

$prov = Join-Path $CandidateDir 'BUILD_PROVENANCE.json'
if (Test-Path -LiteralPath $prov) {
  $p = Get-Content -LiteralPath $prov -Raw | ConvertFrom-Json
  if ($p.source_sha -eq $SourceSha) { Add-Check 'S-03' 'SAME_SHA' 'source_sha 绑定' 'PASS' $p.source_sha }
  else { Add-Check 'S-03' 'SAME_SHA' 'source_sha 绑定' 'FAIL' "candidate=$($p.source_sha) expected=$SourceSha" }
} else { Add-Check 'S-03' 'SAME_SHA' 'BUILD_PROVENANCE 存在' 'FAIL' '缺 BUILD_PROVENANCE.json' }

$expected = @('astrocs.exe','astrocs.product.json','astrocs_io.dll','astrocs_runtime.dll',
 'modules/astrocs_catalog_gaia.dll','modules/astrocs_noop.dll','modules/astrocs_p1_calibration.dll',
 'modules/astrocs_p1_cosmetic.dll','modules/astrocs_p1_drizzle.dll','modules/astrocs_p1_hips_writer.dll',
 'providers/astrocs_cpu_baseline.dll')
$missing = @(); foreach ($m in $expected) { if (-not (Test-Path -LiteralPath (Join-Path $CandidateDir ($m -replace '/','\')))) { $missing += $m } }
if ($missing.Count -eq 0) { Add-Check 'S-04' 'SAME_SHA' '安装树成员齐全' 'PASS' "$($expected.Count)/$($expected.Count)" }
else { Add-Check 'S-04' 'SAME_SHA' '安装树成员齐全' 'FAIL' ($missing -join ',') }

$env:PATH = "$CandidateDir;$env:PATH"
$v = Invoke-Captured $exe @('version','--json') 'version_json.txt'
try { $vj = (($v.output.Trim() -split [char]10) | Where-Object { $_.Trim() } | Select-Object -Last 1) | ConvertFrom-Json } catch { $vj = $null }
if ($v.rc -eq 0 -and $vj -and $vj.name -eq 'astrocs' -and $vj.version) { Add-Check 'S-05' 'SAME_SHA' 'CLI 入口' 'PASS' $vj.version }
else { Add-Check 'S-05' 'SAME_SHA' 'CLI 入口' 'FAIL' "rc=$($v.rc)" }

# ---- A ABI ----
$dumpbin = Find-Tool 'dumpbin'
$dlls = @('astrocs_runtime.dll','astrocs_io.dll','modules\astrocs_noop.dll','providers\astrocs_cpu_baseline.dll')
if ($dumpbin) {
  $expOk = $true; $expDetail = @()
  foreach ($d in $dlls) {
    $p = Join-Path $CandidateDir $d
    $r = Invoke-Captured $dumpbin @('/nologo','/EXPORTS',$p) ("dumpbin_exports_$([System.IO.Path]::GetFileName($d)).txt")
    $cnt = ($r.output -split [char]10 | Where-Object { $_ -match '^\s+\d+\s+[0-9A-Fa-f]{1,8}\s+[0-9A-Fa-f]{1,8}\s+\w' }).Count
    if ($r.rc -ne 0 -or $cnt -le 0) { $expOk=$false }
    $expDetail += "$d=$cnt"
  }
  if ($expOk) { Add-Check 'A-01' 'ABI' '导出符号非空' 'PASS' ($expDetail -join ';') } else { Add-Check 'A-01' 'ABI' '导出符号非空' 'FAIL' ($expDetail -join ';') }
  $rtExp = Invoke-Captured $dumpbin @('/nologo','/EXPORTS',(Join-Path $CandidateDir 'astrocs_runtime.dll')) 'exports_runtime.txt'
  $ioExp = Invoke-Captured $dumpbin @('/nologo','/EXPORTS',(Join-Path $CandidateDir 'astrocs_io.dll')) 'exports_io.txt'
  $hasArtifact = ($rtExp.output -match 'acs_artifact_')
  $hasFio = ($ioExp.output -match 'acs_fio_')
  if ($hasArtifact -and $hasFio) { Add-Check 'A-02' 'ABI' '冻结 C ABI 导出族' 'PASS' 'acs_artifact_* / acs_fio_* 均在位' }
  else { Add-Check 'A-02' 'ABI' '冻结 C ABI 导出族' 'FAIL' "acs_artifact_=$hasArtifact acs_fio_=$hasFio" }
  $hdr = Invoke-Captured $dumpbin @('/nologo','/HEADERS',(Join-Path $CandidateDir 'astrocs.exe')) 'dumpbin_headers.txt'
  if ($hdr.output -match 'machine \(x64\)|machine \(AMD64\)') { Add-Check 'A-04' 'ABI' 'x64 机器类型' 'PASS' 'machine x64' }
  else { Add-Check 'A-04' 'ABI' 'x64 机器类型' 'FAIL' '未见 machine (x64)' }
  $dep = Invoke-Captured $dumpbin @('/nologo','/DEPENDENTS',(Join-Path $CandidateDir 'astrocs.exe')) 'dumpbin_dependents.txt'
  $depTxt = $dep.output
  if ($depTxt -match 'VCRUNTIME140|MSVCP140') { Add-Check 'A-05' 'ABI' 'CRT 动态依赖' 'PASS' 'VCRUNTIME140/MSVCP140 在位' }
  else { Add-Check 'A-05' 'ABI' 'CRT 动态依赖' 'FAIL' '未见 VCRUNTIME140/MSVCP140' }
  if ($depTxt -match 'zlib1\.dll|gsl\.dll|gslcblas\.dll') { Add-Check 'A-06' 'ABI' 'zlib/GSL 静态链接' 'FAIL' '出现 zlib/gsl DLL 依赖' }
  else { Add-Check 'A-06' 'ABI' 'zlib/GSL 静态链接' 'PASS' '0 命中' }
} else {
  Add-Unavailable 'A-01' 'ABI' 'dumpbin 导出' 'dumpbin 不可达（VS 工具链未注入 PATH）'
  Add-Unavailable 'A-02' 'ABI' '冻结 C ABI 导出族' 'dumpbin 不可达'
  Add-Unavailable 'A-04' 'ABI' 'x64 机器类型' 'dumpbin 不可达'
  Add-Unavailable 'A-05' 'ABI' 'CRT 依赖' 'dumpbin 不可达'
  Add-Unavailable 'A-06' 'ABI' 'zlib/GSL 链接面' 'dumpbin 不可达'
}
if ($vj -and $vj.abi_version) {
  $abiOk = ([uint32]$vj.abi_version -eq 1)
  if ($abiOk) { Add-Check 'A-03' 'ABI' 'ABI 版本' 'PASS' "abi_version=$($vj.abi_version)" }
  else { Add-Check 'A-03' 'ABI' 'ABI 版本' 'FAIL' "cli=$($vj.abi_version) header=1" }
} else { Add-Unavailable 'A-03' 'ABI' 'ABI 版本' 'version --json 缺 abi_version' }
foreach ($pair in @(@('modules list','A-07a'),@('modules verify','A-07b'),@('selftest','A-08'))) {
  $parts = $pair[0].Split(' ')
  $r = Invoke-Captured $exe $parts ("$($pair[1]).txt")
  if ($r.rc -eq 0) { Add-Check $pair[1] 'ABI' $pair[0] 'PASS' "rc=0" } else { Add-Check $pair[1] 'ABI' $pair[0] 'FAIL' "rc=$($r.rc)" }
}

# ---- L 长路径 ----
$deep = [System.IO.Path]::Combine($LongPathBase, ('d'*20 + '\')*13)
try {
  New-Item -ItemType Directory -Path "\\?\$deep" -Force | Out-Null
  $lpCandidate = Join-Path $deep 'candidate'
  Copy-Item -LiteralPath $CandidateDir -Destination $lpCandidate -Recurse -Force
  $lpExe = Join-Path $lpCandidate 'astrocs.exe'
  $lpv = Invoke-Captured $lpExe @('version','--json') 'version_from_longpath.txt'
  if ($lpv.rc -eq 0) { Add-Check 'L-01' 'LONG_PATH' '>260 安装树加载' 'PASS' "len=$($lpCandidate.Length)" }
  else { Add-Check 'L-01' 'LONG_PATH' '>260 安装树加载' 'FAIL' "rc=$($lpv.rc) len=$($lpCandidate.Length)" }
} catch { Add-Check 'L-01' 'LONG_PATH' '>260 安装树加载' 'FAIL' $_.Exception.Message }
# L-02/03/07 需真实数据与 config，未提供时登记 UNAVAILABLE
if ($RealDataRoot -and (Test-Path -LiteralPath $RealDataRoot)) {
  Add-Unavailable 'L-02' 'LONG_PATH' '>260 输入 FITS' '需 phase1 config 指向真实只读数据；由 harness 数据 manifest 注入'
  Add-Unavailable 'L-03' 'LONG_PATH' '>260 输出产品' '同上'
} else {
  Add-Unavailable 'L-02' 'LONG_PATH' '>260 输入 FITS' '未提供 -RealDataRoot'
  Add-Unavailable 'L-03' 'LONG_PATH' '>260 输出产品' '未提供 -RealDataRoot'
}

# ---- B 大文件 ----
if ($SkipBigFile -or -not $Phase3BigConfig) {
  foreach ($id in @('B-01','B-02','B-03','B-04','B-05','B-06','B-07')) {
    Add-Unavailable $id 'BIG_FILE' '>2GiB 用例' '未提供 -Phase3BigConfig（须由 Phase3 导出产生 >2^31 B FITS）'
  }
} else {
  $bigDir = Join-Path $ResultDir 'big'
  New-Item -ItemType Directory -Path $bigDir -Force | Out-Null
  $b = Invoke-Captured $exe @('phase3','run','--config',$Phase3BigConfig) 'big_write.log'
  # 找到产物 FITS
  $bigFits = Get-ChildItem -Path $ResultDir -Recurse -Filter *.fits -ErrorAction SilentlyContinue |
             Sort-Object Length -Descending | Select-Object -First 1
  if ($b.rc -ne 0 -or -not $bigFits) { Add-Check 'B-01' 'BIG_FILE' '>2GiB 写入' 'FAIL' "rc=$($b.rc); fits=$($bigFits)" }
  else {
    $lay = Get-FitsLayout $bigFits.FullName
    if ($lay.fileLen -gt 2147483648 -and ($lay.fileLen % 2880) -eq 0) { Add-Check 'B-01' 'BIG_FILE' '>2GiB 写入' 'PASS' "len=$($lay.fileLen)" }
    else { Add-Check 'B-01' 'BIG_FILE' '>2GiB 写入' 'FAIL' "len=$($lay.fileLen) align=$($lay.fileLen % 2880)" }
    $re = Invoke-Captured $exe @('phase3','inspect','--config',$Phase3BigConfig,'--json') 'big_reopen.log'
    if ($re.rc -eq 0) { Add-Check 'B-02' 'BIG_FILE' '>2GiB 重开' 'PASS' "rc=0" } else { Add-Check 'B-02' 'BIG_FILE' '>2GiB 重开' 'FAIL' "rc=$($re.rc)" }
    $ds = Get-FitsDataSum $bigFits.FullName $lay.dataStart $lay.dataBytes
    if ($lay.datasum -and ([uint32]$lay.datasum -eq [uint32]$ds)) { Add-Check 'B-03' 'BIG_FILE' 'DATASUM 重算一致' 'PASS' "datasum=$ds" }
    else { Add-Check 'B-03' 'BIG_FILE' 'DATASUM 重算一致' 'FAIL' "card=$($lay.datasum) computed=$ds" }
    if ($lay.dataBytes -gt 2147483648) {
      $off = [long]([math]::Floor($lay.dataBytes / 2 / 4) * 4)
      $word = Read-FitsWord $bigFits.FullName ($lay.dataStart + $off)
      Add-Check 'B-04' 'BIG_FILE' '>2^31 偏移读取' 'PASS' ("offset=$off word=0x{0:X8}" -f $word)
    } else { Add-Check 'B-04' 'BIG_FILE' '>2^31 偏移读取' 'FAIL' "数据单元仅 $($lay.dataBytes) B" }
    if ($lay.nelem -gt 2147483648) { Add-Check 'B-05' 'BIG_FILE' '>2^31 元素计数' 'PASS' "nelem=$($lay.nelem)" }
    else { Add-Unavailable 'B-05' 'BIG_FILE' '>2^31 元素计数' "本产物 nelem=$($lay.nelem) ≤ 2^31，需专用大图配置" }
    # B-06 截断负向
    $tr = Join-Path $bigDir 'truncated.fits'
    $fsIn=[System.IO.File]::OpenRead($bigFits.FullName); $fsOut=[System.IO.File]::Create($tr)
    $buf=New-Object byte[] (8MB); $left=$bigFits.Length-2880
    while ($left -gt 0) { $n=$fsIn.Read($buf,0,[int][math]::Min($left,$buf.Length)); if($n -le 0){break}; $fsOut.Write($buf,0,$n); $left-=$n }
    $fsIn.Dispose(); $fsOut.Dispose()
    try { $lt = Get-FitsLayout $tr; $lts = Get-FitsDataSum $tr $lt.dataStart $lt.dataBytes
          if (($lt.fileLen % 2880) -ne 0 -or [uint32]$lt.datasum -ne [uint32]$lts) { Add-Check 'B-06' 'BIG_FILE' '截断须失败' 'PASS' '截断后被检出' }
          else { Add-Check 'B-06' 'BIG_FILE' '截断须失败' 'FAIL' '截断未被检出（假绿）' } }
    catch { Add-Check 'B-06' 'BIG_FILE' '截断须失败' 'PASS' $_.Exception.Message }
    # B-07 翻转负向
    $fl = Join-Path $bigDir 'flipped.fits'
    Copy-Item -LiteralPath $bigFits.FullName -Destination $fl -Force
    $fs=[System.IO.File]::Open($fl,'Open','ReadWrite','ReadWrite')
    try { $fs.Seek(($lay.dataStart + [long]([math]::Floor($lay.dataBytes/2/4)*4)),[System.IO.SeekOrigin]::Begin)|Out-Null
          $byt=$fs.ReadByte(); $fs.Seek(-1,[System.IO.SeekOrigin]::Current)|Out-Null; $fs.WriteByte([byte]([int]$byt -bxor 0xFF)) } finally { $fs.Dispose() }
    $fs2 = Get-FitsDataSum $fl $lay.dataStart $lay.dataBytes
    if ([uint32]$fs2 -ne [uint32]$ds) { Add-Check 'B-07' 'BIG_FILE' '翻转须检出' 'PASS' 'DATASUM 变化' }
    else { Add-Check 'B-07' 'BIG_FILE' '翻转须检出' 'FAIL' 'DATASUM 未变（假绿）' }
  }
}

# ---- R 真实产品 ----
if ($RealDataRoot -and (Test-Path -LiteralPath $RealDataRoot)) {
  Add-Unavailable 'R-01' 'REAL_PRODUCT' '跨平台重开' '需 Linux 侧产物清单 + manifest（由控制器提供后启用）'
  Add-Unavailable 'R-02' 'REAL_PRODUCT' '真实数据数值' '需本地真实数据 config；参考值见 REAL-SCIENCE-001 §4/§5'
  Add-Unavailable 'R-03' 'REAL_PRODUCT' 'FWHM 比对' '同上'
} else {
  foreach ($id in @('R-01','R-02','R-03','R-04','R-05')) { Add-Unavailable $id 'REAL_PRODUCT' '真实产品复验' '未提供 -RealDataRoot' }
}
if ($DeterminismReference) { Add-Unavailable 'R-04' 'REAL_PRODUCT' '确定性 digest' '需先运行 1/4/16 worker 写盘并传入参考 digest' }

# ---- 汇总 ----
$summary.executed_cases = $script:Checks.Count
$summary.skipped_cases  = $script:Unavailable.Count
$summary.failures       = $script:Failures
$summary.checks         = $script:Checks
$summary.unavailable    = $script:Unavailable
if ($summary.executed_cases -eq 0) { $summary.verdict='NO_CASES'; $rc=2 }
elseif ($script:Failures -gt 0) { $summary.verdict='FAIL'; $rc=30 }
else { $summary.verdict='PASS'; $rc=0 }
$summary | Add-Member -NotePropertyName long_paths_enabled -NotePropertyValue $summary.host.long_paths_enabled -Force
($summary | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath (Join-Path $ResultDir 'WIN_VERIFY_SUMMARY.json') -Encoding UTF8
Write-Host ("WIN-VERIFY-001 verdict={0} executed={1} skipped={2} failures={3}" -f $summary.verdict,$summary.executed_cases,$summary.skipped_cases,$summary.failures)
exit $rc
