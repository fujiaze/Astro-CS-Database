$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
$outdir = Join-Path $root 'run/temp/p2_v7/abc_32r'
New-Item -ItemType Directory -Force -Path $outdir | Out-Null
$env:Path = 'C:/msys64/mingw64/bin;C:/msys64/usr/bin;' + $env:Path
$anchors = @(
  @{name='A'; sha='b38b446e63d0d27eac672b85ce30527399a057fc'},
  @{name='B'; sha='83471979a1dd778b4e557a9c7a92e22c137107f3'}
)
foreach ($a in $anchors) {
  $wd = Join-Path $wt $a.name
  $src = Join-Path $wd 'lib/phase2'
  $log = Join-Path $outdir ('build_' + $a.name + '.log')
  $bd = Join-Path $wd 'build_ab'
  "=== BUILD $($a.name) @ $($a.sha) ===" | Tee-Object -FilePath $log
  "cmake -S $src -B $bd" | Tee-Object -FilePath $log -Append
  Push-Location $src
  try {
    & cmake -S $src -B $bd -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=mingw32-make 2>&1 | Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -eq 0) {
      & cmake --build $bd --target astrocs-stage2 -j 4 2>&1 | Tee-Object -FilePath $log -Append
    }
  } finally { Pop-Location }
  $exe = Join-Path $bd 'astrocs-stage2.exe'
  if (-not (Test-Path -LiteralPath $exe)) {
    $alt = Get-ChildItem -Path $bd -Recurse -Filter 'astrocs-stage2.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($alt) { $exe = $alt.FullName }
  }
  "stage2_$($a.name)=$(Test-Path -LiteralPath $exe) $(if (Test-Path -LiteralPath $exe) { $exe } else { '' })" | Tee-Object -FilePath $log -Append
}
"BUILD_ALL_DONE"
