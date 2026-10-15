param([string]$Exe, [string]$OutDir, [string]$Mode = "full")
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W7 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint f);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
"@
Add-Type -AssemblyName System.Drawing
[W7]::SetProcessDPIAware() | Out-Null
$orig = New-Object W7+POINT; [W7]::GetCursorPos([ref]$orig) | Out-Null
$p = Start-Process $Exe -ArgumentList "--title_state" -PassThru
$sw = [Diagnostics.Stopwatch]::StartNew()
while(-not $p.HasExited -and $p.MainWindowHandle -eq 0 -and $sw.ElapsedMilliseconds -lt 10000) { Start-Sleep -Milliseconds 20; $p.Refresh() }
$h = $p.MainWindowHandle
Start-Sleep -Milliseconds 1200
[W7]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x13) | Out-Null
[W7]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 500
$r = New-Object W7+RECT; [W7]::GetWindowRect($h, [ref]$r) | Out-Null
$f = ($r.R - $r.L) / 2000.0       # coordinates below were read from a 2000px-wide rendering of the window

function Title { $p.Refresh(); return ($p.MainWindowTitle -replace '^S ', '') }
function Ours($x, $y) {
  [W7]::SetCursorPos($x, $y) | Out-Null
  $pt = New-Object W7+POINT; $pt.X = $x; $pt.Y = $y
  return ([W7]::GetAncestor([W7]::WindowFromPoint($pt), 2) -eq $h)
}
function At($dx, $dy) { return @(($r.L + [int]($dx*$f)), ($r.T + [int]($dy*$f))) }
function Click($dx, $dy, $count = 1, $right = $false) {
  $a = At $dx $dy
  if(-not (Ours $a[0] $a[1])) { throw "pointer target is not our window at ($dx,$dy); aborting" }
  Start-Sleep -Milliseconds 220
  $d = if($right) { 8 } else { 2 }; $u = if($right) { 16 } else { 4 }
  for($i=0; $i -lt $count; $i++) { [W7]::mouse_event($d,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 50; [W7]::mouse_event($u,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80 }
  Start-Sleep -Milliseconds 450
}
function Wheel($dx, $dy, [int]$notches) {
  $a = At $dx $dy
  if(-not (Ours $a[0] $a[1])) { throw "pointer target is not our window (wheel); aborting" }
  Start-Sleep -Milliseconds 200
  $dir = 120; if($notches -lt 0) { $dir = -120 }
  $amt = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$dir), 0)
  for($i=0; $i -lt [math]::Abs($notches); $i++) { [W7]::mouse_event(0x800,0,0,$amt,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60 }
  Start-Sleep -Milliseconds 700
}
function Drag($x0, $y0, $x1, $y1, $steps = 14) {
  $a = At $x0 $y0; $b = At $x1 $y1
  if(-not (Ours $a[0] $a[1])) { throw "pointer target is not our window (drag); aborting" }
  Start-Sleep -Milliseconds 200
  [W7]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  for($i=1; $i -le $steps; $i++) { [W7]::SetCursorPos([int]($a[0] + ($b[0]-$a[0])*$i/$steps), [int]($a[1] + ($b[1]-$a[1])*$i/$steps)) | Out-Null; Start-Sleep -Milliseconds 16 }
  Start-Sleep -Milliseconds 60
  [W7]::mouse_event(4,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 450
}
function ArcFlick($cx, $cy, $rad) {   # fast circular motion around a center (fidget spinner)
  $a = At ($cx + $rad) $cy
  if(-not (Ours $a[0] $a[1])) { throw "pointer target is not our window (arc); aborting" }
  Start-Sleep -Milliseconds 200
  [W7]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  $c = At $cx $cy; $R = [int]($rad*$f)
  for($i=1; $i -le 16; $i++) { $t = $i * 0.18; [W7]::SetCursorPos([int]($c[0] + $R*[math]::Cos($t)), [int]($c[1] + $R*[math]::Sin($t))) | Out-Null; Start-Sleep -Milliseconds 12 }
  [W7]::mouse_event(4,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 150
}
function Key($vk) { [W7]::PostMessage($h, 0x100, [IntPtr]$vk, [IntPtr]1) | Out-Null; Start-Sleep -Milliseconds 30; [W7]::PostMessage($h, 0x101, [IntPtr]$vk, [IntPtr]0xC0000001) | Out-Null; Start-Sleep -Milliseconds 350 }
function TypeText($t) { foreach($c in [char[]]$t) { [W7]::PostMessage($h, 0x102, [IntPtr][int]$c, [IntPtr]1) | Out-Null; Start-Sleep -Milliseconds 30 }; Start-Sleep -Milliseconds 300 }
function Shot($name) {
  $rr = New-Object W7+RECT; [W7]::GetWindowRect($h, [ref]$rr) | Out-Null
  $w = $rr.R-$rr.L; $hh = $rr.B-$rr.T
  $bmp = New-Object Drawing.Bitmap $w, $hh; $g = [Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($rr.L, $rr.T, 0, 0, (New-Object Drawing.Size $w, $hh)); $bmp.Save("$OutDir\$name.png")
}
function Step($label) { "{0,-22} {1}" -f $label, (Title) }

try {
  if($Mode -eq "hold") {
    Wheel 500 700 -5;  Step "wheel x5 down"
    foreach($ms in 300,300,500,1000) { Start-Sleep -Milliseconds $ms; Step "  +$ms ms" }
    return
  }
  if($Mode -eq "sign") {
    Wheel 500 700 ([int]-1);  Step "one notch NEG (down)"
    Wheel 500 700 ([int]1);   Step "one notch POS (up)"
    Wheel 500 700 ([int]-3);  Step "three NEG"
    return
  }
  if($Mode -eq "wheel") {
    Step "start"
    Wheel 500 700 -5;  Step "wheel x5 down"
    Wheel 500 700 -5;  Step "wheel x5 down"
    Shot "t_scrolled"
    Wheel 500 700 12;  Step "wheel up x12"
    return
  }
  Step "start"
  Click 1400 912;            Step "slider -> ~23"
  Click 1100 381;            Step "switch bluetooth"
  Click 840 662;             Step "segmented Month"
  Click 1438 1228;           Step "rating 4"
  Click 1447 1163;           Step "stepper +"
  Click 1300 553;            Step "checkbox 2"
  Click 630 789;             Step "dropdown opened"
  Shot "t_dd_open"
  Click 630 1000;            Step "dropdown item (3rd?)"
  Wheel 500 700 -5;          Step "wheel x5 down"
  Shot "t_scrolled"
  Wheel 500 700 8;           Step "wheel back up"
  Click 592 154;             Step "tab Data"
  Click 705 329;             Step "sort Name asc"
  Click 705 329;             Step "sort Name desc"
  Click 800 490;             Step "select row 3"
  Key 0x28;                  Step "Down key"
  Key 0x28;                  Step "Down key"
  Shot "t_data"
  Click 827 154;             Step "tab Playground"
  Start-Sleep -Milliseconds 800
  Click 500 800;             Step "click in arena (spawn)"
  ArcFlick 1538 480 150;     Step "spinner flick (spin!=0)"
  Start-Sleep -Milliseconds 700
  Shot "t_play"
  Click 1570 85;             Step "palette button"
  TypeText "nord"
  Shot "t_pal"
  Key 0x0D;                  Step "Enter in palette"
  Click 545 85;              Step "Theme menu opened"
  Shot "t_menu"
  Key 0x1B;                  Step "Esc"
} catch { "ERROR: $_" }
finally { [W7]::SetCursorPos($orig.X, $orig.Y) | Out-Null; if(-not $p.HasExited) { $p.Kill() } }




