Add-Type -AssemblyName System.Drawing

function RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-LogoPng([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $green = [System.Drawing.Color]::FromArgb(255, 15, 122, 77)

    # lingkaran hijau penuh
    $br = New-Object System.Drawing.SolidBrush($green)
    $g.FillEllipse($br, 0.5, 0.5, $s - 1.0, $s - 1.0)
    $br.Dispose()

    # koma putih dari outline glyph Segoe UI Bold, diskalakan & dipusatkan
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $fam = New-Object System.Drawing.FontFamily('Georgia')
    $path.AddString(',', $fam, [int][System.Drawing.FontStyle]::Bold, 100,
                    (New-Object System.Drawing.PointF(0, 0)),
                    [System.Drawing.StringFormat]::GenericDefault)
    $b = $path.GetBounds()
    $targetH = $s * 0.52
    $scale = $targetH / $b.Height
    $m = New-Object System.Drawing.Drawing2D.Matrix
    $m.Scale($scale, $scale)
    $path.Transform($m)
    $b = $path.GetBounds()
    $m2 = New-Object System.Drawing.Drawing2D.Matrix
    $m2.Translate($s / 2 - ($b.X + $b.Width / 2), $s / 2 - ($b.Y + $b.Height / 2))
    $path.Transform($m2)
    $br = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillPath($br, $path)
    $br.Dispose(); $path.Dispose(); $fam.Dispose(); $m.Dispose(); $m2.Dispose()

    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output -NoEnumerate ([byte[]]$ms.ToArray())
}

$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = New-Object System.Collections.ArrayList
foreach ($s in $sizes) { [void]$pngs.Add([byte[]](New-LogoPng $s)) }

$out = 'C:\laragon\www\commander\src\app.ico'
$fs = [System.IO.File]::Create($out)
$bw = New-Object System.IO.BinaryWriter($fs)
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $bw.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))   # width
    $bw.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))   # height
    $bw.Write([Byte]0); $bw.Write([Byte]0)                     # colors, reserved
    $bw.Write([UInt16]1); $bw.Write([UInt16]32)                # planes, bpp
    $bw.Write([UInt32]$pngs[$i].Length)
    $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Close(); $fs.Close()
Write-Output "ICO ditulis: $out ($((Get-Item $out).Length) bytes)"

# preview PNG besar untuk dicek visual
[System.IO.File]::WriteAllBytes('C:\Users\Sarta\AppData\Local\Temp\claude\C--laragon-www-commander\ad101b90-2bb0-4a29-8497-e5ef76233bec\scratchpad\logo256.png', (New-LogoPng 256))
[System.IO.File]::WriteAllBytes('C:\Users\Sarta\AppData\Local\Temp\claude\C--laragon-www-commander\ad101b90-2bb0-4a29-8497-e5ef76233bec\scratchpad\logo16.png', (New-LogoPng 16))
