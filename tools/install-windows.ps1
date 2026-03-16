# Define source and destination paths
$sourceDir = "./release/RelWithDebInfo/distroav/"
$destDir = "C:\ProgramData\obs-studio\plugins\distroav"

# Resolve repo root based on script location
$repoRoot = Join-Path $PSScriptRoot ".." | Resolve-Path -ErrorAction Stop
$sourceDir = Join-Path $repoRoot "release/RelWithDebInfo/distroav/"

# Validate that source directory exists
if (-not (Test-Path $sourceDir)) {
    Write-Error "Source directory '$sourceDir' does not exist. Aborting."
    exit 1
}

# Create destination directory if it doesn't exist
if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}
# Remove existing files and directories in destination
if (Test-Path $destDir) {
    Remove-Item -Path "$destDir\*" -Recurse -Force -Confirm:$false
}
# Copy files from source to destination, overwriting existing files
Copy-Item -Path "$sourceDir*" -Destination $destDir -Recurse -Force -Confirm:$false

Write-Host "Files copied successfully from $sourceDir to $destDir (existing files overwritten)"