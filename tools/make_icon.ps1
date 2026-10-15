# Builds apps/fidget/data/icon.ico (PNG-compressed, 16..256 px) from .project/image.png.
# The .project image has transparent padding around the rounded square; crop it so the icon fills its frame.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repo = Split-Path $PSScriptRoot -Parent
$src = [Drawing.Image]::FromFile((Join-Path $repo '.project\image.png'))
$crop = New-Object Drawing.Rectangle 56, 56, 400, 400
$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach($s in $sizes) {
  $bmp = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'AntiAlias'; $g.PixelOffsetMode = 'HighQuality'
  $g.Clear([Drawing.Color]::Transparent)
  $g.DrawImage($src, (New-Object Drawing.Rectangle 0, 0, $s, $s), $crop, [Drawing.GraphicsUnit]::Pixel)
  $ms = New-Object IO.MemoryStream; $bmp.Save($ms, [Drawing.Imaging.ImageFormat]::Png); $pngs += , $ms.ToArray()
  $g.Dispose(); $bmp.Dispose()
}
$src.Dispose()
$out = New-Object IO.MemoryStream
$w = New-Object IO.BinaryWriter $out
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for($i = 0; $i -lt $sizes.Count; $i++) {
  $s = $sizes[$i]; $dim = if($s -ge 256) { 0 } else { $s }
  $w.Write([byte]$dim); $w.Write([byte]$dim); $w.Write([byte]0); $w.Write([byte]0)
  $w.Write([uint16]1); $w.Write([uint16]32); $w.Write([uint32]$pngs[$i].Length); $w.Write([uint32]$offset)
  $offset += $pngs[$i].Length
}
foreach($p in $pngs) { $w.Write($p) }
$dst = Join-Path $repo 'apps\fidget\data\icon.ico'
[IO.File]::WriteAllBytes($dst, $out.ToArray())
"wrote $dst ($([int]((Get-Item $dst).Length / 1KB)) KB)"
