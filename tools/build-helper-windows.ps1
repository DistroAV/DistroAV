# Build Helper Script for Windows
# This script runs the main Windows build process and handles prompts as needed

param(
    [switch]$Force
)

$buildScriptPath = Join-Path -Path $PSScriptRoot -ChildPath "../.github/scripts/Build-Windows.ps1"

if (-not (Test-Path $buildScriptPath)) {
    Write-Error "Build script not found at: $buildScriptPath"
    exit 1
}

try {
    Write-Host "Starting Windows build process..." -ForegroundColor Cyan

    # Ensure required CI-related environment variables are set for local runs
    if (-not $env:CI) {
        $env:CI = "true"
    }
    if (-not $env:GITHUB_EVENT_NAME) {
        $env:GITHUB_EVENT_NAME = "workflow_dispatch"
    }

    & $buildScriptPath @PSBoundParameters
    
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Build script exited with code: $LASTEXITCODE"
        $prompt = Read-Host "Do you want to continue? (Y/N)"
        if ($prompt -ne 'Y' -and $prompt -ne 'y') {
            exit $LASTEXITCODE
        }
    }
    
    Write-Host "Build process completed successfully!" -ForegroundColor Green
}
catch {
    Write-Error "An error occurred: $_"
    exit 1
}