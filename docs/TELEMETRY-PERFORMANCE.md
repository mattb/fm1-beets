# USB telemetry performance

Measured on the connected FM1 on 2026-10-28. Baseline: instrumented firmware `FM-1_731000201`, based on `c4fd2fe`; optimized: `FM-1_731000202`. Each condition ran for approximately eight seconds using `tools/profile-usb-midi.py`. Raw results: [baseline](performance/baseline.json), [optimized](performance/optimized.json).

## Results

The figures below are time spent inside instrumented `app_loop`, divided by the measurement window. These elapsed-time shares include interrupt preemption and instrumentation. Exclusive CPU utilization was not measured. Nested scopes must not be added to the total. Disabled mode still measures a small function/check overhead.

| Condition | Before | After | Full snapshots/s, before -> after |
|---|---:|---:|---:|
| stopped / normal | 1.993% | 0.153% | 1604.2 -> 1.0 |
| stopped / disabled | 0.023% | 0.022% | 0.0 -> 0.0 |
| stopped / blocked | 27.604% | 1.210% | 623.5 -> 26.3 |
| playing / normal | 2.457% | 0.693% | 1537.4 -> 10.7 |
| playing / disabled | 0.022% | 0.022% | 0.0 -> 0.0 |
| playing / blocked | 28.012% | 1.341% | 591.2 -> 26.3 |

Normal playback fell by about 72%; stopped overhead fell by about 92%. The disabled baseline is about 0.022%, leaving approximately 0.13 percentage points stopped and 0.67 points playing above that baseline after the change. Playback snapshot rates differ slightly between the short runs; the short sessions provide representative observations with limited statistical precision.

In the blocked condition, the profiling shim rejects every send to exercise application backpressure logic. The USB host remained connected. The old loop repeatedly serialized the same due state; a 20 ms device-clock retry interval now bounds attempts to at most 50/s (approximately 26/s observed in this run). Transport tests check that disconnected polling avoids engine reads. Disconnected hardware timing was not measured.

The audio xrun counter stayed unchanged during these short sessions. Audible quality and worst-case timing need separate tests. USB enqueue remains roughly 95-100 microseconds per accepted state frame and uses an interrupt lock. Whole-platform maximum IRQ-lock durations were 194-196 microseconds stopped and 360-369 microseconds playing across these runs; those maxima include other platform work and cannot be attributed solely to telemetry. The integer audio-render load field is not a telemetry CPU measurement.

## Implementation

- Poll at most once per device millisecond; reject disconnected work before reading the engine.
- Read only playing/ticks/phase to decide whether a musical update is due.
- Capture full engine/UI state only when a state frame will be serialized.
- After a rejected send, wait 20 ms before retrying. Capture fresh state on retry.
- Retain the bounded input FIFO, accepted-only sequence increments, reconnect snapshot and 100 ms state-priority policy.

The change adds a clock snapshot and a retry interval. JSON format, receiver behavior and saved settings remain compatible.

## Reproducing measurements

The optional profiler is firmware-only. Temporarily set `BEETS_PROFILE` to `1` in `app/telemetry_profile.h`, build a uniquely versioned firmware with `tools/build-local.sh --release VERSION`, and install it using `fm1-base/tools/fm1_install.py`. Then run:

```sh
.venv/bin/python tools/profile-usb-midi.py --seconds 8 --output build/profile/run.json
```

The harness temporarily changes transport and restores it; it never saves settings. It uses development SysEx `F0 7D 42 50 OP F7`, separate from the BEETS protocol. Operations reset/enable (0), report (1), disable (2), or force rejection (3). Reports contain 17 uint32 values, each encoded as five little-endian seven-bit groups: elapsed ticks; count/sum/max for loop, snapshot, serialization and send; rejected sends; platform maximum lock time; xrun delta; audio-render percentage. TIMER4 runs at 24 MHz. Keep windows short: the raw counter wraps in about 179 seconds; the harness restricts windows to 1-30 seconds.

Return `BEETS_PROFILE` to `0` and rebuild/install for normal use. Counters, test commands and timing calls compile out. The production 0.2.3 image is 417,160 bytes, 112 bytes larger than the previous 417,048-byte production image. No profiler protocol is enabled in that image.

## Validation

Firmware, native host and WASM builds passed. The full existing regression suite passed, including engine/asset tests, transport semantics, native controls/persistence and WASM tests. Added transport assertions cover 10,000 same-millisecond calls, idle snapshot avoidance, disconnected engine-read avoidance, and the 50/s retry bound.

Production firmware `FM-1_731000203` was installed and independently queried as running on USB port `X0X FM-1`. The production USB smoke passed: 138 state frames, six input frames, maximum 1,484 bytes, zero sequence gaps and no new input drops. It exercised ASCII JSON/framing, Start/Stop, advancing clock/playheads, encoder change/restore, button/key press/release and remote ACK coexistence. See [smoke summary](performance/production-smoke.json); local raw captures are `build/tests/telemetry-efficient.syx` and `.jsonl`. Profiling symbols are absent from the production disassembly. Physical listening, extensive control combinations and iPad testing remain open checks.
