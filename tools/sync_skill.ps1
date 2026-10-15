# Refreshes skills/gui-foundations/assets/ from apps/ (the apps are the source of truth).
# Run after changing anything under apps/. The skill folder stays self-contained so it can be installed on its own.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$assets = Join-Path $repo 'skills\gui-foundations\assets'
if(Test-Path $assets) { Remove-Item $assets -Recurse -Force }
foreach($app in 'minimal', 'todo', 'fidget') {
  $src = Join-Path $repo "apps\$app"
  $dst = Join-Path $assets $app
  New-Item -ItemType Directory -Force -Path $dst | Out-Null
  Get-ChildItem $src -Force | Where-Object { $_.Name -ne 'build' } | ForEach-Object { Copy-Item $_.FullName (Join-Path $dst $_.Name) -Recurse -Force }
  "synced $app"
}
