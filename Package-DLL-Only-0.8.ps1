$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $MyInvocation.MyCommand.Path
$dll = Join-Path $project 'build\release\0_OStrands.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw "Build the 0.8.0 source first: $dll" }
$out = Join-Path $project 'OStrands-0.8.0-Creature-PPlus-Hybrid-DLL-ONLY-MO2.zip'
$stage = Join-Path $project 'build\package-0.8-stage'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
$dest = Join-Path $stage 'SKSE\Plugins'
New-Item -ItemType Directory -Path $dest -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $dest '0_OStrands.dll')
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }
Compress-Archive -LiteralPath (Join-Path $stage 'SKSE') -DestinationPath $out -CompressionLevel Optimal
Write-Host "Created: $out"
Write-Host 'Contents: SKSE/Plugins/0_OStrands.dll (no PEX, ESP or old meshes)'
