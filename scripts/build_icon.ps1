Add-Type -AssemblyName System.Drawing

$srcPath = "C:\Users\yalsh\.gemini\antigravity-ide\brain\637867f2-3052-4387-823e-f38f4b493a20\voidfall_logo_1791615221929.jpg"
$destPng = "d:\Projects\voxel_3d_voidfall_dredge\assets\icon.png"
$destIco = "d:\Projects\voxel_3d_voidfall_dredge\assets\icon.ico"
$headerPath = "d:\Projects\voxel_3d_voidfall_dredge\src\core\app_icon_data.hpp"

$src = [System.Drawing.Bitmap]::new($srcPath)

# 1. Crop badge region (96, 96, 832, 832)
$cropRect = [System.Drawing.Rectangle]::new(96, 96, 832, 832)
$cropped = [System.Drawing.Bitmap]::new(832, 832, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($cropped)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($src, [System.Drawing.Rectangle]::new(0, 0, 832, 832), $cropRect, [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()

# 2. Apply rounded rectangle alpha mask
# Radius ~ 160 on 832x832
$path = [System.Drawing.Drawing2D.GraphicsPath]::new()
$r = 160
$d = $r * 2
$w = 832
$h = 832
$path.AddArc(0, 0, $d, $d, 180, 90)
$path.AddArc($w - $d, 0, $d, $d, 270, 90)
$path.AddArc($w - $d, $h - $d, $d, $d, 0, 90)
$path.AddArc(0, $h - $d, $d, $d, 90, 90)
$path.CloseFigure()

$masked = [System.Drawing.Bitmap]::new(832, 832, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gm = [System.Drawing.Graphics]::FromImage($masked)
$gm.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gm.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gm.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gm.Clear([System.Drawing.Color]::Transparent)
$gm.SetClip($path)
$gm.DrawImage($cropped, 0, 0)
$gm.ResetClip()
$gm.Dispose()
$cropped.Dispose()
$src.Dispose()

# Save master 256x256 PNG
$sizes = @(256, 128, 64, 48, 32, 16)
$bitmaps = @{}

foreach ($s in $sizes) {
    $bmp = [System.Drawing.Bitmap]::new($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gb = [System.Drawing.Graphics]::FromImage($bmp)
    $gb.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gb.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $gb.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gb.DrawImage($masked, [System.Drawing.Rectangle]::new(0, 0, $s, $s))
    $gb.Dispose()
    $bitmaps[$s] = $bmp
}

$bitmaps[256].Save($destPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Saved master PNG to $destPng"

# Save ICO file containing all sizes
# ICO format structure:
# Header: 2 bytes (0), 2 bytes (1), 2 bytes (count)
# Directory entries: 16 bytes each
# Data: PNG bytes for 256, BMP DIB for 128, 64, 48, 32, 16
$icoMs = [System.IO.MemoryStream]::new()
$bw = [System.IO.BinaryWriter]::new($icoMs)

$count = $sizes.Count
$bw.Write([uint16]0) # Reserved
$bw.Write([uint16]1) # Icon type
$bw.Write([uint16]$count)

# Prepare image data streams
$dataStreams = @()
foreach ($s in $sizes) {
    $bmp = $bitmaps[$s]
    $imgMs = [System.IO.MemoryStream]::new()
    if ($s -eq 256) {
        # PNG format for 256x256 (standard Vista/Win7/8/10/11)
        $bmp.Save($imgMs, [System.Drawing.Imaging.ImageFormat]::Png)
    } else {
        # Standard BMP DIB for 128, 64, 48, 32, 16 for universal compatibility
        $dibBw = [System.IO.BinaryWriter]::new($imgMs)
        # BITMAPINFOHEADER (40 bytes)
        $dibBw.Write([uint32]40) # biSize
        $dibBw.Write([int32]$s)   # biWidth
        $dibBw.Write([int32]($s * 2)) # biHeight (doubled for XOR + AND mask)
        $dibBw.Write([uint16]1)  # biPlanes
        $dibBw.Write([uint16]32) # biBitCount (32-bit BGRA)
        $dibBw.Write([uint32]0)  # biCompression (BI_RGB)
        $dibBw.Write([uint32]($s * $s * 4)) # biSizeImage
        $dibBw.Write([int32]0)   # biXPelsPerMeter
        $dibBw.Write([int32]0)   # biYPelsPerMeter
        $dibBw.Write([uint32]0)  # biClrUsed
        $dibBw.Write([uint32]0)  # biClrImportant

        # Pixel data: bottom-to-top rows of BGRA
        for ($y = $s - 1; $y -ge 0; $y--) {
            for ($x = 0; $x -lt $s; $x++) {
                $c = $bmp.GetPixel($x, $y)
                $dibBw.Write([byte]$c.B)
                $dibBw.Write([byte]$c.G)
                $dibBw.Write([byte]$c.R)
                $dibBw.Write([byte]$c.A)
            }
        }

        # 1-bit AND mask: bottom-to-top (all 0 since alpha mask is used)
        $rowBytes = [int][System.Math]::Ceiling($s / 32.0) * 4
        $maskRow = New-Object byte[] $rowBytes
        for ($y = 0; $y -lt $s; $y++) {
            $dibBw.Write($maskRow, 0, $rowBytes)
        }
    }
    $dataStreams += $imgMs
}

# Calculate offsets
$headerSize = 6 + ($count * 16)
$currentOffset = $headerSize
$offsets = @()
for ($i = 0; $i -lt $count; $i++) {
    $offsets += $currentOffset
    $currentOffset += $dataStreams[$i].Length
}

# Write directory entries
for ($i = 0; $i -lt $count; $i++) {
    $s = $sizes[$i]
    $bw.Write([byte]($s -band 255)) # width (0 for 256)
    $bw.Write([byte]($s -band 255)) # height (0 for 256)
    $bw.Write([byte]0)              # colors (0 for 32bpp)
    $bw.Write([byte]0)              # reserved
    $bw.Write([uint16]1)            # planes
    $bw.Write([uint16]32)           # bitCount
    $bw.Write([uint32]$dataStreams[$i].Length) # size
    $bw.Write([uint32]$offsets[$i])            # offset
}

# Write image data
for ($i = 0; $i -lt $count; $i++) {
    $bytes = $dataStreams[$i].ToArray()
    $bw.Write($bytes, 0, $bytes.Length)
    $dataStreams[$i].Dispose()
}

[System.IO.File]::WriteAllBytes($destIco, $icoMs.ToArray())
$bw.Dispose()
$icoMs.Dispose()
Write-Host "Saved multi-resolution ICO to $destIco"

# Generate C++ header with embedded RGBA pixel data for GLFW
# Export 16x16, 32x32, 48x48
$sb = [System.Text.StringBuilder]::new()
$null = $sb.AppendLine("// Auto-generated Voidfall: Dredge Application Icon Data")
$null = $sb.AppendLine("#pragma once")
$null = $sb.AppendLine("#include <cstdint>")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("namespace Voidfall {")
$null = $sb.AppendLine("namespace IconData {")
$null = $sb.AppendLine("")

$exportSizes = @(16, 32, 48, 64)
foreach ($s in $exportSizes) {
    $bmp = $bitmaps[$s]
    $null = $sb.AppendLine("inline constexpr int ICON_${s}_WIDTH = $s;")
    $null = $sb.AppendLine("inline constexpr int ICON_${s}_HEIGHT = $s;")
    $null = $sb.AppendLine("inline const uint8_t ICON_${s}_RGBA[] = {")
    
    $line = [System.Text.StringBuilder]::new("    ")
    $pixelCount = 0
    for ($y = 0; $y -lt $s; $y++) {
        for ($x = 0; $x -lt $s; $x++) {
            $c = $bmp.GetPixel($x, $y)
            $null = $line.Append(("0x{0:X2},0x{1:X2},0x{2:X2},0x{3:X2}," -f @($c.R, $c.G, $c.B, $c.A)))
            $pixelCount++
            if ($pixelCount % 4 -eq 0) {
                $null = $sb.AppendLine($line.ToString())
                $line.Clear()
                $null = $line.Append("    ")
            }
        }
    }
    if ($line.Length -gt 4) {
        $null = $sb.AppendLine($line.ToString())
    }
    $null = $sb.AppendLine("};")
    $null = $sb.AppendLine("")
}

$null = $sb.AppendLine("} // namespace IconData")
$null = $sb.AppendLine("} // namespace Voidfall")

[System.IO.File]::WriteAllText($headerPath, $sb.ToString())
Write-Host "Generated C++ icon header: $headerPath"

# Clean up
foreach ($s in $sizes) {
    $bitmaps[$s].Dispose()
}
$masked.Dispose()
