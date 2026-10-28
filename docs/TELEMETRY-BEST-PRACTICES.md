# Building FM1 visualizers with USB MIDI, ASCII JSON and TouchOSC

An adoption guide based on the Beets firmware and its working TouchOSC prototype.
Verified implementation baseline: Beets 0.2.0, macOS, TouchOSC 1.5.3.266,
2026-10-28. This guide separates the tested implementation from recommendations
for extending it. It does not claim iPad hardware validation.

## 1. Keep the instrument authoritative

Publish enough state for an external display to draw the instrument, without
making the instrument depend on the display. Audio, sequencing and physical
controls must continue to work when the computer disconnects or stops reading.

Our path is:

```text
Audio engine + panel input
    → short, coherent snapshot in the main loop
    → bounded jems serializer
    → atomic, nonblocking USB MIDI SysEx enqueue
    → macOS/iPad MIDI input
    → TouchOSC document-root decoder
    → named display controls
```

No OSC bridge is required for the tested Mac connection. TouchOSC receives MIDI
from the class-compliant FM1 endpoint directly. A custom application can consume
the same protocol; the firmware does not need to know which display is attached.

Start with a receive-only display. If you later add remote commands, specify them
separately from telemetry, including validation, acknowledgements and conflicts
with physical input. Never make a received state update automatically send a
control command back: that creates feedback loops.

## 2. Make the firmware project reproducible

Keep the instrument in its own repository, with a pinned `fm1-base` submodule.
Use an explicit path override for shared development checkouts; Beets supports
`FM1_BASE` without silently modifying the external dependency (see
[dependency setup](DEPENDENCIES.md)). Own your engine, UI, assets, tests and build
wrapper in the instrument repository.
Avoid copying the platform wholesale or scattering app-specific behavior into it.

Beets adds a small platform extension for whole-frame, nonblocking SysEx output.
It is **not an assumed upstream API**. The extension lives in the tracked
[platform patch](../patches/fm1-base-telemetry.patch), applied idempotently by
[patch-base.sh](../tools/patch-base.sh) during setup/build. Consequently, the
submodule appears modified after setup even though its pinned commit is unchanged.
Do not commit a gitlink pointing at an unpublished local submodule commit: another
developer could not clone it. For long-term distribution, an upstreamed change or
a published, pinned fork can replace the patch.

On a base update, review and revalidate the patch; a failed applicability check
is a reason to inspect the change, not to force it over local edits. Keep the
firmware, native simulator and WASM builds using the same engine and serializer.
The simulator transport is a test adapter, not proof of USB hardware behavior.

Retain source licenses and vendor provenance. Our modified jems has an upstream
commit, license and explicit local changes in its [provenance file](../app/vendor/jems/PROVENANCE.md).
The application and platform have different licensing obligations; preserve both.

## 3. Define one JSON object per SysEx frame

Beets v1 uses:

```text
F0 7D 42 45 45 54 53 01 <printable ASCII JSON> F7
      B  E  E  T  S  version
```

`01` is a binary version byte, not ASCII `1`. `7D` is the development/non-commercial
manufacturer ID. Use your own protocol identifier for an incompatible instrument;
match the entire identifier and version on receipt. Sharing `7D` alone is not
sufficient to identify a message. Review manufacturer-ID requirements before a
commercial release.

There is no COBS, newline, NUL terminator or extra length prefix on the wire.
`F0`/`F7` frame the object. “JSONL” describes the decoded capture file, where the
monitor writes one object per line; it does not describe the MIDI payload.
USB MIDI packets and low-level callbacks may split a SysEx message: use a MIDI
API that reassembles it, or reassemble before parsing. Mido removes the enclosing
`F0`/`F7`; TouchOSC's callback includes them.

All JSON payload bytes must be 7-bit clean. Our stricter contract permits only
printable ASCII, `0x20` through `0x7E`; control characters inside strings are escaped.
For example, a sharp sign is `\u266f`, not its UTF-8 bytes. Supplementary Unicode
characters need valid surrogate pairs. Do not turn each UTF-8 byte into a separate
Unicode escape: that produces mojibake after decoding.

Use a bounded serializer, rather than concatenating unescaped strings or applying
`snprintf` to every field. Our jems adaptation decodes UTF-8 and emits ASCII escapes;
it also avoids stdio-based double/int64 number formatting. We use checked integer
decimal tokens through `jems_literal` for numeric fields. Do not pass arbitrary
text to the literal API. Use string APIs for names and messages. Fixed-point
integers with documented units simplify embedded builds and cross-language decoding.

