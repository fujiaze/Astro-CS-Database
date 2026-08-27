$ErrorActionPreference = "Continue"
foreach ($p in @("F:\Astro dev\Astro CS Normalization Database\GaiaDR3", "F:\Astro dev\Astro CS Normalization Database\GaiaDR3SP")) {
  if (Test-Path -LiteralPath $p) {
    $files = Get-ChildItem -LiteralPath $p -Recurse -File -ErrorAction SilentlyContinue
    $sum = ($files | Measure-Object Length -Sum).Sum
    Write-Host ("DIR " + $p + " items=" + $files.Count + " sizeMB=" + [math]::Round($sum/1MB,1))
  } else { Write-Host ("MISSING " + $p) }
}
