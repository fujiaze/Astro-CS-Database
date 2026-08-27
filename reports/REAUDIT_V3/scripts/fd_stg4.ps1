$ErrorActionPreference = 'Continue'
Write-Output '=== msys64 mingw64 bin has the runtime dlls? ===';
$mg = 'C:/msys64/mingw64/bin'
foreach ($d in @('libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')) {
  $p = Join-Path $mg $d; Write-Output ($d + ': ' + (Test-Path -LiteralPath $p))
}
$root = 'F:/Astro dev/Astro CS Normalization Database'
$p2 = Join-Path $root 'lib/phase2/build'
$env:PATH = 'C:/msys64/mingw64/bin;' + $p2 + ';' + $env:PATH
Set-Location -LiteralPath $p2
Write-Output '=== stage2 no-args with mingw64 on PATH ===';
& (Join-Path $p2 'astrocs-stage2.exe') 2>&1 | Select-Object -First 6
Write-Output ('EXIT=' + $LASTEXITCODE)
