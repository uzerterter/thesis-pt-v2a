# Setup Script for PTSL Integration v2 (using py-ptsl)
# ======================================================

Write-Host "Setting up PTSL Integration v2 with py-ptsl..." -ForegroundColor Cyan

# Navigate to companion directory
Set-Location -Path "$PSScriptRoot"

# Check if venv exists
if (Test-Path "venv_ptsl") {
    Write-Host "✓ Virtual environment already exists" -ForegroundColor Green
} else {
    Write-Host "Creating virtual environment..." -ForegroundColor Yellow
    python -m venv venv_ptsl
}

# Activate virtual environment
Write-Host "Activating virtual environment..." -ForegroundColor Yellow
& ".\venv_ptsl\Scripts\Activate.ps1"

# Upgrade pip
Write-Host "Upgrading pip..." -ForegroundColor Yellow
python -m pip install --upgrade pip

# Install base requirements
Write-Host "Installing base requirements..." -ForegroundColor Yellow
pip install -r requirements_v2.txt

# Install py-ptsl from local repository (editable mode)
Write-Host "Installing py-ptsl library..." -ForegroundColor Yellow
$pyptstl_path = "..\external\py-ptsl"
if (Test-Path $pyptstl_path) {
    Write-Host "  Installing from: $pyptstl_path" -ForegroundColor Gray
    pip install -e $pyptstl_path
    Write-Host "✓ py-ptsl installed successfully!" -ForegroundColor Green
} else {
    Write-Host "✗ ERROR: py-ptsl not found at $pyptstl_path" -ForegroundColor Red
    Write-Host "  Make sure external/py-ptsl directory exists (Git submodule)" -ForegroundColor Yellow
    exit 1
}

# Verify installation
Write-Host "`nVerifying installation..." -ForegroundColor Yellow
Write-Host "  Python version:" -ForegroundColor Gray
python --version

Write-Host "  Installed packages:" -ForegroundColor Gray
pip list | Select-String -Pattern "grpcio|protobuf|soundfile|ptsl"

# Test import
Write-Host "`nTesting imports..." -ForegroundColor Yellow
python -c "import ptsl; print('✓ py-ptsl import successful'); print(f'  Version: {ptsl.__version__}')"
python -c "import soundfile; print('✓ soundfile import successful')"
python -c "import grpc; print('✓ grpcio import successful')"

Write-Host "`n✅ Setup complete!" -ForegroundColor Green
Write-Host "`nTo use this environment:" -ForegroundColor Cyan
Write-Host "  1. Activate: .\venv_ptsl\Scripts\Activate.ps1" -ForegroundColor Gray
Write-Host "  2. Test: python ptsl_integration\ptsl_client_v2.py --audio <file.wav>" -ForegroundColor Gray
Write-Host "  3. In plugin: Use venv_ptsl\Scripts\python.exe" -ForegroundColor Gray
