# Generator icon CSV Commander.
# app.ico     : koma putih dalam lingkaran hijau (icon aplikasi)
# csvfile.ico : dokumen dengan koma hijau + band CSV (icon asosiasi file .csv)
# Semua ukuran dirender 4x lalu di-downscale (supersampling) agar tajam di ukuran kecil.
Add-Type -AssemblyName System.Drawing

$green   = [System.Drawing.Color]::FromArgb(255, 15, 122, 77)
$pageBrd = [System.Drawing.Color]::FromArgb(255, 189, 200, 193)
$foldClr = [System.Drawing.Color]::FromArgb(255, 214, 226, 219)

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

# path teks dari glyph font, tinggi target $th, dipusatkan di ($cx,$cy)
function GlyphPath([string]$txt, [string]$fontName, [float]$th, [float]$cx, [float]$cy) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $fam = New-Object System.Drawing.FontFamily($fontName)
    $path.AddString($txt, $fam, [int][System.Drawing.FontStyle]::Bold, 100,
                    (New-Object System.Drawing.PointF(0, 0)),
                    [System.Drawing.StringFormat]::GenericDefault)
    $b = $path.GetBounds()
    $scale = $th / $b.Height
    $m = New-Object System.Drawing.Drawing2D.Matrix
    $m.Scale($scale, $scale)
    $path.Transform($m)
    $b = $path.GetBounds()
    $m2 = New-Object System.Drawing.Drawing2D.Matrix
    $m2.Translate($cx - ($b.X + $b.Width / 2), $cy - ($b.Y + $b.Height / 2))
    $path.Transform($m2)
    $fam.Dispose(); $m.Dispose(); $m2.Dispose()
    return $path
}

function DrawApp([System.Drawing.Graphics]$g, [float]$s) {
    $br = New-Object System.Drawing.SolidBrush($script:green)
    $g.FillEllipse($br, $s * 0.01, $s * 0.01, $s * 0.98, $s * 0.98)
    $br.Dispose()
    $path = GlyphPath ',' 'Georgia' ($s * 0.52) ($s / 2) ($s / 2)
    $br = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillPath($br, $path)
    $br.Dispose(); $path.Dispose()
}

function DrawCsvFile([System.Drawing.Graphics]$g, [float]$s) {
    # halaman dokumen
    $px = $s * 0.16; $py = $s * 0.03
    $pw = $s * 0.68; $ph = $s * 0.94
    $page = RoundedPath $px $py $pw $ph ($s * 0.05)
    $br = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillPath($br, $page); $br.Dispose()
    $pen = New-Object System.Drawing.Pen($script:pageBrd, [Math]::Max(1.0, $s * 0.02))
    $g.DrawPath($pen, $page); $pen.Dispose()
    # lipatan sudut kanan atas
    $f = $s * 0.20
    $fold = New-Object System.Drawing.Drawing2D.GraphicsPath
    $fold.AddPolygon(@(
        (New-Object System.Drawing.PointF(($px + $pw - $f), $py)),
        (New-Object System.Drawing.PointF(($px + $pw), ($py + $f))),
        (New-Object System.Drawing.PointF(($px + $pw - $f), ($py + $f)))
    ))
    $br = New-Object System.Drawing.SolidBrush($script:foldClr)
    $g.FillPath($br, $fold); $br.Dispose(); $fold.Dispose()
    # koma hijau
    $path = GlyphPath ',' 'Georgia' ($s * 0.34) ($s * 0.5) ($s * 0.36)
    $br = New-Object System.Drawing.SolidBrush($script:green)
    $g.FillPath($br, $path)
    $br.Dispose(); $path.Dispose()
    # band "CSV"
    $bx = $s * 0.10; $by = $s * 0.60
    $bw = $s * 0.80; $bh = $s * 0.24
    $band = RoundedPath $bx $by $bw $bh ($s * 0.05)
    $br = New-Object System.Drawing.SolidBrush($script:green)
    $g.FillPath($br, $band); $br.Dispose(); $band.Dispose()
    $tp = GlyphPath 'CSV' 'Segoe UI' ($bh * 0.52) ($bx + $bw / 2) ($by + $bh / 2)
    $br = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillPath($br, $tp)
    $br.Dispose(); $tp.Dispose()
    $page.Dispose()
}

# render fungsi gambar di kanvas 4x, downscale ke $s → PNG bytes
function RenderPng([int]$s, [scriptblock]$draw) {
    $big = $s * 4
    $bmp = New-Object System.Drawing.Bitmap($big, $big)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    & $draw $g ([float]$big)
    $g.Dispose()
    $small = New-Object System.Drawing.Bitmap($s, $s)
    $g2 = [System.Drawing.Graphics]::FromImage($small)
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g2.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g2.DrawImage($bmp, 0, 0, $s, $s)
    $g2.Dispose(); $bmp.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $small.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $small.Dispose()
    Write-Output -NoEnumerate ([byte[]]$ms.ToArray())
}

function WriteIco([string]$out, [System.Collections.ArrayList]$pngs, [int[]]$sizes) {
    $fs = [System.IO.File]::Create($out)
    $bw = New-Object System.IO.BinaryWriter($fs)
    $bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $s = $sizes[$i]
        $bw.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))
        $bw.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))
        $bw.Write([Byte]0); $bw.Write([Byte]0)
        $bw.Write([UInt16]1); $bw.Write([UInt16]32)
        $bw.Write([UInt32]$pngs[$i].Length)
        $bw.Write([UInt32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($p in $pngs) { $bw.Write($p) }
    $bw.Close(); $fs.Close()
    Write-Output "ICO ditulis: $out ($((Get-Item $out).Length) bytes)"
}

$sizes = 16, 20, 24, 32, 48, 64, 128, 256
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path

$pngs = New-Object System.Collections.ArrayList
foreach ($s in $sizes) { [void]$pngs.Add([byte[]](RenderPng $s ${function:DrawApp})) }
WriteIco (Join-Path $dir 'app.ico') $pngs $sizes

$pngs = New-Object System.Collections.ArrayList
foreach ($s in $sizes) { [void]$pngs.Add([byte[]](RenderPng $s ${function:DrawCsvFile})) }
WriteIco (Join-Path $dir 'csvfile.ico') $pngs $sizes

# preview untuk pemeriksaan visual
$prev = New-Object System.Drawing.Bitmap(560, 300)
$g = [System.Drawing.Graphics]::FromImage($prev)
$g.Clear([System.Drawing.Color]::FromArgb(255, 240, 240, 240))
$x = 10
foreach ($s in @(16, 32, 64, 128)) {
    foreach ($fn in @(${function:DrawApp}, ${function:DrawCsvFile})) {
        $png = RenderPng $s $fn
        $ms = New-Object System.IO.MemoryStream(, [byte[]]$png)
        $img = [System.Drawing.Image]::FromStream($ms)
        $g.DrawImage($img, $x, $(if ($fn -eq ${function:DrawApp}) { 20 } else { 160 }), $s, $s)
        $img.Dispose(); $ms.Dispose()
    }
    $x += $s + 14
}
$g.Dispose()
$prev.Save((Join-Path $env:TEMP 'icon_preview.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$prev.Dispose()
Write-Output "Preview: $env:TEMP\icon_preview.png"
