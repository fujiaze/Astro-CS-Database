$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$p2 = Join-Path $root 'lib/phase2/build'
Write-Output '=== check exe machine + subsystem ===';
$exe = Join-Path $p2 'astrocs-stage2.exe'
$fs = [System.IO.File]::OpenRead($exe)
$br = New-Object System.IO.BinaryReader($fs)
$fs.Seek(0x3C, 'Begin') | Out-Null
$peOff = $br.ReadInt32()
$fs.Seek($peOff, 'Begin') | Out-Null
$sig = $br.ReadUInt32()
$machine = $br.ReadUInt16()
Write-Output ('PE sig ok=' + ($sig -eq 0x4550) + ' machine=0x' + $machine.ToString('X4'))
$fs.Close()
Write-Output '=== how was phase2 built on Windows? look for build script ===';
Get-ChildItem -LiteralPath (Join-Path $root 'lib/phase2') -Name | Select-Object -First 15
