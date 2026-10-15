param([string]$Exe, [string]$ArgList = "", [string]$Out, [int]$Wait = 1600, [string]$TypeText = "", [string]$Keys = "")
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W6 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
Add-Type -AssemblyName System.Drawing
[W6]::SetProcessDPIAware() | Out-Null
$p = if($ArgList -ne "") { Start-Process $Exe -ArgumentList $ArgList -PassThru } else { Start-Process $Exe -PassThru }
$sw = [Diagnostics.Stopwatch]::StartNew()
while(-not $p.HasExited -and $p.MainWindowHandle -eq 0 -and $sw.ElapsedMilliseconds -lt 10000) { Start-Sleep -Milliseconds 20; $p.Refresh() }
$h = $p.MainWindowHandle
Start-Sleep -Milliseconds $Wait
if($TypeText -ne "") { foreach($c in [char[]]$TypeText) { [W6]::PostMessage($h, 0x102, [IntPtr][int]$c, [IntPtr]1) | Out-Null; Start-Sleep -Milliseconds 30 }; Start-Sleep -Milliseconds 500 }
if($Keys -ne "") { foreach($k in $Keys.Split(",")) { $vk = [Convert]::ToInt32($k, 16); [W6]::PostMessage($h, 0x100, [IntPtr]$vk, [IntPtr]1) | Out-Null; Start-Sleep -Milliseconds 30; [W6]::PostMessage($h, 0x101, [IntPtr]$vk, [IntPtr]0xC0000001) | Out-Null; Start-Sleep -Milliseconds 250 }; Start-Sleep -Milliseconds 400 }
[W6]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x13) | Out-Null   # HWND_TOPMOST, no move/size/activate
[W6]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
$r = New-Object W6+RECT; [W6]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R-$r.L; $hh = $r.B-$r.T
# privacy guard: only capture if our window really is what is visible at the center and at 4 inner points
$ok = $true
foreach($pt in @(@(0.5,0.5), @(0.2,0.3), @(0.8,0.3), @(0.2,0.8), @(0.8,0.8))) {
  $q = New-Object W6+POINT; $q.X = $r.L + [int]($w*$pt[0]); $q.Y = $r.T + [int]($hh*$pt[1])
  if([W6]::GetAncestor([W6]::WindowFromPoint($q), 2) -ne $h) { $ok = $false }
}
if(-not $ok) { "NOT CAPTURED: another window covers the app window"; if(-not $p.HasExited) { $p.Kill() }; return }
$bmp = New-Object Drawing.Bitmap $w, $hh
$g = [Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object Drawing.Size $w, $hh))
$bmp.Save($Out)
$p.Refresh()
"exited=$($p.HasExited) responding=$(if(-not $p.HasExited){$p.Responding}) title='$(if(-not $p.HasExited){$p.MainWindowTitle})' ${w}x${hh}"
if(-not $p.HasExited) { $p.Kill() }

