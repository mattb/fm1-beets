# Current verification record

## Multichannel branch — 2026-10-31

Firmware, native and WASM builds and the full Beets suite passed. Default engine
PCM compares byte-for-byte with the previous engine. WASM verifies six stems,
split output, persistence and legacy saves. A muted Chrome smoke passed power,
play, B unmute, Amen selection and stop.

The base diagnostic passed physical six-channel input and output routing, channel
names, native 44.1 kHz formats, a 60-second duplex tone recording and shared-impulse
alignment. Default stereo descriptors and PCM compare exactly against upstream.
See [duplex evidence](verification/multichannel-duplex-functional-summary.json).

After functional testing and review, mono Mac→FM1→Mac loopback measured 39.669 ms
median with 128-frame callbacks and 41.089 ms with 256-frame callbacks. Each test
matched 36 impulses with no host callback errors. These are application wall-clock
times including Mac buffering and scheduling; they do not isolate firmware delay.
See [latency evidence](verification/multichannel-final-wallclock-summary.json).

The diagnostic hard-zeros analog output. No analog electrical measurements or
iPad compatibility tests were performed. Raw recordings remain under
`build/multichannel/hardware/`; the published summaries identify their hashes.

Beets **0.4.0** was then installed and identified as `FM-1_731000400`. Ten
physical USB captures passed A-only, B-only, both-muted and both BLEND extremes
in mono and split modes. Master channels remained zero while the expected stems
played; duplicate deck channels differed by at most two PCM16 counts. The harness
restored blend, mutes and output mode, left transport stopped, and returned MASTER
to the physical pot after fresh stopped-and-muted telemetry. It never saved settings.
See [Beets stem evidence](verification/multichannel-beets-stems.json).

## Earlier production verification — 2026-10-28

Firmware **0.3.1**, device identity `FM-1_731000301`, added persistent split output.
The platform and carried patches are described in [DEPENDENCIES.md](DEPENDENCIES.md).

## Split-output update

- All three targets built; full automated suite passed (`build/logs/split-build.log`,
  `build/logs/split-suite.log`). Firmware image: 424,880 bytes.
- Native tests verify EDIT + PRESETS, unchanged blend, routing changes blocked
  during playback, SAVE and reboot into split and mono.
- WASM tests verify A-left/B-right isolation after SAVE/reload, exact dual-mono
  equality after returning to mono, and unchanged audio from legacy saved setups.
- Chrome smoke checked the gesture, temporary messages, SAVE, reload, playback,
  blocked routing changes while playing, and return to mono. No captured console errors.
- Physical USB audio captures passed channel isolation with host capture requests
  of 44.1 and 48 kHz. Those tests did not verify the device sampling rate. The
  base configuration did not enable its optional 48 kHz path, so the 48 kHz
  recording is not evidence of device-side 48 kHz support (corrected 2026-10-31).
  The mono capture had a maximum two-count PCM16 channel difference; the initial
  exact-equality capture assertion failed and the saved WAV was rechecked with that
  explicit tolerance. [Capture results and limits](verification/split-usb-audio.json).
- Firmware was flashed and its identity queried. Physical power-cycle persistence,
  analog channel capture and iPad USB audio remain untested. The device was left
  stopped in mono with the physical MASTER knob in control.

To repeat the audio capture, install `sounddevice` alongside the MIDI harness
requirements and run `.venv/bin/python tools/smoke-usb-audio.py`. This operates
transport and mutes, restores routing/blend/mutes/transport, and does not SAVE.
The physical MASTER knob remains in control: choose a comfortable audible level.
Captured WAVs are local in `build/tests/usb-audio/`.

## MIDI clock update

- Firmware, native host and WASM builds passed (`build/logs/midi-clock-all.log`).
- Full automated suite passed, including sanitized clock/controller tests and
  existing behavior, telemetry, native and WASM checks
  (`build/logs/midi-clock-suite-final.log`).
- The physical USB clock harness passed input at 120/180 BPM, outgoing clock,
  transport, clock loss and no echo. [Result](verification/midi-clock.json).
- Physical JSON-over-SysEx smoke passed: 145 state and 8 input frames, no sequence
  gaps or new input drops. [Result](verification/midi-clock-telemetry.json).
- Chrome smoke passed power-on, PLAY, Amen selection, B unmute/focus, repeat,
  latched B3–B6 range, release/HOME and STOP. The screen showed `INT CLOCK` and
  moving playheads; no captured console warnings/errors. This checks interaction,
  not listening or browser Web MIDI. [Artifact record](verification/midi-clock-browser.json).
