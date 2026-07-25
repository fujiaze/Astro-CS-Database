# ============================================================================
# P02-001 PlateSolve TestData Manifest 生成脚本
# 扫描 testdata/ 下所有 .fts 文件，计算 SHA-256，解析 target/filter/exposure
# 输出: engineering/evidence/P02-001/testdata_manifest.json
#       engineering/contracts/testdata_manifest.csv
# ============================================================================
[CmdletBinding()]
param(
    [string]$ProjectRoot = "f:\Astro dev\Astro CS Normalization Database"
)

$ErrorActionPreference = "Stop"
$testdataDir = Join-Path $ProjectRoot "testdata"
$evidenceDir = Join-Path $ProjectRoot "engineering\evidence\P02-001"
$contractsDir = Join-Path $ProjectRoot "engineering\contracts"
$outJson = Join-Path $evidenceDir "testdata_manifest.json"
$outCsv = Join-Path $contractsDir "testdata_manifest.csv"

if (-not (Test-Path $evidenceDir)) { New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null }
if (-not (Test-Path $contractsDir)) { New-Item -ItemType Directory -Force -Path $contractsDir | Out-Null }

# ----------------------------------------------------------------------------
# 解析文件名: <target>-<date>@<time>-<exposure>S-<filter>.fts
# ----------------------------------------------------------------------------
function Parse-FitsName {
    param([string]$basename, [string]$filename, [string]$relPath)

    # panel 从路径推断 (Galaxy_Center_T4/lights/panelN/...)
    $panel = ""
    if ($relPath -match "panel(\d+)") { $panel = "panel$($Matches[1])" }

    # target: 截到 -<8位日期>@ 之前
    $target = ""
    if ($basename -match "^(.+?)-\d{8}@") { $target = $Matches[1] }

    # exposure: <number>S
    $exposure = ""
    if ($basename -match "-(\d+)S-") { $exposure = "$($Matches[1])S" }

    # filter: 最后一个 - 到 .fts 之间 (用 filename 含扩展名匹配)
    $filter = ""
    if ($filename -match "-([A-Za-z][A-Za-z\-]*)\.fts$") { $filter = $Matches[1] }

    # date_obs: 日期@时间
    $dateObs = ""
    if ($basename -match "-(\d{8}@\d{6})-") { $dateObs = $Matches[1] }

    return @{
        target   = $target
        panel    = $panel
        exposure = $exposure
        filter   = $filter
        date_obs = $dateObs
    }
}

# ----------------------------------------------------------------------------
# 扫描所有 .fts 文件
# ----------------------------------------------------------------------------
Write-Host "扫描 $testdataDir 下所有 .fts 文件..."
$files = Get-ChildItem -Path $testdataDir -Recurse -Filter "*.fts" -File | Sort-Object FullName
Write-Host "找到 $($files.Count) 个 .fts 文件"

# ----------------------------------------------------------------------------
# 逐文件计算 SHA-256 + 解析
# ----------------------------------------------------------------------------
$entries = New-Object System.Collections.Generic.List[object]
$byTarget = @{}
$idx = 0
foreach ($f in $files) {
    $idx++
    $hash = (Get-FileHash -Path $f.FullName -Algorithm SHA256).Hash
    $relPath = $f.FullName.Substring($ProjectRoot.Length).TrimStart('\','/')
    $parsed = Parse-FitsName -basename $f.BaseName -filename $f.Name -relPath $relPath

    # 规范化 target_name (归一到天区名)
    $targetName = $parsed.target
    if ($targetName -match "^Galaxy_Center_mosaic\d") { $targetName = "Galaxy_Center" }
    elseif ($targetName -match "^Victory_Nebula_mosaic\d") { $targetName = "Victory_Nebula" }
    elseif ($targetName -match "^NGC1727") { $targetName = "NGC1727" }
    elseif ($targetName -match "^NGC247") { $targetName = "NGC247" }
    elseif ($targetName -match "^NGC55") { $targetName = "NGC55" }
    elseif ($targetName -match "^NGC83") { $targetName = "NGC83_cluster" }
    elseif ($targetName -match "^NGC90") { $targetName = "NGC83_cluster" }
    elseif ($targetName -match "^LDN43") { $targetName = "LDN43" }

    $entry = [ordered]@{
        case_id       = "P02-001-{0:D4}" -f $idx
        index         = $idx
        target_name   = $targetName
        target_full   = $parsed.target
        panel         = $parsed.panel
        filename      = $f.Name
        filepath     = $relPath
        size_bytes    = $f.Length
        sha256        = $hash
        filter        = $parsed.filter
        exposure      = $parsed.exposure
        date_obs      = $parsed.date_obs
    }
    $entries.Add($entry)

    if (-not $byTarget.ContainsKey($targetName)) { $byTarget[$targetName] = 0 }
    $byTarget[$targetName]++

    if ($idx % 50 -eq 0) { Write-Host "  已处理 $idx / $($files.Count)..." }
}
Write-Host "全部 $($files.Count) 帧已计算 SHA-256"

# ----------------------------------------------------------------------------
# 计算 manifest 整体 SHA-256 (基于每行 sha256|filepath 拼接)
# ----------------------------------------------------------------------------
$concatStr = ($entries | ForEach-Object { "$($_.sha256)|$($_.filepath)" }) -join "`n"
$manifestHashBytes = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($concatStr))
$manifestSha256 = -join ($manifestHashBytes | ForEach-Object { $_.ToString("X2") })

# ----------------------------------------------------------------------------
# 输出 JSON
# ----------------------------------------------------------------------------
$jsonObj = [ordered]@{
    _meta = [ordered]@{
        task_id          = "P02-001"
        task_name        = "PlateSolve 全量 TestData 与旧路径基线 (v1.1 开发包)"
        phase            = "P02"
        gate             = "G2"
        commit_base      = $(Push-Location $ProjectRoot; git rev-parse HEAD; Pop-Location)
        generated_at     = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss+08:00")
        total_frames     = $entries.Count
        manifest_sha256  = $manifestSha256
        scope            = "testdata/ 下所有 .fts 文件 (lights)"
        dedup_rule       = "相同 SHA-256 视为同一帧"
    }
    by_target = $byTarget
    frames    = $entries
}

$jsonObj | ConvertTo-Json -Depth 6 | Out-File -FilePath $outJson -Encoding utf8
Write-Host "JSON manifest 已写入: $outJson ($($entries.Count) entries)"

# ----------------------------------------------------------------------------
# 输出 CSV (全量注册表，用于 A/B 对比)
# ----------------------------------------------------------------------------
$csvHeader = "case_id,index,target_name,target_full,panel,filename,filepath,size_bytes,sha256,filter,exposure,date_obs"
$csvLines = @($csvHeader)
foreach ($e in $entries) {
    $line = "$($e.case_id),$($e.index),$($e.target_name),$($e.target_full),$($e.panel),$($e.filename),$($e.filepath),$($e.size_bytes),$($e.sha256),$($e.filter),$($e.exposure),$($e.date_obs)"
    $csvLines += $line
}
$csvLines | Out-File -FilePath $outCsv -Encoding utf8
Write-Host "CSV manifest 已写入: $outCsv"

# ----------------------------------------------------------------------------
# 摘要输出
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Manifest 摘要 ==="
Write-Host "总帧数: $($entries.Count)"
Write-Host "manifest_sha256: $manifestSha256"
Write-Host "目标天区分布:"
$byTarget.GetEnumerator() | Sort-Object Name | ForEach-Object { Write-Host ("  {0}: {1} frames" -f $_.Key, $_.Value) }
