# Beets USB MIDI telemetry, version 1

Beets publishes its instrument and panel state as ASCII JSON inside SysEx on its
class-compliant USB MIDI connection. This is a read-only visualizer feed: this
protocol defines **no incoming control commands**. A receiver can draw both decks,
playheads, probabilities, held controls, and status messages without reproducing
the FM1's 240×240 screen. A receive-only TouchOSC prototype is included at `touchosc/Beets.tosc`; see `touchosc/README.md`.

## Wire format

Each complete MIDI SysEx message contains exactly one JSON object:

```text
F0 7D 42 45 45 54 53 01 <ASCII JSON> F7
   |  B  E  E  T  S  |               |
   |                 version 1       end of SysEx
   non-commercial / experimental ID
```

The version byte is binary `01`, not the character `1`. `7D` is an experimental
namespace, not a Beets manufacturer allocation; match the entire signature.
JSON bytes are printable ASCII (`20`–`7E`); strings use JSON escapes for control
characters and non-ASCII text. There is **no LF, NUL, or length prefix on the
wire**. The current maximum complete frame is 3072 bytes. The serializer uses
vendored jems, a bounded buffer, and no allocation. If serialization fails, no
partial message is sent. The MIDI transport accepts a whole frame or defers it.

MIDI APIs can split SysEx across transport packets. Reassemble through `F7` before
parsing; never treat a USB packet or native callback as a JSON message boundary.
Mido already assembles a `sysex` message and excludes `F0`/`F7` from its `data`.
Ignore unrelated signatures and unsupported versions. Treat unknown JSON keys as
extensions; validate required fields before drawing. All identifiers and bit
positions below are **zero-based**, regardless of the screen's human labels.
Flags are integer `0`/`1`, not JSON booleans.

## Emission and reconnect behavior

A full `state` snapshot is sent on connection, after observed input/state changes,
at each observed sixteenth-note boundary while playing, and at least once a
second while stopped when USB can accept it. `input` messages carry observed
press/release edges and encoder deltas. Inputs use a 32-entry FIFO; a state
snapshot takes priority after 100 ms so sustained input cannot starve it.
Serialization and USB output run outside the audio interrupt.

This is best-effort telemetry, not a sample-accurate event log. Backpressure holds
queued inputs and coalesces snapshots. FIFO overflow and stale queued inputs
purged on disconnect increase `ui.input_dropped`; inputs while disconnected are
not recorded. Timing guarantees depend on the transport accepting a frame.
Receivers should use snapshots as authoritative, with input messages only for
short-lived gestures/animation. Do not reconstruct instrument state by replaying
input messages: the firmware remains the source of truth.

`seq` is a shared unsigned 32-bit sequence for both message types, incremented
when a frame is accepted for transmission. A blocked attempt does not consume a
sequence number. It wraps modulo 2³², persists across USB reconnect during the
same boot, and resets at firmware initialization. `ms` is a wrapping unsigned
32-bit device millisecond timestamp (input capture time for `input`, snapshot
time for `state`). There is no boot/session UUID. On reconnect, clear local held
control/animation state and wait for a full snapshot. Do not interpret a lower
sequence alone as a restart: use unsigned arithmetic and connection context.
Sequence gaps and `input_dropped` are different signals; neither guarantees
exactly-once delivery. Stop interpolation and show a disconnected/stale indicator
if fresh snapshots stop arriving.

## JSON schema

Both message types contain `v:1`, `type`, `seq`, and `ms`.

### State

| Field | Meaning |
| --- | --- |
| `tempo`, `blend`, `playing` | BPM (60–220), deck B mix percentage (0–100), transport flag. |
| `step` | Current eighth-note step, 0–7. |
| `frames` | Total rendered stereo sample frames, including stopped time; uint32 wraps. |
| `ticks` | Cumulative eighth-note triggers; uint32 wraps. Start triggers an eighth immediately. |
| `rng` | Engine's current random-generator state; diagnostic, not a UI control. |
| `clock_phase` | Integer numerator of position within the current eighth; denominator **2646000**. |
| `lanes` | Two objects, ordered A then B, described below. |
| `playheads` | Two actual playback-position objects, ordered A then B. |
| `ui` | Panel/display state, described below. |

The normalized eighth phase is `clock_phase / 2646000`; its first/second half
corresponds to the two sixteenths. This is audio-engine time, not host wall time
or MIDI Clock. Transport start resets the phase and `step`; `ticks` is cumulative.
A visualizer may interpolate between snapshots using tempo while playing, then
resynchronize on each snapshot. Do not advance the phase while stopped.

