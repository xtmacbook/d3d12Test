<#
Convert project text files to UTF-8 (with BOM) and back up originals.

Usage:
  - Review the script first.
  - Run in repository root in PowerShell (as you):
      .\scripts\convert_to_utf8.ps1 -WhatIf
    to preview changes, or
      .\scripts\convert_to_utf8.ps1
    to perform conversion.

This script targets common source/text file extensions. It will skip files
that already start with a UTF-8 BOM. Originals are copied to a backup folder
named `.utf8_backup_YYYYMMDD_HHMMSS`.
#>

param(
    [switch]$WhatIf,
    [string[]]$Include = @("*.cpp","*.c","*.h","*.hpp","*.in","CMakeLists.txt","*.cmake","*.txt","*.md","*.json","*.sln","*.vcxproj","*.props","*.rc","*.py","*.cs","*.yaml","*.yml","*.xml","*.ini")
)

function Write-Log { param($m) Write-Host "[convert_to_utf8] $m" }

$start = Get-Date
$backupRoot = Join-Path (Get-Location) (".utf8_backup_{0:yyyyMMdd_HHmmss}" -f $start)

Write-Log "Scanning files..."
$files = Get-ChildItem -Path (Get-Location) -Recurse -File -ErrorAction SilentlyContinue | Where-Object { foreach ($p in $Include) { if ($_.Name -like $p -or $_.FullName -like "*\$p") { return $true } }; return $false }

if (-not $files) { Write-Log "No matching files found."; return }

Write-Log "Found $($files.Count) files to check. Backup root: $backupRoot"

foreach ($f in $files) {
    $path = $f.FullName
    $bytes = [System.IO.File]::ReadAllBytes($path)

    # check for UTF-8 BOM (EF BB BF)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        Write-Log "SKIP (already UTF-8 BOM): $path"
        continue
    }

    # create backup copy
    $rel = $path.Substring((Get-Location).Path.Length).TrimStart('\','/')
    $bakPath = Join-Path $backupRoot $rel
    if (-not $WhatIf) {
        New-Item -ItemType Directory -Path (Split-Path $bakPath) -Force | Out-Null
        Copy-Item -Path $path -Destination $bakPath -Force
    }

    Write-Log "Converting: $path -> UTF-8"

    if ($WhatIf) { continue }

    try {
        # read with default system encoding (assume ANSI) and write UTF8 with BOM
        $content = Get-Content -Path $path -Raw -ErrorAction Stop
        # Use Set-Content with -Encoding UTF8 (Windows PowerShell writes BOM)
        Set-Content -Path $path -Value $content -Encoding UTF8 -Force
    }
    catch {
        Write-Log "ERROR converting $path : $_"
    }
}

$end = Get-Date
Write-Log "Done. Backup of originals (if changed) is in: $backupRoot"
Write-Log "Elapsed: $($end - $start)"
