$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
New-Item -ItemType Directory -Force -Path $wt | Out-Null
$anchors = @(
  @{name='A'; sha='b38b446e63d0d27eac672b85ce30527399a057fc'},
  @{name='B'; sha='83471979a1dd778b4e557a9c7a92e22c137107f3'}
)
Push-Location $root
foreach ($a in $anchors) {
  $wd = Join-Path $wt $a.name
  if (Test-Path -LiteralPath (Join-Path $wd '.git')) {
    "WORKTREE_EXISTS $($a.name) sha=$( (git -C $wd rev-parse HEAD) )"
  } else {
    "CREATING $($a.name) @ $($a.sha)..."
    & git worktree add $wd $a.sha 2>&1
    if ($LASTEXITCODE -eq 0) { "WORKTREE_OK $($a.name) sha=$( (git -C $wd rev-parse HEAD) )" } else { "WORKTREE_FAIL $($a.name)" }
  }
}
"--- worktree list ---"
& git worktree list 2>&1
Pop-Location