Set a maximum **complete frame** size, including header and delimiters. Beets uses
3072 bytes. If serialization overflows, send nothing. Test the maximum-length
strings, numeric boundaries, malformed UTF-8 and exact buffer limits—not just a
normal snapshot. Never put a partially serialized object on the MIDI bus.

## 4. Publish state, events and timing with distinct meanings

| Data | What to publish | Why |
| --- | --- | --- |
| Full state | Parameters, selected/queued objects, ranges, masks, mute/bypass/focus | A receiver can attach mid-performance and recover after missed events. |
| Clock | Transport flag, tempo, tick count, phase and explicit phase denominator | A display can synchronize without confusing host time with engine time. |
| Playback | Actual source position, direction, window and active/tail/gain state | Selected slices and audible voices are not always the same thing. |
| UI | Held controls, LED masks, status text and remaining duration | Reconstructs feedback that cannot be inferred from musical parameters alone. |
| Input events | Press and release edges, encoder deltas, capture timestamp | Preserves gestures between snapshots without replaying them as musical commands. |
| Diagnostics | Sequence, device timestamp, drop counter | Exposes discontinuities and overload instead of silently hiding them. |

Snapshots are authoritative. Events are useful for animation and diagnostics;
replaying them is not a substitute for reconstructing state from snapshots.
A “loop selected” field is different from “loop currently sounding.” Likewise,
muted, stopped, auditioning and release-tail states are different. Do not label
an unmuted deck “audible” solely from its mute flag.

Document every unit, index base, range and sentinel. Beets uses zero-based IDs,
`-1` for no queued selection, integer `0`/`1` flags, positions in millionths of a
source sample, and a clock-phase denominator of `2646000`. Lua array indexing is
one-based even though lane IDs in the objects are zero-based. Read the
[complete wire schema](TELEMETRY.md) rather than inferring semantics from names.

Keep transient visualization getters separate from persisted settings. Adding a
playhead to a saved struct can accidentally change save-format compatibility.
Capture related engine fields under one short lock, then unlock **before** JSON
formatting and transmission. Do not serialize, allocate, print or wait for USB in
the audio interrupt. If the snapshot grows large, consider a double-buffered
publication design and measure the remaining interrupt exclusion time.

## 5. Use musical cadence with explicit overload behavior

Beets emits snapshots at observed sixteenth-note boundaries while playing, on
observed changes, and at one-second stopped heartbeats when transport permits.
A newly connected receiver gets a full snapshot. Heartbeats let an idle display
distinguish “stopped” from “disconnected.” This is best-effort observation, not a
sample-accurate recording of every boundary.

At 160 BPM, a sixteenth is 93.75 ms. At 220 BPM, approximately 14.7 snapshots per
second × 1500 bytes is about 22 KB/s of SysEx before USB packet overhead and input
messages. This is a **USB** budget; do not blindly forward it to DIN MIDI or BLE.
Measure worst-case payloads, audio behavior and coexistence with other protocols
on the actual device. Reduce the rate or split static metadata from dynamic state
if your schema grows; do not assume unused USB bandwidth means free CPU time.

The transport contract should be `accepted whole frame` or `accepted nothing`.
Our `plat_sysex_try_send` returns 0 or -1 accordingly. Do not feed a long SysEx
through a small ordinary-MIDI ring, or assume the old blocking SysEx sender is
suitable for periodic telemetry. Other protocol replies must not be inserted
inside an unfinished frame. Queue-capacity checks and publication must also be
safe against a USB reset. Review this against the pinned platform implementation.

Beets' concrete policy is:

- A fixed 32-entry FIFO preserves observed input edges and deltas.
- On would-block, retain the queued event and retry without consuming a sequence.
- Coalesce state into a fresh snapshot rather than queueing obsolete snapshots.
- Give state priority after 100 ms under sustained input, provided output accepts it.
- Drop new input events on FIFO overflow and increment `input_dropped`.
- Purge stale queued input on disconnect/link change and count that loss.
- Do not record input while disconnected; reconnect with authoritative state.

The 3072-byte frame bound is supported by an app-selected `SXQ` of 1024 USB MIDI
packets. It costs static RAM. Recalculate capacity, RAM and enqueue timing when
changing these values; do not copy the number without checking your platform.

Sequence increments mean acceptance for transmission, **not delivery acknowledgement**.
Beets has no boot UUID: sequence and milliseconds wrap at 32 bits, sequence resets
on initialization, and persists through a USB reconnect in the same boot. Use
unsigned/modular arithmetic and freshness checks. For a more robust multi-device
product, add explicit device/session identity in a future version. A lower
sequence alone is not a reliable reboot detector.

