# DistroAV receiver-clock minimal fix

Base: **DistroAV 6.2.1**

This package contains the smallest source overlay for the receiver-clock change.
It intentionally excludes the Clock Lab CSV recorder, diagnostic probe filters,
analysis script, test modes, UI additions, startup-hardening registry, and research documentation.

## File to copy

```text
src/ndi-source.cpp
```

Copy that file over the matching file in a branch created directly from the
DistroAV `6.2.1` tag.

## Behavior

- **FrameSync disabled:** Stock DistroAV Direct Receive remains unchanged.
- **FrameSync enabled:** The FrameSync path uses one receiver-owned epoch and
  generates audio/video timestamps from the receiving OBS clock.

The receiver-paced FrameSync path:

- derives audio time from cumulative delivered samples;
- derives video time from OBS frame ticks;
- pulls both streams on receiver-owned deadlines;
- limits audio catch-up work when the thread is late;
- skips missed video ticks instead of permanently shifting the schedule;
- resets the scheduler when the receiver/FrameSync is rebuilt;
- refreshes the epoch while disconnected or hidden so stale time is not caught up later.

## Files intentionally not included

```text
src/receiver-clock-diagnostics.cpp
src/receiver-clock-diagnostics.h
tests/receiver-clock-diagnostics-test.cpp
tools/analyze-receiver-clock-log.py
data/locale/en-US.ini
CMakeLists.txt
plugin-main.cpp
buildspec.json
```

They are not required by this minimal implementation.

## Build and test

1. Create a branch from DistroAV `6.2.1`.
2. Copy `src/ndi-source.cpp` into the repository.
3. Commit and push the branch.
4. Let the existing DistroAV GitHub Actions workflow build it.
5. Install the resulting test artifact with OBS closed.
6. Enable **FrameSync** on the receiving NDI source.
7. Confirm the OBS log contains:

```text
Receiver-paced FrameSync scheduling active
```

8. Run the same long-duration A/V test used for the Stock Direct control.

## Validation status

The scheduling/timestamp behavior was proven in the full Receiver Clock Lab:

- Stock Direct control: approximately **-1.802 ms/minute**
- Receiver-Paced validation: approximately **-0.00094 ms/minute**

This stripped overlay has not been compiled in this packaging environment because
the OBS development SDK is not installed here. The repository's GitHub Actions
build should be treated as the authoritative compile check before testing.
