# Draws the gui_fidget logo (512x512 PNG): a white fidget spinner (hub, three arms, three lobes with bearing holes)
# on a near-black rounded square. Then tools/make_icon.ps1 turns it into the exe icon.
#   powershell -File tools/make_logo.ps1 [-Out .project/image.png]
param([string]$Out = (Join-Path (Split-Path $PSScriptRoot -Parent) '.project\image.png'))
Add-Type -AssemblyName System.Drawing
$S = 4                      # supersampling factor (drawn at 2048 px, downscaled to 512)
$N = 512 * $S
$bmp = New-Object Drawing.Bitmap $N, $N
$g = [Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.Clear([Drawing.Color]::Transparent)
$black = [Drawing.Color]::FromArgb(255, 18, 18, 18)
$white = [Drawing.Color]::White

function RoundRect($x, $y, $w, $h, $r) {
  $p = New-Object Drawing.Drawing2D.GraphicsPath
  $d = 2 * $r
  $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
  $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90); $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
  $p.CloseFigure(); return $p
}
function Disc($cx, $cy, $r, $color) {
  $b = New-Object Drawing.SolidBrush $color
  $g.FillEllipse($b, [single](($cx - $r) * $S), [single](($cy - $r) * $S), [single](2 * $r * $S), [single](2 * $r * $S)); $b.Dispose()
}

# frame: rounded square, 400 px, centered (56 px transparent margin, which make_icon.ps1 crops)
$g.FillPath((New-Object Drawing.SolidBrush $black), (RoundRect (56 * $S) (56 * $S) (400 * $S) (400 * $S) (100 * $S)))

# fidget spinner: hub + 3 arms + 3 lobes, white, with dark bearing holes
$cx = 256; $cy = 262; $dist = 108; $lobe = 58; $hub = 56; $arm = 40
$pts = @()
foreach($deg in -90, 30, 150) { $a = $deg * [math]::PI / 180; $pts += , @(($cx + $dist * [math]::Cos($a)), ($cy + $dist * [math]::Sin($a))) }
$pen = New-Object Drawing.Pen $white, ([single](2 * $arm * $S)); $pen.StartCap = 'Round'; $pen.EndCap = 'Round'
foreach($p in $pts) { $g.DrawLine($pen, [single]($cx * $S), [single]($cy * $S), [single]($p[0] * $S), [single]($p[1] * $S)) }
Disc $cx $cy $hub $white
foreach($p in $pts) { Disc $p[0] $p[1] $lobe $white }
foreach($p in $pts) { Disc $p[0] $p[1] 28 $black }
Disc $cx $cy 26 $black
Disc $cx $cy 11 $white

# downsample to 512x512
$o = New-Object Drawing.Bitmap 512, 512
$g2 = [Drawing.Graphics]::FromImage($o)
$g2.InterpolationMode = 'HighQualityBicubic'; $g2.SmoothingMode = 'AntiAlias'; $g2.PixelOffsetMode = 'HighQuality'
$g2.DrawImage($bmp, 0, 0, 512, 512)
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$o.Save($Out, [Drawing.Imaging.ImageFormat]::Png)
"wrote $Out"
