# RUN_REAL_DATA.ps1
# Place this in your PROJECT ROOT (next to 'python' and 'data')
# Right-click -> "Run with PowerShell"

$ErrorActionPreference = "Stop"

# --- 1. Find Python ---
$python = $null
foreach ($cmd in @("python", "py", "python3")) {
    if (Get-Command $cmd -ErrorAction SilentlyContinue) {
        $python = $cmd
        break
    }
}
if (-not $python) {
    Write-Host "`nERROR: Python is not installed or not in PATH." -ForegroundColor Red
    Write-Host "1. https://www.python.org/downloads/" -ForegroundColor Yellow
    Write-Host "2. Install Python 3.11+ and CHECK 'Add Python to PATH'" -ForegroundColor Yellow
    pause
    exit 1
}
Write-Host "Found Python: $python" -ForegroundColor Green

# --- 2. Safety check ---
if (-not (Test-Path "python\download_data.py")) {
    Write-Host "`nERROR: Run this from your PROJECT ROOT (folder with 'python' and 'data')." -ForegroundColor Red
    pause
    exit 1
}

# --- 3. Ensure folders exist ---
$rawDir = "data\raw"
$procDir = "data\processed"
if (-not (Test-Path $rawDir)) { New-Item -ItemType Directory -Path $rawDir -Force | Out-Null }
if (-not (Test-Path $procDir)) { New-Item -ItemType Directory -Path $procDir -Force | Out-Null }

# --- 4. Write bbox.json WITHOUT BOM ---
$bboxText = @"
{
  "name": "Gulberg III / MM Alam Road corridor, Lahore",
  "latitude_min": 31.5100,
  "latitude_max": 31.5260,
  "longitude_min": 74.3420,
  "longitude_max": 74.3580
}
"@
$bboxBytes = [System.Text.Encoding]::UTF8.GetBytes($bboxText)
[System.IO.File]::WriteAllBytes("$rawDir\study_area_bbox.json", $bboxBytes)
Write-Host "Created: $rawDir\study_area_bbox.json (no BOM)" -ForegroundColor Green

# --- 5. Run pipeline ---
$scripts = @(
    "python\download_data.py",
    "python\preprocess_osm.py",
    "python\fetch_weather.py",
    "python\estimate_population.py"
)

foreach ($script in $scripts) {
    Write-Host "`n>>> Running $script ..." -ForegroundColor Cyan
    & $python $script
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nWARNING: $script exited with code $LASTEXITCODE" -ForegroundColor Yellow
    }
}

# --- 6. Verify ---
$buildings = "data\processed\buildings.json"
if (Test-Path $buildings) {
    $txt = Get-Content $buildings -Raw
    if ($txt -match '"data_source": "overpass_live"') {
        Write-Host "`nSUCCESS: Real Gulberg OSM data loaded!" -ForegroundColor Green
    } elseif ($txt -match '"data_source": "sample_fallback"') {
        Write-Host "`nNOTE: Still using sample data (Overpass download may have failed)." -ForegroundColor Yellow
        Write-Host "If you have no internet, use the Manual Fallback from the guide." -ForegroundColor Yellow
    }
}

Write-Host "`nDone. Run your C++ application now." -ForegroundColor Green
pause