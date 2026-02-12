# Agent Rules for DistroAV Development

## Completion Criteria

A task is NOT complete until ALL of the following have been done:

### 1. Implementation
- All code changes are written and saved
- No syntax errors or obvious bugs

### 2. Build
- Code compiles successfully on CI or locally
- No build warnings related to the changes

### 3. Deployment
- Built artifacts are deployed to stream.lan test environment
- OBS is restarted with the new plugin loaded
- Plugin loads without errors (check OBS logs)

### 4. Verification Testing
- Run the 30-cycle OBS restart test:
  ```bash
  .github/scripts/obs-restart-sync-test.sh 30 2
  ```
- **Pass criteria**: Max A/V sync drift ≤ 2ms across all 30 cycles
- If test fails, debug and fix before marking complete

### 5. Only Then
- Report results to user
- Mark task as complete

## Never Stop Early

- Do NOT stop after just writing code
- Do NOT stop after just building
- Do NOT stop after just deploying
- ALWAYS run the 30-cycle verification test
- ALWAYS wait for test results before declaring success

## Test Environment

- **Test host**: stream.lan (Windows)
- **OBS restart**: Via scheduled task `schtasks /run /s stream.lan /tn "OBS Restart"`
- **Plugin path**: `C:\Program Files\obs-studio\obs-plugins\64bit\distroav.dll`
- **Build artifacts**: Download from GitHub Actions CI

## Debugging Failed Tests

If the 30-cycle test fails:
1. Check sync dock timing output for anomalies
2. Review OBS logs for errors
3. Analyze timestamp patterns before/after restarts
4. Fix root cause and re-run full 30-cycle test
