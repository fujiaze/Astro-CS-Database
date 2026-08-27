$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
"--- git ---"
$g = Get-Command git -ErrorAction SilentlyContinue
if ($g) { "git=$($g.Source)"; & git --version 2>&1 } else { "git=NOT_FOUND" }
"--- cmake ---"
$c = Get-Command cmake -ErrorAction SilentlyContinue
if ($c) { "cmake=$($c.Source)"; & cmake --version 2>&1 | Select-Object -First 1 } else { "cmake=NOT_FOUND" }
"--- repo .git ---"
$gitdir = Join-Path $root '.git'
"repo_git_exists=$(Test-Path -LiteralPath $gitdir)"
if (Test-Path -LiteralPath $gitdir) { Push-Location $root; & git rev-parse HEAD 2>&1; & git status --porcelain 2>&1 | Select-Object -First 5; Pop-Location }
"--- worktrees ---"
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
"wt_dir_exists=$(Test-Path -LiteralPath $wt)"
if (Test-Path -LiteralPath $wt) { Get-ChildItem -LiteralPath $wt -Directory | ForEach-Object { "wt_child=$($_.Name) git=$((Test-Path -LiteralPath (Join-Path $_.FullName '.git'))) $(if (Test-Path -LiteralPath (Join-Path $_.FullName '.git')) { 'has_worktree' } else { '' })" } }
"--- existing build dirs ---"
Get-ChildItem -LiteralPath $root -Directory -Filter 'build*' | ForEach-Object { "build_dir=$($_.Name)" }
"--- anchors exist in repo? ---"
if (Test-Path -LiteralPath $gitdir) { Push-Location $root; foreach ($sha in @('b38b446e63d0d27eac672b85ce30527399a057fc','83471979a1dd778b4e557a9c7a92e22c137107f3','535e73879662346ee1f599d7a9cae96c6c23680d')) { & git cat-file -t $sha 2>&1 | ForEach-Object { "anchor $($sha.Substring(0,7)) type=$_" } }; Pop-Location }
PROBE_DONE
