$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== what DLLs does astrocs-stage2.exe import? ===';
# crude: scan the PE import table via strings of dll names
$exe = Join-Path $root 'lib/phase2/build/astrocs-stage2.exe'
$bytes = [System.IO.File]::ReadAllBytes($exe)
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
foreach ($name in @('astro_image_io.dll','acr_cuda_bridge.dll','libwinpthread-1.dll','libstdc++-6.dll','libgcc_s_seh-1.dll','zlibd.dll','zlib1.dll','cfitsio.dll','libcurl.dll','libtbb12.dll','tbb12.dll','cudart64_110.dll')) {
  if ($text.Contains($name)) { Write-Output ('NEEDS ' + $name) }
}
Write-Output '=== where do those live on F:? ===';
Get-ChildItem -LiteralPath (Join-Path $root 'lib') -Recurse -Include libwinpthread-1.dll,libstdc++-6.dll,libgcc_s_seh-1.dll -ErrorAction SilentlyContinue | Select-Object -First 10 -ExpandProperty FullName
