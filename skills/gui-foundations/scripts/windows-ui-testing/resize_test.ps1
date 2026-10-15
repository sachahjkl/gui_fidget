param([string]$Exe, [string]$Out = "")
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W5 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
Add-Type -AssemblyName System.Drawing
[W5]::SetProcessDPIAware() | Out-Null
$p = Start-Process $Exe -PassThru
$sw = [Diagnostics.Stopwatch]::StartNew()
while(-not $p.HasExited -and $p.MainWindowHandle -eq 0 -and $sw.ElapsedMilliseconds -lt 10000) { Start-Sleep -Milliseconds 20; $p.Refresh() }
Start-Sleep -Milliseconds 800
$h = $p.MainWindowHandle
$r = New-Object W5+RECT; [W5]::GetWindowRect($h, [ref]$r) | Out-Null
$w0 = $r.R-$r.L; $h0 = $r.B-$r.T
# NOMOVE|NOZORDER|NOACTIVATE
$sizes = @(@(0.6,0.7),@(0.5,0.5),@(1.0,1.0),@(0.35,0.4),@(0.8,0.9),@(0.3,0.3),@(1.0,1.0))
$i = 0
foreach($s in $sizes) {
  [W5]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, [int]($w0*$s[0]), [int]($h0*$s[1]), 0x0016) | Out-Null
  Start-Sleep -Milliseconds 350
  $p.Refresh()
  "resize $i -> $([int]($w0*$s[0]))x$([int]($h0*$s[1])): exited=$($p.HasExited) responding=$(if(-not $p.HasExited){$p.Responding}) title='$(if(-not $p.HasExited){$p.MainWindowTitle})'"
  if($p.HasExited) { break }
  $i++
}
# rapid-fire resize burst (like dragging an edge)
if(-not $p.HasExited) {
  for($k=0; $k -lt 60 -and -not $p.HasExited; $k++) {
    $w = [int]($w0 * (0.4 + 0.6*[math]::Abs([math]::Sin($k/6.0)))); $hh = [int]($h0 * (0.5 + 0.5*[math]::Abs([math]::Cos($k/7.0))))
    [W5]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, $w, $hh, 0x0016) | Out-Null
    Start-Sleep -Milliseconds 15
  }
  Start-Sleep -Milliseconds 500; $p.Refresh()
  "burst done: exited=$($p.HasExited) responding=$(if(-not $p.HasExited){$p.Responding}) title='$(if(-not $p.HasExited){$p.MainWindowTitle})'"
}
if($Out -ne "" -and -not $p.HasExited) {
  [W5]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, [int]($w0*0.6), [int]($h0*0.62), 0x0016) | Out-Null; Start-Sleep -Milliseconds 700
  $rr = New-Object W5+RECT; [W5]::GetWindowRect($h, [ref]$rr) | Out-Null
  $w = $rr.R-$rr.L; $hh = $rr.B-$rr.T
  $bmp = New-Object Drawing.Bitmap $w, $hh; $g = [Drawing.Graphics]::FromImage($bmp)
  [W5]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 300
  $g.CopyFromScreen($rr.L, $rr.T, 0, 0, (New-Object Drawing.Size $w, $hh)); $bmp.Save($Out)
}
if(-not $p.HasExited) { $p.Kill() }
