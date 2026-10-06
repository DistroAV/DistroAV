<#
.SYNOPSIS
  Installs clang-format (if needed) and runs it on new or modified C/C++ source files.

.DESCRIPTION
  Detects clang-format 19.x on PATH, or via the Python "clang-format" pip package
  (installing it with pip if neither is found), then formats -- or checks the
  formatting of -- the set of new/modified source files (*.c, *.h, *.cpp, *.hpp,
  *.m, *.mm), using this repo's .clang-format style.

.PARAMETER Base
  Git ref to diff against (e.g. origin/master). When omitted, the script formats
  uncommitted working-tree changes instead (staged, unstaged, and untracked files).

.PARAMETER Staged
  Only consider staged files (git diff --cached). Ignored if -Base is given.

.PARAMETER Check
  Check formatting only; do not modify files. Exits non-zero if any file would change.

.EXAMPLE
  .github/scripts/run-clang-format.ps1
  Formats all locally changed/new source files in place.

.EXAMPLE
  .github/scripts/run-clang-format.ps1 -Base origin/master -Check
  Checks formatting of every file changed on this branch relative to origin/master.
#>
param(
    [string]$Base,
    [switch]$Staged,
    [switch]$Check
)

# Kept at the default 'Continue' (rather than 'Stop') on purpose: native tools here
# (pip, clang-format) write routine warnings/diagnostics to stderr, and under PS7
# 'Stop' turns those into terminating errors. Exit codes are checked explicitly
# via $LASTEXITCODE instead.
$ErrorActionPreference = 'Continue'
$PSNativeCommandUseErrorActionPreference = $false

$RequiredMajor = 19
$Extensions = @('c', 'h', 'cpp', 'hpp', 'm', 'mm')

$RepoRoot = git rev-parse --show-toplevel 2>$null
if (-not $RepoRoot) {
    Write-Error "Not inside a git repository."
    exit 1
}
Set-Location $RepoRoot

function Test-EndsWithNewline {
    param([string]$Path)
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) { return $true }
    return $bytes[-1] -eq 0x0A
}

function Add-TrailingNewline {
    param([string]$Path)
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) { return }

    # Match the file's existing line-ending style (CRLF vs LF); default to LF.
    $hasCRLF = $false
    for ($i = 1; $i -lt $bytes.Length; $i++) {
        if ($bytes[$i - 1] -eq 0x0D -and $bytes[$i] -eq 0x0A) { $hasCRLF = $true; break }
    }
    $newlineBytes = if ($hasCRLF) { [byte[]](0x0D, 0x0A) } else { [byte[]](0x0A) }
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write)
    try {
        $stream.Write($newlineBytes, 0, $newlineBytes.Length)
    } finally {
        $stream.Close()
    }
}

function Get-ClangFormatVersion {
    param([string]$Exe)
    try {
        $output = & $Exe --version 2>$null
    } catch {
        return $null
    }
    if ($LASTEXITCODE -ne 0) { return $null }
    if ($output -match '(\d+)\.(\d+)\.(\d+)') {
        return [version]"$($Matches[1]).$($Matches[2]).$($Matches[3])"
    }
    return $null
}

function Find-ClangFormat {
    foreach ($name in @('clang-format-19', 'clang-format')) {
        $found = Get-Command $name -ErrorAction SilentlyContinue
        if ($found) {
            $version = Get-ClangFormatVersion -Exe $found.Source
            if ($version -and $version.Major -eq $RequiredMajor) {
                return $found.Source
            }
        }
    }

    # The pip "clang-format" package installs a clang-format(.exe) console script,
    # which on Windows --user installs often isn't on PATH. Look there directly.
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
    if ($python) {
        $probe = "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user')); print(sysconfig.get_path('scripts'))"
        $scriptDirs = & $python.Source -c $probe 2>$null
        $exeName = if ($env:OS -eq 'Windows_NT') { 'clang-format.exe' } else { 'clang-format' }
        foreach ($dir in ($scriptDirs -split "`r?`n" | Where-Object { $_ })) {
            $candidate = Join-Path $dir $exeName
            if (Test-Path $candidate -PathType Leaf) {
                $version = Get-ClangFormatVersion -Exe $candidate
                if ($version -and $version.Major -eq $RequiredMajor) {
                    return $candidate
                }
            }
        }
    }

    return $null
}

function Install-ClangFormat {
    Write-Host "clang-format $RequiredMajor.x not found; installing via pip..." -ForegroundColor Yellow

    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
    if (-not $python) {
        Write-Error "Python (with pip) is required to auto-install clang-format. Install Python from https://python.org, or install LLVM (which bundles clang-format) manually from https://github.com/llvm/llvm-project/releases, then re-run this script."
        exit 1
    }

    & $python.Source -m pip install --user --upgrade "clang-format~=$RequiredMajor.1"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "pip install of clang-format failed."
        exit 1
    }
}

$clangFormat = Find-ClangFormat
if (-not $clangFormat) {
    Install-ClangFormat
    $clangFormat = Find-ClangFormat
    if (-not $clangFormat) {
        Write-Error "clang-format $RequiredMajor.x still not available after installation attempt."
        exit 1
    }
}
Write-Host "Using clang-format: $clangFormat" -ForegroundColor Cyan

if ($Base) {
    $changed = git diff --name-only --diff-filter=ACMR "$Base...HEAD"
} elseif ($Staged) {
    $changed = git diff --name-only --cached --diff-filter=ACMR
} else {
    $changed = @(git diff --name-only --diff-filter=ACMR HEAD) + @(git ls-files --others --exclude-standard)
}

$files = @($changed |
    Where-Object { $_ } |
    Sort-Object -Unique |
    Where-Object {
        $ext = [System.IO.Path]::GetExtension($_).TrimStart('.')
        $Extensions -contains $ext -and (Test-Path $_ -PathType Leaf)
    })

if ($files.Count -eq 0) {
    Write-Host "No new or modified C/C++ source files to format."
    exit 0
}

Write-Host "Files to format:" -ForegroundColor Cyan
$files | ForEach-Object { Write-Host "  $_" }

if ($Check) {
    $failed = @()
    foreach ($file in $files) {
        & $clangFormat -style=file --dry-run --Werror $file 2>$null
        if ($LASTEXITCODE -ne 0 -or -not (Test-EndsWithNewline $file)) { $failed += $file }
    }
    if ($failed) {
        Write-Host "The following files need formatting:" -ForegroundColor Red
        $failed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        exit 1
    }
    Write-Host "All files are properly formatted." -ForegroundColor Green
} else {
    & $clangFormat -style=file -i @files
    if ($LASTEXITCODE -ne 0) {
        Write-Error "clang-format failed."
        exit 1
    }

    $fixedEof = @($files | Where-Object { -not (Test-EndsWithNewline $_) })
    foreach ($file in $fixedEof) { Add-TrailingNewline $file }
    if ($fixedEof) {
        Write-Host "Added missing trailing newline to:" -ForegroundColor Cyan
        $fixedEof | ForEach-Object { Write-Host "  $_" }
    }

    Write-Host "Formatted $($files.Count) file(s)." -ForegroundColor Green
}
