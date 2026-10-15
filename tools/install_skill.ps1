# Installs the skill into your agent skills folder (default: ~/.agents/skills). Re-run to update.
#   powershell -File tools/install_skill.ps1 [-Dest C:\Users\me\.agents\skills]
param([string]$Dest = (Join-Path $HOME '.agents\skills'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'sync_skill.ps1') | Out-Null
$target = Join-Path $Dest 'gui-foundations'
if(Test-Path $target) { Remove-Item $target -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
Copy-Item (Join-Path $repo 'skills\gui-foundations') $target -Recurse -Force
"installed -> $target"
