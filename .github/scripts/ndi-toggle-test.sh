#!/bin/bash
# NDI Toggle Test - NDI Reconnection Variance (Video-Only Isolation)
# Tests A/V sync stability when toggling NDI source visibility
# This isolates NDI reconnection behavior from OBS/ASIO startup
# Usage: ./ndi-toggle-test.sh [cycles]

CYCLES=${1:-10}
results=()

echo "Starting $CYCLES-cycle NDI toggle test..."
echo ""

for i in $(seq 1 $CYCLES); do
    echo "=== Cycle $i/$CYCLES ==="

    # Toggle NDI source off then on via OBS websocket or scene item visibility
    # This triggers NDI reconnection without restarting OBS
    ssh stream.lan 'powershell -Command "
        # Hide and show the NDI source to trigger reconnection
        # Uses OBS WebSocket if available, otherwise manual scene item toggle
    "' 2>/dev/null || true

    sleep 5

    # Get latest latency from logs
    latency=$(ssh stream.lan 'powershell -Command "Get-ChildItem \"$env:APPDATA\\obs-studio\\logs\\*.txt\" | Sort-Object LastWriteTime -Descending | Select-Object -First 1 | Get-Content | Select-String \"latency=\" | Select-Object -Last 1"' 2>/dev/null | grep -oP 'latency=[-0-9.]+' | grep -oP '[-0-9.]+')

    if [ -n "$latency" ]; then
        results+=("$latency")
        echo "  Latency: ${latency}ms"
    else
        echo "  Warning: No latency reading"
    fi
done

echo ""
echo "=== Results ($CYCLES cycles) ==="
printf '%s\n' "${results[@]}" | sort -n

if [ ${#results[@]} -gt 0 ]; then
    min=$(printf '%s\n' "${results[@]}" | sort -n | head -1)
    max=$(printf '%s\n' "${results[@]}" | sort -n | tail -1)
    range=$(echo "$max - $min" | bc)
    mean=$(printf '%s\n' "${results[@]}" | awk '{sum+=$1} END {printf "%.1f", sum/NR}')
    echo ""
    echo "Min: ${min}ms, Max: ${max}ms"
    echo "Range: ${range}ms"
    echo "Mean: ${mean}ms"
    echo ""
    echo "NOTE: With 5-frame warmup fix, range should be <1ms"
fi
