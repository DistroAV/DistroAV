# DistroAV Cleanup Audit

Scope: `CMakeLists.txt`, `src/` (output, filter, source, finder, sync-debug, update, plugin-main). See "Status" at the end for what was done.

## Hardcoded values that should be shared

| Where | Problem | Fix |
|---|---|---|
| `src/ndi-output.cpp` (103-127), `src/preview-output.cpp` (155-156), `src/forms/output-settings.cpp` (383-404) | Settings keys `"ndi_name"` / `"ndi_groups"` repeated as literals in 3+ files | Define `constexpr` keys in one header |
| `src/ndi-output.cpp` (115-116) | Default `"DistroAV output (changeme)"` used for both name and groups | Named constant; groups should likely default to empty |
| `src/ndi-output.cpp` (~290-300) | `strcmp` against `"NDI Main Output"` / `"NDI Preview Output"` | Share output-name constants with `main-output.cpp` / `preview-output.cpp` |
| `src/plugin-main.h` (64-75), `src/plugin-main.cpp` (91) | ~10 URLs repeat the `https://distroav.org` prefix | Single `PLUGIN_WEB_BASE_URL` macro |
| `src/ndi-filter.cpp` (28-29), `src/ndi-source.cpp` (32-47) | Property keys as `#define`s; filter and output use different key names for the same concept | One `constexpr` constants header |
| `src/ndi-output.cpp` (~215, ~420), `src/ndi-finder.cpp` (51) | Magic numbers: 1 s throttle, 10 ms connection timeout, 1000 ms finder wait | Named constants |
| `src/sync-debug.cpp` (60) | `1000000000`, `1000000` literals | `std::chrono` or named constants |
| `src/forms/output-settings.cpp` (86) | Untranslated `"NDI Library not loaded"` | Move to locale files |
| `data/locale/en-US.ini` (39), `src/ndi-filter.cpp` (152, 376-393) | `NDIPlugin.FilterProps.NDIName.Default` (`"${filter} (${source})"`) is a locale string, but it is a functional template parsed by the plugin via the `${source}` / `${filter}` tokens. Translators can break it. | Define it as a plugin constant (e.g. `NDI_FILTER_DEFAULT_NAME`) next to the token constants and remove the key from all locale files |

## Performance and correctness

- Wrong frame rate: `ndi_output_rawvideo` sets `frame_rate_N = fps*100`, `frame_rate_D = 100` (TODO says broken for fractional rates). Use `fps_num` / `fps_den` from `video_output_get_info()`.
- Race on sender: mutex is released before `send_send_video_async_v2`; `ndi_output_stop` can destroy the sender meanwhile. `ndi_output_stop` also checks `o->ndi_sender` before taking the lock.
- Dangling pointers: `o->ndi_name` / `o->ndi_groups` point into `obs_data_t` strings. Store `std::string` copies.
- Oversized buffer: I444 conversion buffer is `height * linesize * 2`, but `conv_linesize` is already `width * 2`. Allocates 2x what is needed.
- Duplicated code: connection-check block exists in both video and audio paths (likely also `ndi-filter.cpp` ~190). Extract a helper.
- Audio path does a per-channel `memcpy` every callback. Check whether avoidable against the NDI SDK.
- `video_to_color_format_map.at(format)` throws for unmapped formats. Use `find`.
- I444 to UYVY conversion is a scalar byte loop on the video thread.
- Connection throttle still takes the lock and calls `steady_clock::now()` on every frame.

## Outdated practices

- C-style casts (`(ndi_output_t *)data`) and `NULL`. Use `static_cast` / `nullptr`.
- `bzalloc` / `bfree` and raw `pthread_mutex_t` for plain C++ state. Use `new`, `std::mutex`, `std::unique_ptr` / `std::vector`. Raw `new[]` / `delete[]` for `conv_buffer` should be `std::vector<uint8_t>`.
- `min_uint32` helper. Use `std::min`.
- `typedef struct {...} ndi_output_t` is C style. Use `struct`.
- `new QTimer()` in `src/forms/update.cpp` (566) has no parent. Check `new QProgressDialog` and `new QMessageBox` too.
- `src/plugin-support.c.in` (33) uses `malloc`.

## CMake and build

- `find_package(Qt6 COMPONENTS ...)` lacks `REQUIRED`.
- macOS AGL-stripping block (~20 lines) is inline. Move to `cmake/macos/` or guard by Qt version.
- `WINDOWS_APP_UUID` check, `installer-Windows.iss` `configure_file`, and `.ico` copy run on all platforms. Guard with `if(WIN32)`.
- Confirm `build_macos/` and `release/` are in `.gitignore`.
- `cmake_minimum_required(3.28...3.30)` is hardcoded. Qt component list is repeated in two places; add `Gui` to components since the AGL loop references `Qt6::Gui`.
- Manual `target_sources` list is easy to forget when adding files.

## Status

Done:

- Shared constants (settings keys, output names, URLs, filter name template) in `plugin-main.h`; filter default name removed from locale files.
- `ndi-output.cpp`: fractional frame rate, sender lock held through send, `std::string` names, correct I444 buffer size, safe format lookup, `std::vector` buffers, `new`/`delete`, `static_cast`.
- Shared `ndi_connection_monitor` (declared in `plugin-main.h`, defined in `plugin-main.cpp`) used by output and filter.
- `ndi_pack_planar_audio` skips the per-channel audio copy when OBS planes are contiguous; the path taken is logged at debug level on change.
- `ndi-filter.cpp`: `new`/`delete`, `std::vector` audio buffer, `static_cast`, pthread mutexes now destroyed.
- `ndi-source.cpp`: atomic `running` / `reset_ndi_receiver`, `config_mutex` guarding name strings, PTZ and tally, reset flag no longer overwritten, named constants, `static_cast`.
- `ndi-finder.cpp` / `sync-debug.cpp`: named constants, `nullptr`.
- `update.cpp`: disabled `UPDATE_REQUEST_QT` path fixed (stray `)`, duplicate call, unparented `QTimer`). Other `new Q*` dialogs already use `WA_DeleteOnClose` / `deleteLater` or a parent.
- CMake: Windows-only steps under `if(WIN32)`, `Qt6 REQUIRED` with `Gui`, AGL stripping moved to `cmake/macos/helpers.cmake`. `target_sources` stays explicit on purpose (matches the OBS plugin template). `plugin-support.c.in` is OBS's C template, so `malloc` stays.

Open:

- Filter still uses `pthread_mutex_t` with conditional locking (video + audio); changing it needs runtime testing.
- I444 to UYVY conversion is a scalar loop.
- `"NDI Library not loaded"` in `output-settings.cpp` is untranslated.
- Output groups default is the same text as the name; an empty default would be more sensible but changes behavior.
- If the debug log shows OBS never delivers contiguous audio planes, remove `ndi_pack_planar_audio` and go back to the plain copy.
