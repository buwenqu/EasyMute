# 生成 EasyMute 应用图标（多尺寸 PNG 压缩 ICO）
# 用法：& .\tools\make_icon.ps1
# 依赖：Windows PowerShell 5.1 自带的 System.Drawing

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$outPath = Join-Path $root 'res\app.ico'

$sizes = @(16, 20, 24, 32, 48, 64, 128, 256)
$pngs = @()

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    # 圆角矩形背景（蓝色渐变）
    $pad = [Math]::Max(0.5, $s * 0.015)
    $rect = New-Object System.Drawing.RectangleF([single]$pad, [single]$pad, [single]($s - 2 * $pad), [single]($s - 2 * $pad))
    $d = [single]([Math]::Max(2, $s * 0.30))
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($rect.X, $rect.Y, $d, $d, 180, 90)
    $path.AddArc($rect.Right - $d, $rect.Y, $d, $d, 270, 90)
    $path.AddArc($rect.Right - $d, $rect.Bottom - $d, $d, $d, 0, 90)
    $path.AddArc($rect.X, $rect.Bottom - $d, $d, $d, 90, 90)
    $path.CloseFigure()

    $c1 = [System.Drawing.Color]::FromArgb(255, 74, 140, 255)
    $c2 = [System.Drawing.Color]::FromArgb(255, 35, 84, 216)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $c1, $c2, [single]90)
    $g.FillPath($brush, $path)

    # 白色扬声器
    $speaker = New-Object System.Drawing.Drawing2D.GraphicsPath
    $pts = @(
        (New-Object System.Drawing.PointF([single]($s * 0.26), [single]($s * 0.38))),
        (New-Object System.Drawing.PointF([single]($s * 0.40), [single]($s * 0.38))),
        (New-Object System.Drawing.PointF([single]($s * 0.62), [single]($s * 0.20))),
        (New-Object System.Drawing.PointF([single]($s * 0.62), [single]($s * 0.80))),
        (New-Object System.Drawing.PointF([single]($s * 0.40), [single]($s * 0.62))),
        (New-Object System.Drawing.PointF([single]($s * 0.26), [single]($s * 0.62)))
    )
    $speaker.AddPolygon([System.Drawing.PointF[]]$pts)
    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillPath($white, $speaker)

    # 红色静音斜线
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 255, 77, 79), [single]([Math]::Max(1.2, $s * 0.11)))
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($pen, [single]($s * 0.50), [single]($s * 0.76), [single]($s * 0.84), [single]($s * 0.26))

    # 输出 PNG
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()

    $g.Dispose(); $bmp.Dispose(); $ms.Dispose()
    $brush.Dispose(); $white.Dispose(); $pen.Dispose(); $path.Dispose(); $speaker.Dispose()
}

# 组装 ICO 容器（ICONDIR + ICONDIRENTRY + PNG 数据）
$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($out)
$bw.Write([UInt16]0)
$bw.Write([UInt16]1)
$bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $data = $pngs[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([Byte]$dim)
    $bw.Write([Byte]$dim)
    $bw.Write([Byte]0)
    $bw.Write([Byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$data.Length)
    $bw.Write([UInt32]$offset)
    $offset += $data.Length
}
foreach ($data in $pngs) { $bw.Write($data) }
$bw.Flush()

$dir = Split-Path -Parent $outPath
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
[System.IO.File]::WriteAllBytes($outPath, $out.ToArray())
Write-Host "已生成: $outPath ($($out.Length) 字节)"
