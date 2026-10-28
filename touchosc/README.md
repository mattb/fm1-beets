# Beets TouchOSC prototype

Open **Beets.tosc** in TouchOSC (current generation, not TouchOSC Mk1). It is a
self-contained, receive-only 1024×768 landscape layout for Mac or iPad. All Lua
is embedded in the document root; the adjacent `beets.lua` is the editable source.

1. Connect the FM1 running Beets 0.2.0 by USB.
2. In TouchOSC **Connections → MIDI**, enable one connection, choose the FM1
   as **Receive Port** (currently `X0X FM-1` on this Mac), and leave Send Port blank.
3. Press TouchOSC's triangle to leave the editor. The banner should say
   **USB MIDI / LIVE** within one second, even with the FM1 stopped.

On iPad, transfer the `.tosc` file into TouchOSC and select the locally enumerated
FM1 port. The MIDI port selection belongs to the app/device, not this layout.
A powered USB adapter/hub may be needed. Mac operation was tested; iPad operation
has not yet been tested.

Both decks stay visible: ten loop choices, eight slices, queued selections (`>`),
source playheads, playback-window bounds, mute and held-effect state, and five
probabilities. Colored probability tiles show mutations that fired. Small bottom
rows follow held white keys and buttons; the event line shows recent edges and
encoder deltas. The top bar is phase within an eighth note. Positions update from
received snapshots, without speculative audio-position interpolation.

The root matches `F0 7D 42 45 45 54 53 01 ... F7`, validates printable ASCII and
version/schema, and consumes only recognized Beets frames. Other MIDI is left
alone. There are no outgoing MIDI/OSC messages or touch actions. After three
seconds without a state snapshot it marks the feed stale and clears held-control
indicators; the last musical state remains visible. RX, sequence discontinuity,
malformed-frame and firmware-drop counters are displayed at the bottom.

**Compatibility correction:** the suggested `pcall(json.toTable, ...)` pattern
fails in TouchOSC 1.5.3.266: its Lua sandbox does not expose `pcall`. This layout
uses the native `json.toTable` directly and checks the result before use. The
prototype targets our known firmware schema, not arbitrary untrusted JSON.

## Rebuild and verify

From the repository root:

```sh
.venv/bin/python tools/build-touchosc.py
.venv/bin/python tests/touchosc_receiver_test.py
.venv/bin/python tools/smoke-usb-midi.py --output build/tests/touchosc-live
```

The Lua replay test uses the hardware capture `build/tests/usb-telemetry.jsonl`
from the USB smoke test. It mocks only TouchOSC objects and JSON decoding, runs
the actual root callback, and checks mapping, filtering, sequence and staleness.
Close the document before regenerating it, and reopen afterward. Running scripts
change control properties; when closing a preview, discard those runtime changes
unless you intentionally want them saved as defaults.

Live macOS verification on 2026-10-28: opened the generated file in TouchOSC
1.5.3.266, received state directly from the physical FM1, and observed both decks,
transport, tempo and advancing slice/playhead displays with an empty script-error
log. Concurrent USB harness passed with 139 state + 6 input frames, no sequence
gaps or input drops, and separate remote acknowledgements. No bridge is required.

API references: [callbacks](https://hexler.net/touchosc/manual/complete),
[control properties](https://hexler.net/touchosc/manual/script-properties-and-values),
[Lua subset](https://hexler.net/touchosc/manual/script-functions-lua).
