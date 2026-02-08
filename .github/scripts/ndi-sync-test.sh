#!/usr/bin/env bash
set -euo pipefail

# ============================================================================
# NDI Timestamp Sync Integration Test
#
# Tests that A/V sync latency remains persistent (within tolerance) across
# OBS restart cycles on a 3-machine NDI chain.
#
# Setup:
#   resolume.lan -> (NDI) -> strih.lan -> (NDI) -> stream.lan
#   Audio travels via hardware mixer (stable).
#   stream.lan runs obs-audio-video-sync-dock to measure A/V latency.
#
# The test reads sync-dock measurements from the OBS log file on stream.lan.
# Log format: [obs-audio-video-sync-dock] [sync-dock] latency=XX.X ms ...
# ============================================================================

# Configuration (from env or defaults)
STREAM_HOST="${STREAM_HOST:-stream.lan}"
STREAM_USER="${STREAM_USER:-newlevel}"
STRIH_HOST="${STRIH_HOST:-strih.lan}"
STRIH_USER="${STRIH_USER:-newlevel}"
STRIH_PASS="${STRIH_PASS:-newlevel}"
RESOLUME_HOST="${RESOLUME_HOST:-resolume.lan}"
RESOLUME_USER="${RESOLUME_USER:-newlevel}"
RESOLUME_PASS="${RESOLUME_PASS:-newlevel}"
RESTART_CYCLES="${RESTART_CYCLES:-10}"
TOLERANCE_MS="${TOLERANCE_MS:-1.0}"
STABILIZATION_SECS="${STABILIZATION_SECS:-30}"
RESTART_TARGET="${RESTART_TARGET:-all}"
RESULTS_DIR="${RESULTS_DIR:-$(pwd)/test-results}"

# OBS launch method: "task" uses Windows Scheduled Task, "direct" uses Start-Process
OBS_LAUNCH_METHOD="${OBS_LAUNCH_METHOD:-task}"
# Scheduled task names per machine (if using task method)
OBS_TASK_STREAM="${OBS_TASK_STREAM:-StartOBS}"
OBS_TASK_STRIH="${OBS_TASK_STRIH:-StartOBS}"
OBS_TASK_RESOLUME="${OBS_TASK_RESOLUME:-StartOBS}"

SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=10"

mkdir -p "$RESULTS_DIR"

# SSH helper functions
ssh_stream()   { ssh $SSH_OPTS "${STREAM_USER}@${STREAM_HOST}" "$@"; }
ssh_strih()    { sshpass -p "$STRIH_PASS" ssh $SSH_OPTS "${STRIH_USER}@${STRIH_HOST}" "$@"; }
ssh_resolume() { sshpass -p "$RESOLUME_PASS" ssh $SSH_OPTS "${RESOLUME_USER}@${RESOLUME_HOST}" "$@"; }

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"; }

# ============================================================================
# OBS Control Functions
# ============================================================================

stop_obs() {
    local ssh_func=$1
    local host_name=$2
    log "Stopping OBS on ${host_name}..."
    $ssh_func "taskkill /IM obs64.exe /F 2>nul & exit /b 0" 2>/dev/null || true
    sleep 3
}

start_obs() {
    local ssh_func=$1
    local host_name=$2
    local task_name=$3

    log "Starting OBS on ${host_name}..."
    if [ "$OBS_LAUNCH_METHOD" = "task" ]; then
        $ssh_func "schtasks /Run /TN \"${task_name}\" 2>nul & exit /b 0" 2>/dev/null || true
    else
        $ssh_func "powershell -Command \"Start-Process 'C:\\Program Files\\obs-studio\\bin\\64bit\\obs64.exe' -ArgumentList '--minimize-to-tray'\"" 2>/dev/null || true
    fi
    sleep 10
}

restart_obs() {
    local ssh_func=$1
    local host_name=$2
    local task_name=$3
    stop_obs "$ssh_func" "$host_name"
    start_obs "$ssh_func" "$host_name" "$task_name"
}

activate_sync_dock() {
    log "Activating sync-dock on stream.lan..."
    ssh_stream "schtasks /Run /TN ClickSyncDock 2>nul & exit /b 0" 2>/dev/null || true
    sleep 15
}

# ============================================================================
# Sync-Dock Measurement Collection
#
# Reads latency values from the OBS log on stream.lan.
# The sync-dock plugin writes lines like:
#   [obs-audio-video-sync-dock] [sync-dock] latency=27.9 ms  index=170 ...
# ============================================================================

get_latest_log_file() {
    ssh_stream "powershell -Command \"(Get-ChildItem '%APPDATA%\\obs-studio\\logs\\' | Sort-Object LastWriteTime -Descending | Select-Object -First 1).Name\"" 2>/dev/null | tr -d '\r\n'
}

collect_latency_samples() {
    local num_samples=${1:-10}
    local log_file
    log_file=$(get_latest_log_file)

    if [ -z "$log_file" ]; then
        echo "ERROR: Could not find OBS log file on stream.lan"
        return 1
    fi

    # Get the last N sync-dock latency lines from the log
    ssh_stream "powershell -Command \"Get-Content '%APPDATA%\\obs-studio\\logs\\${log_file}' | Select-String 'sync-dock.*latency=' | Select-Object -Last ${num_samples}\"" 2>/dev/null | \
        grep -oP 'latency=\K-?[0-9]+\.[0-9]+' || true
}

