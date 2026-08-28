# FIX_AND_RUN.ps1
# PUT THIS FILE IN YOUR PROJECT ROOT (the folder containing 'python' and 'data')
# Right-click -> "Run with PowerShell"

$projectRoot = $PSScriptRoot
if (-not $projectRoot) { $projectRoot = (Get-Location).Path }

Write-Host "Project detected: $projectRoot" -ForegroundColor Cyan

# 1. Create folders explicitly
$rawDir    = Join-Path $projectRoot "data\raw"
$procDir   = Join-Path $projectRoot "data\processed"
New-Item -ItemType Directory -Path $rawDir  -Force | Out-Null
New-Item -ItemType Directory -Path $procDir -Force | Out-Null
Write-Host "Folders created." -ForegroundColor Green

# 2. Write study_area_bbox.json with NO BOM using absolute path
$bboxText = @"
{
  "name": "Gulberg III / MM Alam Road corridor, Lahore",
  "latitude_min": 31.5100,
  "latitude_max": 31.5260,
  "longitude_min": 74.3420,
  "longitude_max": 74.3580
}
"@
$bboxPath = Join-Path $rawDir "study_area_bbox.json"
[System.IO.File]::WriteAllText($bboxPath, $bboxText, [System.Text.Encoding]::UTF8)
Write-Host "Created: $bboxPath" -ForegroundColor Green

# 3. Find Python
$python = $null
foreach ($cmd in @("python", "py", "python3")) {
    if (Get-Command $cmd -ErrorAction SilentlyContinue) {
        $python = $cmd
        break
    }
}
if (-not $python) {
    Write-Host "ERROR: Python not found. Install from python.org and check 'Add to PATH'." -ForegroundColor Red
    pause
    exit 1
}
Write-Host "Found Python: $python" -ForegroundColor Green

# 4. Run the four data scripts using full paths
$scripts = @(
    (Join-Path $projectRoot "python\download_data.py"),
    (Join-Path $projectRoot "python\preprocess_osm.py"),
    (Join-Path $projectRoot "python\fetch_weather.py"),
    (Join-Path $projectRoot "python\estimate_population.py")
)

foreach ($script in $scripts) {
    Write-Host "`n>>> Running $script ..." -ForegroundColor Cyan
    & $python $script
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: $script exited with code $LASTEXITCODE" -ForegroundColor Yellow
    }
}

# 5. Quick check
$buildings = Join-Path $procDir "buildings.json"
if (Test-Path $buildings) {
    $txt = Get-Content $buildings -Raw
    if ($txt -match '"data_source": "overpass_live"') {
        Write-Host "`nSUCCESS! Real Gulberg data is loaded." -ForegroundColor Green
    } elseif ($txt -match '"data_source": "sample_fallback"') {
        Write-Host "`nNOTE: Using sample fallback (internet/Overpass API may be blocked)." -ForegroundColor Yellow
    }
}

Write-Host "`nAll done. You can now run your C++ application." -ForegroundColor Green
pause