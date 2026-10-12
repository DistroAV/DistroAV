#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
# Run the integration suite in tests/integration against a built plugin.
#
#   DISTROAV_OBS_BINARY  the obs to run (default: obs on the PATH)
#   DISTROAV_PLUGIN_DIR  folder holding distroav.so and distroav/locale
#                        (default: a build_*/rundir/<config> of this checkout)
#   DISTROAV_IT_VENV     where to keep the virtualenv
#
# Arguments go to pytest. See tests/integration/README.md.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(dirname "${here}")"
venv="${DISTROAV_IT_VENV:-${TMPDIR:-/tmp}/distroav-integration-venv}"

if [[ ! -x "${venv}/bin/python" ]]; then
	python3 -m venv "${venv}"
fi
"${venv}/bin/pip" install --quiet -r "${repo}/tests/integration/requirements.txt"

cd "${repo}"
exec "${venv}/bin/python" -m pytest tests/integration "$@"
