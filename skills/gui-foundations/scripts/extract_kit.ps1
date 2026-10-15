# Copies the layers of the RAD Debugger codebase that form a standalone GPU UI kit.
# Usage: powershell -File extract_kit.ps1 -Upstream <path-to>\raddebugger\src -Dest C:\proj\kit [-IncludeMetagen]
# Directory structure is preserved because all #includes are relative to src/.
param(
  [Parameter(Mandatory=$true)][string]$Upstream,
  [Parameter(Mandatory=$true)][string]$Dest,
  [switch]$IncludeMetagen
)
$ErrorActionPreference = 'Stop'
if(-not (Test-Path (Join-Path $Upstream 'base\base_inc.h'))) { throw "Upstream '$Upstream' does not look like raddebugger's src/ (missing base\base_inc.h)" }

$dirs = @(
  'base', 'window_manager', 'shell', 'font_provider', 'render', 'font_cache', 'draw', 'ui',
  'win32\base', 'win32\shell', 'win32\window_manager',
  'linux\base', 'linux\shell', 'linux\window_manager',
  'lib_raddbg_markup',
  'third_party\stb', 'third_party\radsort', 'third_party\martins_hash', 'third_party\martins_memfun', 'third_party\xxHash'
)
if($IncludeMetagen) { $dirs += @('metagen', 'mdesk') }

# individual files at the root of win32/ and linux/ that the backends include
$files = @('win32\win32_inc.h','win32\win32_inc.c','win32\win32.h','win32\win32.c',
           'linux\linux_inc.h','linux\linux_inc.c','linux\linux.h','linux\linux.c')

New-Item -ItemType Directory -Force -Path $Dest | Out-Null
$count = 0
foreach($d in $dirs) {
  $s = Join-Path $Upstream $d
  if(-not (Test-Path $s)) { Write-Warning "missing $d (skipped)"; continue }
  $t = Join-Path $Dest $d
  New-Item -ItemType Directory -Force -Path (Split-Path $t -Parent) | Out-Null
  Copy-Item $s $t -Recurse -Force
  $count += (Get-ChildItem $t -Recurse -File).Count
}
foreach($f in $files) {
  $s = Join-Path $Upstream $f
  if(Test-Path $s) { $t = Join-Path $Dest $f; New-Item -ItemType Directory -Force -Path (Split-Path $t -Parent) | Out-Null; Copy-Item $s $t -Force; $count++ }
}
# provenance so you can diff against upstream later
$commit = ''
try { $commit = (git -C (Split-Path $Upstream -Parent) rev-parse HEAD 2>$null) } catch {}
"source: $Upstream`ncommit: $commit`nextracted: $(Get-Date -Format s)" | Set-Content (Join-Path $Dest 'KIT_COMMIT.txt')
"Copied $count files into $Dest (commit $commit)"
