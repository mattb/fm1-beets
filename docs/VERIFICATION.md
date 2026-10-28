# Current verification record

Updated 2026-10-28. This is the current index of tested behavior and remaining
gaps; older reports retain their original scope. Runtime source is `14cd0d9`
(telemetry optimization), also included in `5584cd9`. This update adds tests and
documentation only; it does not change the installed firmware. The platform pin
and carried patches are described in [DEPENDENCIES.md](DEPENDENCIES.md).

## Evidence

| Area | Result and scope | Evidence |
|---|---|---|
| Source contract fixtures | **Passed this update:** six literal boundary traces plus the FM1 manual-priority adaptation, with ASan/UBSan. No Norns audio oracle. | [Fixture provenance and limits](../tests/fixtures/README.md); `app/build/tests/contract.log` |
| Full automated suite | **Passed this update** using existing native/WASM artifacts: asset provenance/decoding, sanitized engine and telemetry, host controls/persistence, actual WASM execution/audio/persistence. | `build/logs/contract-verification-suite.log`; `app/build/tests/` |
| Three target builds | **Passed earlier on 2026-10-28** for the optimized runtime: firmware, native host, WASM. No runtime changes since. | `build/logs/telemetry-efficient-final-build.log`, `telemetry-efficient-host.log`, `telemetry-efficient-web.log` |
| Physical firmware identity | **Queried again this update:** `FM-1_731000203` running, USB port `X0X FM-1`. Production 0.2.3; profiling disabled. | `.venv/bin/python fm1-base/tools/fm1_install.py --info`; prior install log `build/logs/telemetry-efficient-final-flash.log` |
| Physical USB MIDI | **Passed earlier on 2026-10-28** on production 0.2.3: 138 state + 6 input frames, no sequence gaps/new drops. ASCII JSON/framing, Start/Stop, advancing playheads, remote encoder/key/button edges and ACK coexistence. | [Committed smoke summary](performance/production-smoke.json); local raw `build/tests/telemetry-efficient.syx` and `.jsonl` |
| Device processing cost | **Measured earlier on 2026-10-28:** enabled/disabled/forced-rejection sessions; zero observed xrun increments in short windows. Elapsed scopes include interrupt preemption. | [Performance report and raw results](TELEMETRY-PERFORMANCE.md) |
| macOS TouchOSC | **Prior live verification, 2026-10-28:** TouchOSC 1.5.3.266 received the physical device stream and updated the UI without script errors. Not rerun for this test/docs update. | [Live-session record](../touchosc/README.md#rebuild-and-verify) |
| Chrome browser interaction | **Passed this update:** power-on, PLAY, Amen selection on A, unmute/focus B, repeat adjustment, latched B3–B6 range, release/HOME recovery and STOP. Running screen showed both deck playheads at changing positions. No captured console warnings/errors. Visual/interaction check, not a listening or output-device test. | [Smoke summary](verification/chrome-smoke.json); local `app/build/tests/chrome-range.jpg`, `chrome-stopped.jpg`, `chrome-stopped-dom.txt` |

Paths shown as code are local, generated evidence and are not committed. The
linked performance JSON and fixture expectations are committed. A missing local
log does not invalidate the recorded run, but a fresh checkout should regenerate
its own evidence before claiming its build is verified.

## Remaining coverage gaps

- Physical DAC listening and transition quality, prolonged worst-case playback,
  and hands-on key/chord/MASTER feel. Remote panel injection does not test the
  physical sensors or electrical controls.
- Physical TRS MIDI and USB audio routing/quality; successful USB MIDI does not
  establish either. Device SAVE plus power-cycle recovery has not been verified
  here; persistence is covered in native and WASM tests.
- iPad TouchOSC and other controller/OS combinations.
- Softcut render equivalence: the sample encoding, rate conversion and fades are
  deliberate adaptations. Literal decision traces do not prove audio parity.

External MIDI clock sync, Link, arbitrary sample loading, recording/overdub,
filters, crow and arc support are **not implemented by this app**, rather than
merely awaiting verification. See [scope and adaptations](PORT_REVIEW.md).

## Refreshing the record

Use `tools/build-local.sh --all` when runtime/platform code changes, then
`tests/run-tests.sh --no-build`. To test the connected device, explicitly install
the intended version, query its identity, and run
`tools/smoke-usb-midi.py --output build/tests/usb-telemetry` using `.venv/bin/python`.
The smoke temporarily operates transport and controls, then restores them; it is
not a listening test. TouchOSC validation has separate instructions in its README.

Update the rows with the tested revision, date, artifact/device identity and log
paths. Keep prior observations labelled as prior; do not turn compilation or
successful installation into a claim of full hardware acceptance.
