#!/bin/bash
# NDI A/V Sync Test - OBS Restart Variance
# Tests the A/V sync stability across multiple OBS restarts
# Usage: ./ndi-sync-test.sh [cycles]

CYCLES=${1:-10}
results=()

echo "Starting $CYCLES-cycle OBS restart A/V sync test..."
echo ""

for i in $(seq 1 $CYCLES); do
    echo "=== Cycle $i/$CYCLES ==="

    # Restart OBS
    ssh stream.lan 'taskkill /IM obs64.exe /F' 2>/dev/null || true
    sleep 2
    ssh stream.lan 'schtasks /Run /TN "StartOBS"'
    sleep 8

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
fi