Each `lanes` object contains:

| Field | Meaning |
| --- | --- |
| `lane`, `name` | 0 / `"A"`, or 1 / `"B"`. |
| `loop`, `loop_name` | Current loop ID 0–9 and display name (`BEETS 1`…`BEETS 8`, `AMEN 1`, `AMEN 2`). |
| `queued_loop`, `queued_loop_name` | Pending loop selection; `-1` and `""` mean none. |
| `slice`, `queued_slice` | Current/pending slice 0–7; queued `-1` means none. |
| `first`, `last` | Inclusive selected slice range, each 0–7. |
| `muted`, `bypass`, `auto_next` | Deck mute, probability bypass, automatic next-loop flags. |
| `force_reverse`, `force_stutter` | Held performance effects; distinct from saved probabilities. |
| `loop_mask`, `slice_mask` | Eligibility bitsets: bit N selects loop/slice N. |
| `events` | Mutation flags for the current trigger: bits 0 repeat, 1 skip, 2 reverse, 3 stutter, 4 switch. |
| `probability` | Object with `repeat`, `skip`, `reverse`, `stutter`, `switch`, each 0–100. |

Each `playheads` object contains `lane`, `sounding_loop`,
`sounding_loop_name`, `position_ppm`, `direction` (+1/-1), `active`, `tail_active`,
`audition_remaining`, `window_start_ppm`, `window_end_ppm`, and `gain_ppm`.
Positions and window bounds are normalized to the entire source sample:
0–1000000 represents 0–100%. `gain_ppm` is the actual smoothed mute envelope
on the same millionths scale. `active` means transport running or audition frames
remaining; a muted deck can therefore be active with zero gain. `tail_active`
indicates an inactive voice with a remaining release envelope. Audition duration
is measured in 44.1 kHz audio frames. `sounding_loop` can differ from the selected
lane loop while a stopped selection's previous sound releases. Use this object
for sample-position animation; use `lanes` for selected controls and sequencing.

`ui` contains:

| Field | Meaning |
| --- | --- |
| `focus`, `dirty` | Focus deck 0/1; unsaved setup flag. |
| `keys`, `buttons` | Current held physical-control bitsets. |
| `frame_ms` | UI animation time in milliseconds. |
| `message`, `message_remaining_ms` | Temporary status text and remaining display duration. |
| `master` | Master level, 0–4096. |
| `lights_buttons`, `lights_keys` | Lit physical LED bitsets. |
| `dim_buttons`, `dim_keys` | Dim physical LED bitsets. |
| `input_dropped` | Cumulative discarded queued input records, uint32. |

### Input

`type:"input"` contains `keys_down`, `keys_up`, `buttons_down`, `buttons_up`
(bitsets), and `enc` (seven signed encoder deltas). Several changes can be present
in one record. An edge describes physical input observed by the main loop, not
the moment a quantized cue becomes audible. A snapshot reflects engine state.

Encoder array order is `SELECT, ALGO, PRESET, K1, K2, K3, K4`. In Beets these are
tempo, switch probability, blend, repeat, skip, reverse, and stutter.

Button bit positions 0–13 are `FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ,
PLAY, REC, OCT−, OCT+`. Button-light bit 14 is the green PLAY indicator and is not
an additional input button. Physical key bit positions are chromatic: white keys
left to right are `[0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26]`; black keys are
`[1,3,5,8,10,13,15,17,20,22,25]`. The first eight whites address A and the next
eight B. See [the playing guide](../app/docs/MANUAL.md) for performance gestures.

## Capture on a Mac

Install the optional receiver dependencies in the project's environment:

```sh
uv pip install --python .venv/bin/python mido python-rtmidi
.venv/bin/python tools/monitor-midi.py --list
.venv/bin/python tools/monitor-midi.py --port "Beets" --count 64 --timeout 15 > capture.jsonl
```

Use the actual name printed by `--list`; `--port` accepts an exact name or a
unique case-insensitive substring. Without it, the receiver selects a unique
Beets-named input or the sole available input. It refuses ambiguous selection.
The receiver opens only an input port. It filters and validates Beets v1 framing,
printable ASCII, strict JSON, type/version, and unsigned `seq`/`ms`; it is not a
complete schema validator. Valid objects go to stdout, one per line. **Those
newlines are added by the receiver, not transmitted by firmware.** Diagnostics
go to stderr. `--list` prints port names instead of JSONL.

