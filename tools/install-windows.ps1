# Ensure this script runs elevated (Administrator).
$ErrorActionPreference = "Stop"
Write-Host "[Step 1/8] Starting DistroAV Windows install script"

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)

if (-not $isAdmin) {
    $scriptPath = if ($PSCommandPath) { $PSCommandPath } else { $MyInvocation.MyCommand.Path }
    Write-Host "[Step 2/8] Not running as Administrator. Requesting elevation..."

    $argumentList = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$scriptPath`""
    ) + $args

    try {
        Write-Host "[Step 3/8] Launching elevated PowerShell process"
        $process = Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $argumentList -Wait -PassThru
        Write-Host "[Step 4/8] Elevated process finished with exit code $($process.ExitCode)"
        exit $process.ExitCode
    }
    catch {
        Write-Error "Failed to start elevated install process: $($_.Exception.Message)"
        exit 1
    }
}
else {
    Write-Host "[Step 2/8] Running as Administrator"
}

try {
    Write-Host "[Step 3/8] Preparing source and destination paths"
    # Define source and destination paths
    $sourceDir = "./release/RelWithDebInfo/distroav/"
    $destDir = "C:\ProgramData\obs-studio\plugins\distroav"

    Write-Host "[Step 4/8] Resolving repository root"
    # Resolve repo root based on script location
    $repoRoot = Join-Path $PSScriptRoot ".." | Resolve-Path -ErrorAction Stop
    $sourceDir = Join-Path $repoRoot "release/RelWithDebInfo/distroav/"
    Write-Host "  Source: $sourceDir"
    Write-Host "  Destination: $destDir"

    Write-Host "[Step 5/8] Validating source directory exists"
    # Validate that source directory exists
    if (-not (Test-Path $sourceDir)) {
        throw "Source directory '$sourceDir' does not exist."
    }

    Write-Host "[Step 6/8] Ensuring destination directory exists"
    # Create destination directory if it doesn't exist
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        Write-Host "  Created destination directory"
    }
    else {
        Write-Host "  Destination directory already exists"
    }

    Write-Host "[Step 7/8] Cleaning destination directory"
    # Remove existing files and directories in destination
    if (Test-Path $destDir) {
        Remove-Item -Path "$destDir\*" -Recurse -Force -Confirm:$false
    }

    Write-Host "[Step 8/8] Copying build output to destination"
    # Copy files from source to destination, overwriting existing files
    Copy-Item -Path "$sourceDir*" -Destination $destDir -Recurse -Force -Confirm:$false

    Write-Host "Install completed successfully"
    Write-Host "Files copied successfully from $sourceDir to $destDir (existing files overwritten)"
    exit 0
}
catch {
    Write-Error "Install failed: $($_.Exception.Message)"
    exit 1
}