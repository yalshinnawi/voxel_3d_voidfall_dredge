# Validate markdown links within wiki/
param (
    [string]$WikiPath = "wiki"
)

$Root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
if (-not (Test-Path "$Root\$WikiPath")) {
    $Root = (Get-Location).Path
}

$FullWikiPath = "$Root\$WikiPath"
if (-not (Test-Path $FullWikiPath)) {
    Write-Host "[-] Wiki directory not found at $FullWikiPath" -ForegroundColor Red
    exit 1
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Validating Markdown Links in $WikiPath" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$Files = Get-ChildItem -Path $FullWikiPath -Filter "*.md" -Recurse
$BrokenLinks = 0
$CheckedLinks = 0

foreach ($File in $Files) {
    $Content = Get-Content -Path $File.FullName -Raw
    $MatchesList = [regex]::Matches($Content, '\[([^\]]+)\]\(([^)]+)\)')

    foreach ($M in $MatchesList) {
        $Link = $M.Groups[2].Value
        # Skip external web URLs
        if ($Link -match '^https?://') {
            continue
        }

        $CheckedLinks++
        # Strip anchors
        $CleanLink = $Link.Split('#')[0]
        if ([string]::IsNullOrWhiteSpace($CleanLink)) {
            continue
        }

        # Handle file:/// URIs
        if ($CleanLink -match '^file:///(.+)$') {
            $Target = $Matches[1] -replace '/', '\'
        } elseif ([System.IO.Path]::IsPathRooted($CleanLink)) {
            $Target = $CleanLink
        } else {
            $Target = Join-Path -Path $File.DirectoryName -ChildPath $CleanLink
        }

        if (-not (Test-Path $Target)) {
            Write-Host "[-] Broken link in $($File.Name): '$Link' -> target does not exist" -ForegroundColor Red
            $BrokenLinks++
        }
    }
}

Write-Host "------------------------------------------" -ForegroundColor Cyan
Write-Host "Checked $CheckedLinks links. $BrokenLinks broken links found." -ForegroundColor $(if ($BrokenLinks -eq 0) { "Green" } else { "Red" })
Write-Host "==========================================" -ForegroundColor Cyan

exit $BrokenLinks