## 6. Build TouchOSC around a document-root receiver

Use current-generation TouchOSC, not TouchOSC Mk1. Put framing, decoding and model
updates in the document root; map the decoded model to stable, named controls.
Keep one parser rather than duplicating parsing code across widgets. Return
`true` for recognized messages you handle; leave unrelated MIDI alone.

Our exact header is `BEETS`, not the illustrative `FM` header. The minimum flow is:

```lua
local header = {0xF0,0x7D,0x42,0x45,0x45,0x54,0x53,0x01}
function onReceiveMIDI(message, connections)
  if #message < #header + 2 then return end
  for i = 1, #header do
    if message[i] ~= header[i] then return end
  end
  if #message > 3072 or message[#message] ~= 0xF7 then return true end
  local chars = {}
  for i = #header + 1, #message - 1 do
    local b = message[i]
    if b < 32 or b > 126 then return true end
    chars[#chars + 1] = string.char(b)
  end
  local data = json.toTable(table.concat(chars))
  if type(data) ~= 'table' or data.v ~= 1 then return true end
  -- Validate the required schema, then update the model and named controls.
  return true
end
```

This is framing scaffolding, not complete schema validation. The working
[root script](../touchosc/beets.lua) and [layout](../touchosc/Beets.tosc) show the
mapping. Validate nested tables and numeric fields before indexing, formatting or
arithmetic. Clamp normalized display values. Ignore unknown additive fields;
reject incompatible versions. A general-purpose receiver should validate every
required type/range and exercise malformed JSON in the real runtime. The current
prototype targets the known Beets producer and is not a hardened arbitrary-input
parser.

