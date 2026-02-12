#!/bin/bash
# OBS Restart A/V Sync Stability Test
# Tests that A/V sync remains within tolerance across OBS restarts
# Usage: ./obs-restart-sync-test.sh [cycles] [max_drift_ms]

CYCLES=${1:-30}
MAX_DRIFT_MS=${2:-2}
SETTLE_TIME=12  # Seconds to wait for sync to stabilize after restart

results=()
timestamps=()

echo "=========================================="
echo "OBS Restart A/V Sync Stability Test"
echo "=========================================="
echo "Cycles: $CYCLES"
echo "Max allowed drift: ${MAX_DRIFT_MS}ms"
echo "Settle time: ${SETTLE_TIME}s"
echo ""

get_av_sync() {
    # Get the latest av= value from distroav SYNC log line
    ssh stream.lan 'type "%APPDATA%\obs-studio\logs\*.txt" 2>nul | findstr /C:"[distroav] SYNC:"' 2>/dev/null | \
        tail -1 | grep -oP 'av=[-0-9.]+' | grep -oP '[-0-9.]+'
}

for i in $(seq 1 $CYCLES); do
    echo "--- Cycle $i/$CYCLES ---"

    # Kill OBS
    echo "  Stopping OBS..."
    ssh stream.lan 'taskkill /IM obs64.exe /F' 2>/dev/null || true
    sleep 2

    # Start OBS
    echo "  Starting OBS..."
    ssh stream.lan 'schtasks /Run /TN "StartOBS"'

    # Wait for OBS to start and sync to stabilize
    echo "  Waiting ${SETTLE_TIME}s for sync to stabilize..."
    sleep $SETTLE_TIME

    # Get A/V sync value
    av_sync=$(get_av_sync)

    if [ -n "$av_sync" ]; then
        results+=("$av_sync")
        timestamps+=("$(date +%H:%M:%S)")
        echo "  A/V Sync: ${av_sync}ms"
    else
        echo "  WARNING: No sync reading available"
        results+=("NaN")
        timestamps+=("$(date +%H:%M:%S)")
    fi
done

echo ""
echo "=========================================="
echo "TEST RESULTS"
echo "=========================================="

# Filter out NaN values for statistics
valid_results=()
for r in "${results[@]}"; do
    if [ "$r" != "NaN" ]; then
        valid_results+=("$r")
    fi
done

if [ ${#valid_results[@]} -lt 2 ]; then
    echo "ERROR: Not enough valid readings to analyze"
    exit 1
fi

# Calculate statistics
min=$(printf '%s\n' "${valid_results[@]}" | sort -n | head -1)
max=$(printf '%s\n' "${valid_results[@]}" | sort -n | tail -1)
range=$(echo "$max - ($min)" | bc -l | awk '{printf "%.1f", $1}')
mean=$(printf '%s\n' "${valid_results[@]}" | awk '{sum+=$1} END {printf "%.2f", sum/NR}')

# Calculate max drift between consecutive readings
max_drift=0
prev=""
for r in "${valid_results[@]}"; do
    if [ -n "$prev" ]; then
        drift=$(echo "$r - ($prev)" | bc -l | awk '{printf "%.2f", ($1<0)?-$1:$1}')
        if (( $(echo "$drift > $max_drift" | bc -l) )); then
            max_drift=$drift
        fi
    fi
    prev=$r
done

echo ""
echo "Individual readings:"
for i in "${!results[@]}"; do
    echo "  Cycle $((i+1)): ${results[$i]}ms (${timestamps[$i]})"
done

echo ""
echo "Statistics:"
echo "  Valid readings: ${#valid_results[@]}/${#results[@]}"
echo "  Min: ${min}ms"
echo "  Max: ${max}ms"
echo "  Range: ${range}ms"
echo "  Mean: ${mean}ms"
echo "  Max consecutive drift: ${max_drift}ms"

echo ""
echo "=========================================="
# Pass/fail check
if (( $(echo "$max_drift <= $MAX_DRIFT_MS" | bc -l) )); then
    echo "RESULT: PASS"
    echo "Max drift (${max_drift}ms) is within tolerance (${MAX_DRIFT_MS}ms)"
    exit 0
else
    echo "RESULT: FAIL"
    echo "Max drift (${max_drift}ms) EXCEEDS tolerance (${MAX_DRIFT_MS}ms)"
    exit 1
fi