- Physical TRS and analog conversion remain untested. USB arrival timing and
  source-selection policy are described in [MIDI clock](MIDI-CLOCK.md).

The following records describe earlier revisions; processing-cost and TouchOSC
measurements have not been repeated on 0.3.0.

## Earlier evidence

The prose update rebuilt WASM and passed the full suite. A fresh Chrome session
checked the revised instructions, playback, deck selection, probability adjustment,
range selection and stop. The [smoke summary](verification/prose-smoke.json) records
source/artifact hashes; [the README screenshot](images/beets-web.jpg) shows that session.
Build and test logs are `build/logs/prose-web-build.log` and `build/logs/prose-suite.log`.

| Area | Result and scope | Evidence |
|---|---|---|
| Source contract fixtures | Passed in the fixture update: six literal boundary traces plus the FM1 manual-priority adaptation, with ASan/UBSan. No Norns audio oracle. | [Fixture provenance and limits](../tests/fixtures/README.md); `app/build/tests/contract.log` |
| Full automated suite | Passed in the fixture update using existing native/WASM artifacts: asset provenance/decoding, sanitized engine and telemetry, host controls/persistence, actual WASM execution/audio/persistence. | `build/logs/contract-verification-suite.log`; `app/build/tests/` |
| Three target builds | Passed on 2026-10-28 for the optimized runtime: firmware, native host, WASM. Superseded by the 0.3.0 build above. | `build/logs/telemetry-efficient-final-build.log`, `telemetry-efficient-host.log`, `telemetry-efficient-web.log` |
| Physical firmware identity | Queried during the fixture update: `FM-1_731000203` running, USB port `X0X FM-1`. Production 0.2.3; profiling disabled. | `.venv/bin/python fm1-base/tools/fm1_install.py --info`; prior install log `build/logs/telemetry-efficient-final-flash.log` |
| Physical USB MIDI | Passed on 2026-10-28 on production 0.2.3: 138 state + 6 input frames, no sequence gaps/new drops. ASCII JSON/framing, Start/Stop, advancing playheads, remote encoder/key/button edges and ACK coexistence. | [Committed smoke summary](performance/production-smoke.json); local raw `build/tests/telemetry-efficient.syx` and `.jsonl` |
| Device processing cost | Measured on 2026-10-28: enabled/disabled/forced-rejection sessions; zero observed xrun increments in short windows. Elapsed scopes include interrupt preemption. | [Performance report and raw results](TELEMETRY-PERFORMANCE.md) |
| macOS TouchOSC | Live verification on 2026-10-28: TouchOSC 1.5.3.266 received the physical device stream and updated the UI without script errors. Not rerun for this test/docs update. | [Live-session record](../touchosc/README.md#rebuild-and-verify) |
| Chrome browser interaction | Passed in the fixture update: power-on, PLAY, Amen selection on A, unmute/focus B, repeat adjustment, latched B3-B6 range, release/HOME recovery and STOP. Running screen showed both deck playheads at changing positions. No captured console warnings/errors. Visual/interaction check, not a listening or output-device test. | [Smoke summary](verification/chrome-smoke.json); local `app/build/tests/chrome-range.jpg`, `chrome-stopped.jpg`, `chrome-stopped-dom.txt` |

Paths shown as code are local, generated evidence and are not committed. The
linked performance JSON and fixture expectations are committed. Run the checks again after a fresh build to generate evidence for that build.

## Remaining coverage gaps

- Physical DAC listening and transition quality, prolonged worst-case playback,
  and hands-on key/chord/MASTER feel. Remote panel injection does not test the
  physical sensors or electrical controls.
- Physical TRS MIDI and prolonged USB audio quality; the short captures above
  establish channel routing only. Device SAVE plus power-cycle recovery has not been verified
  here; persistence is covered in native and WASM tests.
- iPad TouchOSC and other controller/OS combinations.
- Softcut render equivalence: the sample encoding, rate conversion and fades are
  deliberate adaptations. Literal decision traces do not prove audio parity.

Link, arbitrary sample loading, recording/overdub,
filters, crow and arc support are omitted from this app. See [scope and adaptations](PORT_REVIEW.md).

## Refreshing the record

Use `tools/build-local.sh --all` when runtime/platform code changes, then
`tests/run-tests.sh --no-build`. To test the connected device, explicitly install
the intended version, query its identity, and run
`tools/smoke-usb-midi.py --output build/tests/usb-telemetry` using `.venv/bin/python`.
The smoke temporarily operates transport and controls, then restores them; it is
not a listening test. TouchOSC validation has separate instructions in its README.

Update the rows with the tested revision, date, artifact/device identity and log
paths. Keep the date and scope of each observation attached to its result.