**Do not copy desktop Lua assumptions into TouchOSC.** We encountered a runtime
failure using `pcall(json.toTable, ...)`: TouchOSC 1.5.3.266 has no `pcall`. Its
[documented Lua subset](https://hexler.net/touchosc/manual/script-functions-lua)
excludes that function. The working prototype calls the native JSON decoder and
checks its return value. A mocked decoder test does not prove the native parser's
behavior for every malformed payload; test that separately before broadening the
input contract. Do not add a fake `pcall` that pretends to catch exceptions.

Use `control.values.text` for label contents, and properties such as `color` and
`frame` for appearance and geometry. Cache named-control lookups. For a growing
layout, decode into a model in the callback and render changed fields in `update()`
at a bounded rate. The prototype renders on state receipt and uses `update()` for
freshness. These are distinct from sending MIDI: changing a value may trigger
configured mappings, so keep receive-only widgets free of outgoing messages.
See the [properties/values reference](https://hexler.net/touchosc/manual/script-properties-and-values).

Keep both decks visible, retain readable text contrast, and distinguish held
controls from latched LED state. Show queued changes explicitly. Freeze stale
playheads, mark the feed stale, and clear held-control indicators after a timeout;
do not leave a disconnected display looking live. Beets uses three seconds,
longer than its idle heartbeat. Message duration should expire locally even if no
new state arrives. Avoid speculative playhead interpolation until direction,
loop/window boundaries, stutter and audition behavior are correctly modeled.

## 7. Treat the layout as a build artifact

Store editable Lua, a reproducible layout generator, the generated `.tosc`, and
setup instructions together. Our [Python generator](../tools/build-touchosc.py)
uses standard-library XML and zlib with deterministic control IDs. The format was
checked against a document saved by the installed TouchOSC version: a zlib stream
containing `lexml` version 6 XML. Treat this as an observed file format, not a
promise of permanent vendor compatibility. Reopen generated files in TouchOSC
after generator changes and app upgrades.

Our file has its root script embedded. Users only need the `.tosc` file to run it;
the neighboring Lua file is for maintenance. Stable names are part of the script
contract. Check that every referenced control exists and that the embedded script
matches the maintained source. Labels do not necessarily render multiline text
as expected: inspect the actual layout, not just XML or a mock screenshot.

Close the document before regenerating it, then reopen from disk. TouchOSC does
not automatically replace an open document with externally changed contents.
Script updates to properties can mark a preview modified; discard those runtime
changes when closing unless you deliberately want to change the saved defaults.
Avoid overwriting freshly generated files with an older in-memory preview.

Configure one enabled MIDI connection with the FM1 as Receive Port and leave
Send Port empty for this visualizer. Use the actual enumerated port, not a guessed
product label: this Mac retained `X0X FM-1` despite the firmware's Beets identity.
Select the port independently on each Mac/iPad; distributing a layout does not
configure the user's local MIDI devices. See [TouchOSC MIDI connections](https://hexler.net/touchosc/manual/connections-midi).

## 8. Verify each layer, then test the complete path

| Layer | Useful checks | What passing does not prove |
| --- | --- | --- |
| Serializer | JSON parsing, ASCII-only payload, Unicode, bounds, integer extremes, canaries/sanitizers | USB delivery or TouchOSC decoding |
| Stream scheduler | Would-block retry, FIFO order/overflow, reconnect purge, heartbeat, time wrap, state deadline | Physical USB reset timing or interrupt latency |
| Native/WASM | Shared engine behavior, playback getters, native wire captures, regression tests | Actual hardware transport; WASM currently discards telemetry output |
| Lua replay | Actual callback with mocked controls, captured states, mapping, filtering, staleness | TouchOSC's native JSON decoder or MIDI connection |
| FM1 USB harness | Actual wire framing/JSON, sequence, Start/Stop, moving playheads, input edges, ACK coexistence | Audio quality or visual readability |
| TouchOSC live | Open generated file, inspect MIDI/script logs, observe transport and UI updates | iPad behavior unless also tested on iPad |

For this repository, from its root:

```sh
tools/setup-local.sh
# Optional hardware-monitor dependencies, using the project environment:
uv pip install --python .venv/bin/python mido python-rtmidi
tools/build-local.sh --all --release 0.2.0
tests/run-tests.sh --no-build
.venv/bin/python tools/build-touchosc.py
.venv/bin/python tools/monitor-midi.py --list
.venv/bin/python tools/monitor-midi.py --port 'X0X FM-1' --count 8 --timeout 12
# Requires matching telemetry firmware already installed on the connected FM1:
.venv/bin/python tools/smoke-usb-midi.py
.venv/bin/python tests/touchosc_receiver_test.py
```

The last test also requires a Lua interpreter and the capture generated by the
hardware smoke test. The firmware toolchain and Docker requirements remain those
in the [repository build guide](../README.md). Installing a package on hardware
is a separate step from compiling it; verify the running firmware identity.

The hardware harness temporarily operates transport, tempo and remote panel
controls, releases its held controls and restores tempo/transport. It never saves
settings. Run it while nobody is playing the instrument, not during a performance.
It uses the base's existing remote-panel protocol to exercise logical panel input;
that is not proof of the physical switch scanner. Include manual press/release
and unplug/replug checks in a release test.

Capture raw SysEx alongside decoded JSONL and a result summary. Keep evidence
linked to the firmware identity and app version. In the tested Mac run, the USB
harness passed with TouchOSC concurrently listening: 139 state messages, 6 input
messages, no sequence gaps/new input drops, and separate remote acknowledgements.
That supports the USB integration claim. It does not establish audio quality,
maximum interrupt latency, an exhaustive overload test, or iPad readiness.

## 9. Troubleshoot from the wire toward the screen

| Symptom | First checks |
| --- | --- |
| No telemetry in the monitor | Running firmware identity; port enumeration; USB configuration/power; heartbeat while stopped |
| Monitor works, TouchOSC sees no MIDI | Connection enabled; correct Receive Port; avoid configuring the same source twice |
| TouchOSC MIDI log has frames but UI is empty | Script log; correct full header; embedded script version; `pcall` incompatibility; schema/indexing |
| UI stays on an old design | Close and reopen the generated file; do not save an obsolete preview over it |
| Sequence discontinuities | Restart/reconnect context, duplicate routes, consumer overload; compare raw capture and drop counter |
| Input animation sticks | Release edges, authoritative held masks, stale timeout, FIFO overflow |
| Playhead looks plausible but wrong | Source position versus slice number; reverse/window/stutter semantics; stale interpolation |
| Audio glitches when visualizer attaches | Blocking send path, work in audio ISR, lock duration, full-frame serialization rate |

For adoption, start with one heartbeat containing one real engine parameter,
prove it in TouchOSC, then add the full schema and musical cadence. Preserve the
same separation as the system grows: the engine owns truth, the stream is bounded
and lossy by design, and the display makes freshness and loss visible.

## Measure observation overhead

Check connection and scheduling deadlines before capturing full state. A tiny clock snapshot can decide whether a musical update is due. Bound retries after backpressure; repeated serialization in a busy loop wastes more work than the normal stream. Start with a short retry interval before introducing cached frames or readiness APIs. See the [measured Beets optimization](TELEMETRY-PERFORMANCE.md), including disabled-mode comparison and timing limitations.
