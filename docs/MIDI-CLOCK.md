# MIDI clock

Beets 0.3.0 accepts MIDI clock and Start/Continue/Stop over USB and the FM1's TRS
MIDI input. It sends clock and transport over class-compliant USB MIDI. The FM1
has no TRS MIDI output; use a USB MIDI host with a MIDI output to drive DIN/TRS gear.

## Playing with other instruments

The first USB or TRS clock/transport message selects that input. Beets displays
`USB CLOCK` or `TRS CLOCK`; another source is ignored while that source is active.
MIDI clock has 24 pulses per quarter note, so twelve pulses advance one Beets
eighth-note step. Start resets playback on the next pulse, Continue resumes it,
and Stop stops playback. Clock alone does not start a stopped instrument.
The tempo knob is ignored while externally clocked. The displayed tempo and
sample playback rate follow a twelve-pulse estimate, within Beets' 60–220 BPM range.

After 500 ms without clock or transport from the selected source, Beets stops and
returns to `INT CLOCK`. Press PLAY to resume with its internal clock. Incoming
clock continues to own timing while stopped if the sender keeps sending pulses.
Panel PLAY can arm a fresh start on the next incoming pulse.

Internal playback sends Start followed by clock at 24 PPQN, then Stop when stopped.
TRS clock/transport is forwarded to USB. USB input is never echoed back to USB,
following fm1-base's source-mask routing convention. Song Position Pointer, MIDI
Time Code, Link and BLE clock are not supported by this app.

## Eurorack analog clock

There is an FM1-oriented converter project: Oficina de Sonido's
[Expander](https://www.oficinadesonido.org/expander/) describes a working prototype
that converts analog sync at 1, 2, 4 or 24 PPQN to USB MIDI clock and offers TRS MIDI
outputs. Its page says schematics and firmware will be released when development
is finished. This is a potential external conversion route, not a tested Beets accessory.

We found no verified implementation that makes the FM1's own TRS input accept raw
Eurorack pulses. The current fm1-base driver reads serial MIDI through the input
circuit; it has no analog-clock input API. A firmware option alone would not
establish compatible voltage thresholds or electrical protection. Use a documented
analog-clock-to-MIDI converter with a compatible MIDI connection. Check its input
voltage range and pulse division against your Eurorack source. Do not treat a TRS
plug as evidence that a direct patch is electrically compatible.

## Verification

On 2026-10-28, production firmware `FM-1_731000300` passed the connected USB harness:
120 and 180 BPM input, Start/Stop/Continue, no USB echo, and stop/release after clock
loss. At 160 BPM, outgoing clock averaged 64.07 Hz (64 expected). Mac-observed
arrival gaps had a 17.22 ms median, 23.68 ms p95 and 36.13 ms maximum during normal
telemetry. These include USB scheduling and host polling; they are not wire-level
jitter measurements. Audio-render scheduling is block-based, so USB clock delivery
is not sample accurate.

Sanitized engine/controller tests cover 24 PPQN generation, external pulse-driven
steps, TRS forwarding, competing sources, timeout and timer wraparound. Physical
TRS input and an analog converter have not been tested. See the
[current verification record](VERIFICATION.md) for the other targets and limits.

```sh
tests/run-tests.sh --no-build
.venv/bin/python tools/smoke-midi-clock.py
```

The device harness temporarily operates transport and tempo, restores them, and
does not save settings. Run it with other clock senders disconnected.
