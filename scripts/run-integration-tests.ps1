# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
# Run the integration suite in tests/integration against a built plugin.
#
#   DISTROAV_OBS_BINARY   the obs64.exe to run: the one of a portable install
#                         made for testing, as its folder is where the suite's
#                         profile goes
#   DISTROAV_PLUGIN_DIR   folder holding distroav.dll and distroav\locale
#                         (default: a build_*\rundir\<config> of this checkout)
#   NDI_RUNTIME_DIR_V6    folder holding Processing.NDI.Lib.x64.dll, if the NDI
#                         runtime is not installed
#   DISTROAV_PYTHON       the python to make the virtualenv with (default: python)
#   DISTROAV_IT_VENV      where to keep the virtualenv
#
# Arguments go to pytest. See tests/integration/README.md.
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$python = if ($env:DISTROAV_PYTHON) { $env:DISTROAV_PYTHON } else { 'python' }
$venv = if ($env:DISTROAV_IT_VENV) { $env:DISTROAV_IT_VENV } else { Join-Path $env:TEMP 'distroav-integration-venv' }
$venvPython = Join-Path $venv 'Scripts\python.exe'

if (-not (Test-Path $venvPython)) {
    & $python -m venv $venv
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $venvPython -m pip install --quiet -r (Join-Path $repo 'tests\integration\requirements.txt')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Set-Location $repo
& $venvPython -m pytest tests/integration @args
exit $LASTEXITCODE