`--count 0` and `--timeout 0` mean unlimited. Timeout is an overall capture limit,
not an idle timeout. Exit status is 0 after reaching count (or a timed capture
with any frames and no requested count), 2 if timeout yields no frames or fewer
than the requested count, 1 on setup/backend error, and 130 on Ctrl-C. Reopen the
receiver after a device disconnect; it does not auto-reconnect.

The local build wrapper applies `patches/fm1-base-telemetry.patch` to the pinned
upstream base to provide atomic, nonblocking SysEx output and MIDI-ring guards.
For host-only integration capture, run the native app with
`FM1B_SYSEX_LOG=/absolute/path/capture.syx`; this file contains concatenated raw
SysEx frames, not JSONL. Split on complete `F0`…`F7` frames and remove those two
bytes before passing each payload to `parse_payload` in the monitor module.
This host log validates application output independently of USB hardware.

A hardware acceptance capture should demonstrate: initial full snapshot; stopped
heartbeat; play/stop; changing both deck controls; down and up edges; held effects;
queued cue becoming current; reconnect with a fresh snapshot; and sensible drop
reporting under load. A successful host test or build does not establish that a
physical FM1, adapter, or iPad has passed this test.

## TouchOSC and custom visualizers

TouchOSC's documented document-root `onReceiveMIDI(message, connections)` callback
receives a raw byte list and runs before child routing. Returning `true` consumes
the message. Its JSON support includes `json.toTable(string)` (since 1.1.8.157).
These APIs support a direct native TouchOSC receiver; they do not require OSC
translation. [TouchOSC callback manual](https://hexler.net/touchosc/manual/complete),
[JSON API manual](https://hexler.net/pub/touchosc/touchosc-manual.pdf).

Enable a TouchOSC MIDI connection and choose the FM1's port as Receive Port. Leave
Send Port unset for a receive-only layout. [TouchOSC MIDI connections](https://hexler.net/touchosc/manual/connections-midi).
This document-root script is a starting receiver, not a finished layout:

```lua
local state = nil
local lastInput = nil
local signature = {0xF0, 0x7D, 0x42, 0x45, 0x45, 0x54, 0x53, 0x01}

function onReceiveMIDI(message, connections)
  if #message < 10 or #message > 3072 then return end
  for i = 1, #signature do
    if message[i] ~= signature[i] then return end
  end
  if message[#message] ~= 0xF7 then return true end
  local chars = {}
  for i = 9, #message - 1 do
    if message[i] < 0x20 or message[i] > 0x7E then return true end
    chars[#chars + 1] = string.char(message[i])
  end
  local value = json.toTable(table.concat(chars))
  if type(value) == 'table' and value.v == 1 then
    if value.type == 'state' then state = value end
    if value.type == 'input' then lastInput = value end
  end
  return true
end
-- A future update() can draw controls from state and animate lastInput.
-- Validate fields before use; clear cached state on reconnect/staleness.
```

For a custom Mac/iPad application, use a native CoreMIDI input and reassemble the
MIDI 1.0 SysEx stream before parsing JSON. Hand decoded snapshots to the UI thread;
avoid doing drawing or expensive JSON work in the MIDI callback. Apple's
[CoreMIDI input API](https://developer.apple.com/documentation/coremidi/midiinputportcreatewithblock(_:_:_:_:))
provides the native receive path. A native app can also bridge snapshots to a
web view for rendering; this design does **not** assume Safari Web MIDI support.
The iPad needs a compatible USB host connection and sufficient device power.
The included TouchOSC prototype has been verified on macOS with the physical FM1.
Receiving on iPad remains unverified; see `touchosc/README.md` for setup and test scope.

## Hardware smoke test

Run `.venv/bin/python tools/smoke-usb-midi.py --port 'X0X FM-1'` with the
telemetry firmware installed. This drives MIDI Start/Stop, notes, and the base
firmware's remote panel protocol while independently decoding the received USB
SysEx. It checks ASCII JSON, sequence continuity, advancing clocks/playheads,
encoder changes, held reverse, key/button edges, drop counters, and coexistence
with remote acknowledgements. It restores tempo and transport and releases all
remote-held controls; it does not save settings. Do not operate the panel during
the test. Raw SysEx, JSONL and a result summary go to `build/tests/usb-telemetry.*`.

On 2026-10-28, the physical FM1 running `FM-1_731000200` passed: 139 state frames,
6 input frames, maximum 1486-byte frame, zero sequence gaps, no new input drops,
and 8 separate remote acknowledgements. Firmware/native/WASM builds and the
full automated suite also passed. This verifies the USB feed and engine state;
it is not an audio-quality measurement or an iPad/TouchOSC execution test.