collect_dropped_frames() {
    local log_file
    log_file=$(get_latest_log_file)

    if [ -z "$log_file" ]; then
        echo "0"
        return
    fi

    # Check for dropped/lagged frames in the log
    local dropped
    dropped=$(ssh_stream "powershell -Command \"(Get-Content '%APPDATA%\\obs-studio\\logs\\${log_file}' | Select-String 'dropped|lagged').Count\"" 2>/dev/null | tr -d '\r\n')
    echo "${dropped:-0}"
}

# ============================================================================
# Test Execution
# ============================================================================

run_restart_test() {
    local machine_name=$1
    local ssh_func=$2
    local task_name=$3
    local results_file="${RESULTS_DIR}/results_${machine_name}.csv"

    echo "cycle,latency_avg_ms,latency_min_ms,latency_max_ms,latency_range_ms,status" > "$results_file"

    log "=============================="
    log "Testing restart cycles for: ${machine_name}"
    log "Cycles: ${RESTART_CYCLES}, Tolerance: ${TOLERANCE_MS}ms"
    log "=============================="

    local all_passed=true
    local baseline_latency=""

    for cycle in $(seq 1 "$RESTART_CYCLES"); do
        log "--- Cycle ${cycle}/${RESTART_CYCLES}: restarting ${machine_name} ---"

        restart_obs "$ssh_func" "$machine_name" "$task_name"

        # If we restarted stream.lan, re-activate the sync-dock
        if [ "$machine_name" = "stream.lan" ]; then
            activate_sync_dock
        fi

        # Wait for NDI chain to reconnect and sync-dock to stabilize
        log "Waiting ${STABILIZATION_SECS}s for sync stabilization..."
        sleep "$STABILIZATION_SECS"

        # Collect sync measurements, retry up to 3 times with extra waits
        log "Collecting latency samples..."
        local samples=""
        local retries=0
        while [ -z "$samples" ] && [ $retries -lt 3 ]; do
            samples=$(collect_latency_samples 20)
            if [ -z "$samples" ]; then
                retries=$((retries + 1))
                log "No measurements yet, waiting 15s more (retry ${retries}/3)..."
                sleep 15
            fi
        done

        if [ -z "$samples" ]; then
            log "ERROR: No sync-dock measurements found for cycle ${cycle}"
            echo "${cycle},N/A,N/A,N/A,N/A,ERROR" >> "$results_file"
            all_passed=false
            continue
        fi

        # Compute stats
        local stats
        stats=$(echo "$samples" | python3 -c "
import sys
vals = [float(line.strip()) for line in sys.stdin if line.strip()]
if not vals:
    print('N/A,N/A,N/A,N/A')
else:
    avg = sum(vals) / len(vals)
    mn = min(vals)
    mx = max(vals)
    rng = max(vals) - min(vals)
    print(f'{avg:.1f},{mn:.1f},{mx:.1f},{rng:.1f}')
")

        local avg_latency min_latency max_latency range_ms
        IFS=',' read -r avg_latency min_latency max_latency range_ms <<< "$stats"

        # Set baseline from first successful measurement
        if [ -z "$baseline_latency" ] && [ "$avg_latency" != "N/A" ]; then
            baseline_latency="$avg_latency"
            log "Baseline latency set to ${baseline_latency}ms"
        fi

        # Check tolerance against baseline
        local status="PASS"
        if [ "$avg_latency" != "N/A" ] && [ -n "$baseline_latency" ]; then
            status=$(python3 -c "
baseline = ${baseline_latency}
current = ${avg_latency}
tolerance = ${TOLERANCE_MS}
diff = abs(current - baseline)
print('PASS' if diff <= tolerance else 'FAIL')
")
        fi

        echo "${cycle},${avg_latency},${min_latency},${max_latency},${range_ms},${status}" >> "$results_file"
        log "Cycle ${cycle}: avg=${avg_latency}ms range=${range_ms}ms (${min_latency}-${max_latency}) ${status}"

        if [ "$status" = "FAIL" ]; then
            all_passed=false
            log "FAIL: Latency ${avg_latency}ms deviates from baseline ${baseline_latency}ms by more than ${TOLERANCE_MS}ms"
        fi
    done

    if $all_passed; then
        log "ALL ${RESTART_CYCLES} CYCLES PASSED for ${machine_name}"
        return 0
    else
        log "SOME CYCLES FAILED for ${machine_name}"
        return 1
    fi
}

# ============================================================================
# Main
# ============================================================================

log "============================================"
log "NDI Sync Integration Test"
log "============================================"
log "Restart target:   ${RESTART_TARGET}"
log "Cycles/machine:   ${RESTART_CYCLES}"
log "Tolerance:        ${TOLERANCE_MS}ms"
log "Stabilization:    ${STABILIZATION_SECS}s"
log "Results dir:      ${RESULTS_DIR}"
log "============================================"

# Verify connectivity
log "Verifying SSH connectivity..."
ssh_stream "echo ok" >/dev/null 2>&1 || { log "ERROR: Cannot connect to stream.lan"; exit 1; }
ssh_strih "echo ok" >/dev/null 2>&1 || { log "ERROR: Cannot connect to strih.lan"; exit 1; }
ssh_resolume "echo ok" >/dev/null 2>&1 || { log "ERROR: Cannot connect to resolume.lan"; exit 1; }
log "All machines reachable."

# Ensure all machines have OBS running for initial chain
log "Ensuring OBS is running on all machines..."
start_obs ssh_resolume "resolume.lan" "$OBS_TASK_RESOLUME"
start_obs ssh_strih "strih.lan" "$OBS_TASK_STRIH"
start_obs ssh_stream "stream.lan" "$OBS_TASK_STREAM"
activate_sync_dock
log "Waiting 30s for full NDI chain to establish..."
sleep 30

# Verify sync-dock is producing measurements (retry up to 5 times)
log "Verifying sync-dock is active..."
initial_samples=""
for attempt in $(seq 1 5); do
    initial_samples=$(collect_latency_samples 3)
    if [ -n "$initial_samples" ]; then
        break
    fi
    log "No sync-dock measurements yet, waiting 15s (attempt ${attempt}/5)..."
    sleep 15
done
if [ -z "$initial_samples" ]; then
    log "ERROR: No sync-dock measurements found after retries. Is obs-audio-video-sync-dock running on stream.lan?"
    exit 1
fi
log "Sync-dock active. Initial latency samples: $(echo "$initial_samples" | tr '\n' ' ')"

overall_pass=true

# Build machine list based on target
declare -A machine_ssh_funcs
declare -A machine_tasks
machine_ssh_funcs[stream.lan]=ssh_stream
machine_ssh_funcs[strih.lan]=ssh_strih
machine_ssh_funcs[resolume.lan]=ssh_resolume
machine_tasks[stream.lan]="$OBS_TASK_STREAM"
machine_tasks[strih.lan]="$OBS_TASK_STRIH"
machine_tasks[resolume.lan]="$OBS_TASK_RESOLUME"

if [ "$RESTART_TARGET" = "all" ]; then
    targets=("stream.lan" "strih.lan" "resolume.lan")
else
    targets=("$RESTART_TARGET")
fi

for machine in "${targets[@]}"; do
    if ! run_restart_test "$machine" "${machine_ssh_funcs[$machine]}" "${machine_tasks[$machine]}"; then
        overall_pass=false
    fi

    # Reset full chain between machine tests
    if [ "$RESTART_TARGET" = "all" ] && [ "$machine" != "${targets[-1]}" ]; then
        log "Resetting full chain between machine tests..."
        restart_obs ssh_resolume "resolume.lan" "$OBS_TASK_RESOLUME"
        restart_obs ssh_strih "strih.lan" "$OBS_TASK_STRIH"
        restart_obs ssh_stream "stream.lan" "$OBS_TASK_STREAM"
        activate_sync_dock
        sleep 30
    fi
done

# ============================================================================
# Summary Report
# ============================================================================
summary_file="${RESULTS_DIR}/summary.txt"
{
    echo "============================================"
    echo "NDI Sync Integration Test Report"
    echo "============================================"
    echo "Date:     $(date)"
    echo "Branch:   $(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo 'unknown')"
    echo "Commit:   $(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
    echo "Target:   ${RESTART_TARGET}"
    echo "Cycles:   ${RESTART_CYCLES}"
    echo "Tolerance: ${TOLERANCE_MS}ms"
    echo ""

    for csv in "${RESULTS_DIR}"/results_*.csv; do
        [ -f "$csv" ] || continue
        machine=$(basename "$csv" .csv | sed 's/results_//')
        echo "--- ${machine} ---"
        cat "$csv"
        echo ""

        python3 -c "
import csv
with open('${csv}') as f:
    rows = list(csv.DictReader(f))
latencies = [float(r['latency_avg_ms']) for r in rows if r['latency_avg_ms'] != 'N/A']
passed = sum(1 for r in rows if r['status'] == 'PASS')
failed = sum(1 for r in rows if r['status'] == 'FAIL')
errors = sum(1 for r in rows if r['status'] == 'ERROR')
if latencies:
    print(f'  Avg:   {sum(latencies)/len(latencies):.1f}ms')
    print(f'  Min:   {min(latencies):.1f}ms')
    print(f'  Max:   {max(latencies):.1f}ms')
    print(f'  Range: {max(latencies)-min(latencies):.1f}ms')
print(f'  Pass/Fail/Error: {passed}/{failed}/{errors}')
" 2>/dev/null || echo "  (stats unavailable)"
        echo ""
    done

    if $overall_pass; then
        echo "OVERALL: PASS"
    else
        echo "OVERALL: FAIL"
    fi
} | tee "$summary_file"

log "Results saved to ${RESULTS_DIR}/"

if $overall_pass; then
    log "TEST SUITE PASSED"
    exit 0
else
    log "TEST SUITE FAILED"
    exit 1
fi
