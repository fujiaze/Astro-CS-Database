$ErrorActionPreference = 'Stop'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Set-Location -LiteralPath $root
$g = git rev-parse HEAD 2>$null; Write-Output ('HEAD=' + $g)
$b = git branch --show-current 2>$null; Write-Output ('BRANCH=' + $b)
foreach ($sha in @('b38b446e63d0d27eac672b85ce30527399a057fc','83471979a1dd778b4e557a9c7a92e22c137107f3')) {
  $c = git cat-file -t $sha 2>$null; Write-Output ($sha.Substring(0,7) + ' type=' + $c)
}
$m = git status --porcelain 2>$null | Select-Object -First 5; Write-Output 'porcelain:'; $m
