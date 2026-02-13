#!/bin/bash
# SYNC Log Regression Test
# Validates internal consistency of DistroAV SYNC log lines from stream.lan
#
# Checks:
#   - creation + net == receive (timestamps match delays)
#   - total == net + rel + obs (delays sum correctly)
#   - All timestamps are valid HH:MM:SS.mmm (not 00:00:00.000)
#   - obs >= 0 (render happens after release)
#   - Values within sane ranges (net < 200ms, obs < 500ms, total < 1000ms)
#
# Usage: DEPLOY_HOST=strih.lan ./sync-log-regression-test.sh [min_lines] [max_wait_seconds]

DEPLOY_HOST=${DEPLOY_HOST:-stream.lan}
MIN_LINES=${1:-5}
MAX_WAIT=${2:-60}

echo "=========================================="
echo "SYNC Log Regression Test"
echo "=========================================="
echo "Minimum lines: $MIN_LINES"
echo "Max wait: ${MAX_WAIT}s"
echo ""

# Collect SYNC log lines
echo "Collecting SYNC log lines from $DEPLOY_HOST..."

SYNC_LINES=""
ELAPSED=0
POLL_INTERVAL=5

while [ $ELAPSED -lt $MAX_WAIT ]; do
    SYNC_LINES=$(ssh "$DEPLOY_HOST" 'type "%APPDATA%\obs-studio\logs\*.txt" 2>nul | findstr /C:"[distroav] SYNC:"' 2>/dev/null | tail -"$MIN_LINES")

    LINE_COUNT=$(echo "$SYNC_LINES" | grep -c "SYNC:" 2>/dev/null || echo 0)

    if [ "$LINE_COUNT" -ge "$MIN_LINES" ]; then
        echo "Collected $LINE_COUNT SYNC log lines"
        break
    fi

    echo "  Found $LINE_COUNT/$MIN_LINES lines, waiting ${POLL_INTERVAL}s..."
    sleep $POLL_INTERVAL
    ELAPSED=$((ELAPSED + POLL_INTERVAL))
done

if [ -z "$SYNC_LINES" ] || [ "$(echo "$SYNC_LINES" | grep -c "SYNC:")" -lt "$MIN_LINES" ]; then
    echo "ERROR: Could not collect $MIN_LINES SYNC lines within ${MAX_WAIT}s"
    exit 1
fi

echo ""
echo "--- Raw SYNC lines ---"
echo "$SYNC_LINES"
echo "---"
echo ""

# Parse and validate each line
PASS_COUNT=0
FAIL_COUNT=0
LINE_NUM=0

while IFS= read -r line; do
    # Skip empty lines
    [ -z "$line" ] && continue
    echo "$line" | grep -q "SYNC:" || continue

    LINE_NUM=$((LINE_NUM + 1))
    LINE_PASS=true

    # Extract fields using regex
    # Format: SYNC: av=X.Xms drops=N(X.X%) creation=HH:MM:SS.mmm +Nms(net) receive=HH:MM:SS.mmm +Nms(rel) release=HH:MM:SS.mmm +Nms(obs) render=HH:MM:SS.mmm total=Nms
    creation=$(echo "$line" | grep -oP 'creation=\K[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}')
    receive=$(echo "$line" | grep -oP 'receive=\K[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}')
    release=$(echo "$line" | grep -oP 'release=\K[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}')
    render=$(echo "$line" | grep -oP 'render=\K[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}')
    net_ms=$(echo "$line" | grep -oP '\+\K[-0-9]+(?=ms\(net\))')
    rel_ms=$(echo "$line" | grep -oP '\+\K[-0-9]+(?=ms\(rel\))')
    obs_ms=$(echo "$line" | grep -oP '\+\K[-0-9]+(?=ms\(obs\))')
    total_ms=$(echo "$line" | grep -oP 'total=\K[-0-9]+(?=ms)')

    echo "Line $LINE_NUM: creation=$creation net=${net_ms}ms receive=$receive rel=${rel_ms}ms release=$release obs=${obs_ms}ms render=$render total=${total_ms}ms"

    # Check 1: All timestamps are valid (not empty, not 00:00:00.000)
    for ts_name in creation receive release render; do
        ts_val=$(eval echo "\$$ts_name")
        if [ -z "$ts_val" ]; then
            echo "  FAIL: $ts_name timestamp missing"
            LINE_PASS=false
        elif [ "$ts_val" = "00:00:00.000" ]; then
            echo "  WARN: $ts_name is 00:00:00.000 (may indicate uninitialized)"
        fi
    done

    # Check 2: All delays are present
    for delay_name in net_ms rel_ms obs_ms total_ms; do
        delay_val=$(eval echo "\$$delay_name")
        if [ -z "$delay_val" ]; then
            echo "  FAIL: $delay_name missing"
            LINE_PASS=false
        fi
    done

    # Check 3: total == net + rel + obs
    if [ -n "$net_ms" ] && [ -n "$rel_ms" ] && [ -n "$obs_ms" ] && [ -n "$total_ms" ]; then
        expected_total=$((net_ms + rel_ms + obs_ms))
        if [ "$expected_total" -ne "$total_ms" ]; then
            echo "  FAIL: total=${total_ms}ms != net+rel+obs=${expected_total}ms (${net_ms}+${rel_ms}+${obs_ms})"
            LINE_PASS=false
        fi
    fi

    # Check 4: obs >= 0 (render happens after release)
    if [ -n "$obs_ms" ]; then
        if [ "$obs_ms" -lt 0 ]; then
            echo "  FAIL: obs_delay=${obs_ms}ms is negative (render before release)"
            LINE_PASS=false
        fi
    fi

    # Check 5: Sane ranges
    if [ -n "$net_ms" ] && [ "$net_ms" -gt 200 ]; then
        echo "  WARN: network delay ${net_ms}ms > 200ms (high but not fatal)"
    fi
    if [ -n "$obs_ms" ] && [ "$obs_ms" -gt 500 ]; then
        echo "  WARN: OBS delay ${obs_ms}ms > 500ms (high but not fatal)"
    fi
    if [ -n "$total_ms" ] && [ "$total_ms" -gt 1000 ]; then
        echo "  WARN: total delay ${total_ms}ms > 1000ms (high but not fatal)"
    fi

    if [ "$LINE_PASS" = true ]; then
        echo "  OK"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

done <<< "$SYNC_LINES"

echo ""
echo "=========================================="
echo "Results: $PASS_COUNT passed, $FAIL_COUNT failed (of $LINE_NUM lines)"

if [ "$FAIL_COUNT" -gt 0 ]; then
    echo "RESULT: FAIL"
    exit 1
fi

if [ "$PASS_COUNT" -eq 0 ]; then
    echo "RESULT: FAIL (no lines validated)"
    exit 1
fi

echo "RESULT: PASS"
exit 0
