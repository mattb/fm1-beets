# Porting a Norns instrument to FM1

A Norns port needs an explicit account of what each gesture does, when it takes effect and how it sounds. Use that account to decide which features to preserve and which need adaptation for FM1. The Beets examples show how those decisions affected one instrument.

Platform references were reviewed on 2026-10-28. Check the pinned `fm1-base` revision and build configuration for each required service, then test its hardware connection.

## Establish the instrument contract

Pin the source revision and inspect the executable code, included libraries, engine files, sample metadata, and initialization/cleanup paths. Read documentation alongside code. If possible, record a reference session on Norns: gestures, state transitions, audio, synchronization, and save/reload behavior. Source inspection alone cannot establish audible equivalence.

List each feature as preserve, adapt, omit or investigate. Include default values, parameter units, ranges, timing, interaction priorities, and exceptional cases. Separate the instrument's essential musical behavior from convenient platform services. A live looper that cannot record may no longer be the same instrument; a fixed demo bank may be sufficient for a particular sample-based performance port.

Norns scripts can use a SuperCollider engine, Softcut, MIDI output, or combinations of these. Start by identifying which case applies. The [Norns introduction](https://monome.org/docs/norns/study-0/) explains the Lua/engine relationship; the [script reference](https://monome.org/docs/norns/reference/) indexes platform services and dependencies.

| Dependency | Assess before implementation | Porting decision |
|---|---|---|
| Lua and included libraries | State machine, algorithms, dynamic structures, external code | Translate the needed behavior; bound collections and work |
| Softcut | Playback, recording, overdub, routing, filters, cuts, buffer editing | Implement the required DSP subset |
| Custom SuperCollider engine | Graph, commands, plugins, polyphony, effects | Port DSP and voice lifecycle explicitly |
| `clock`, `metro`, polls | Musical scheduling, elapsed time, observation | Assign each to an appropriate clock or snapshot path |
| `params`, presets, files | Scaling, actions, load order, dynamic assets | Define parameters and persistence deliberately |
| Grid, arc, screen, keys | Simultaneous gestures, focus, feedback | Map the performance model to available controls |
| MIDI, crow, OSC, Link | Musical role and physical/network transport | Verify a replacement path or declare the omission |

> Beets example: the pinned [Lua entry point](https://github.com/mattb/beets/blob/226c2b892658bf7418555677a6a5aeab6fb3cfc6/beets.lua) and [instrument library](https://github.com/mattb/beets/blob/226c2b892658bf7418555677a6a5aeab6fb3cfc6/lib/libbeets.lua) use two Softcut voices, not a custom SuperCollider engine. The preserved core is two synchronized eight-slice decks and independent mutations. Arbitrary sample loading, filter pages, arc, and crow were omitted. Another port may need to implement those features.

## Prove feasibility before writing the whole instrument

Build an unchanged app template for firmware, native host, and WASM first. Then prototype the most demanding feature: the required recording duration, maximum voices, feedback/filter path, or external synchronization. A playback-only example is not evidence that a recording-heavy instrument fits.

Budget static memory, working buffers, stack, flash, asset storage, and worst-case audio work. For an uncompressed buffer:

```text
bytes = seconds × sample_rate × channels × bytes_per_sample
```

Include delay lines, overlapping playback heads, conversion buffers, and metadata. Distinguish immutable flash assets from writable audio RAM. Establish the actual audio input/output route before designing recording; neither an SDK API nor USB enumeration proves that the intended signal path works.

Keep the instrument in its own repository and pin the platform dependency. Own local build wrappers, assets, tests, and any required platform patches in that repository. Verify fresh-checkout builds. The Beets [dependency guide](DEPENDENCIES.md) demonstrates a submodule plus an optional external checkout; other dependency managers can provide the same pinning and override behavior.

## Choose a DSP replacement strategy

### Softcut-based instruments

Softcut is more than a sample player: it includes multiple voices, shared buffers, recording, routing, filters, smoothing, and cut behavior. The official [Softcut studies](https://monome.org/docs/norns/softcut/) explain those facilities and their sample-rate assumptions. Inventory the calls actually used before choosing a replacement.

For playback, specify source-frame addressing, interpolation, loop bounds, forward/reverse wrapping, seeks, retriggers, gain changes, and transitions. Distinguish time stretching from rate changes that also change pitch. A fade from the previous output value is not equivalent to overlapping playback heads; implement the transition behavior required by the sound.

For recording and overdub, additionally specify:

- Writable buffer size and ownership; how reads and writes interact at different rates.
- Input, existing-buffer, and feedback gains, including saturation or limiting behavior.
- Record/playhead alignment, wrap boundaries, and whether interpolation changes feedback behavior.
- Clear/copy/save operations and how to perform them without blocking audio.
- Storage stalls, missing input, full buffers, and what the user hears during failure.

Filters require their own assessment: topology, cutoff/resonance range, parameter smoothing, stability under modulation, and pre-record versus post-playback placement. A similarly named filter can sound different. For shared buffers and feedback routing, operation order is part of the sound.

> Beets example: [engine.c](../app/dsp/engine.c) uses flash samples, interpolation, and 96-sample transitions at 44.1 kHz. The source requests a 10 ms Softcut fade and different level smoothing. The port therefore preserves performance behavior without claiming sample-identical Softcut output. Its player is not a ready-made recording/overdub engine.

### SuperCollider-based instruments

A `SynthDef` describes a UGen graph whose nodes retain DSP state between samples. Inventory persistent DSP state, channel expansion, envelopes, node lifetime, voice allocation/stealing, buses, feedback, randomness, and third-party UGens. Preserve the external command meanings while implementing a smaller internal engine. See [UGens and Synths](https://doc.sccode.org/Guides/UGens-and-Synths.html) and the [Norns engine API](https://monome.org/docs/norns/api/modules/engine.html).

Separate audio-rate, control-rate, and initialization-time values. Recompute coefficients when appropriate and smooth controls where the sound requires it; moving every control-rate operation to the screen-refresh loop changes behavior. Do not assume SuperCollider's default block size is part of the instrument contract. See the official [rates tutorial](https://doc.sccode.org/Tutorials/Mark_Polishook_tutorial/08_Rates.html) and [Control reference](https://doc.sccode.org/Classes/Control.html).

Build one fixed-parameter voice first, then note lifecycle, modulation, bounded polyphony, and effects. Compare representative renders after each stage. If the sound depends on an unavailable plugin or expensive algorithm, make the approximation an explicit product decision and document its effect on the sound.

## Separate musical time, timers, and observation

Norns [`clock.sync()`](https://monome.org/docs/norns/reference/clock) schedules against beats, while `clock.sleep()` uses seconds. [`metro`](https://monome.org/docs/norns/api/modules/metro.html) provides timed callbacks; [`poll`](https://monome.org/docs/norns/api/modules/poll.html) receives observed values. Keep their scheduling and observation roles separate in the port.

For an audio-producing instrument, let rendered audio frames drive internal musical scheduling. Retain fractional phase/remainder across blocks and tempo changes. For a MIDI-only instrument, choose a suitably precise platform clock and output scheduling path; it need not acquire an artificial audio dependency. Use the chosen clock consistently for musical events.

Specify immediate versus quantized actions, transport restart, cue priorities, release tails, and panic behavior. Keep musical RNG independent from drawing and diagnostics. Test event timelines across different render block sizes or timer service intervals.

External sync is a separate requirement: implement and test phase tracking, tempo changes, clock loss, Start/Continue/Stop, and any supported song position. Norns' [clock integration](https://monome.org/docs/norns/control-clock/) does not transfer automatically to the new app. Accepting MIDI notes or Start/Stop does not prove MIDI-clock synchronization.

> Beets example: its eighth-note clock adds `BPM * 2` per rendered sample and crosses `44100 * 60`, retaining the remainder. Repeat/skip ordering and bar anchoring are tested independently of rendering block size. External MIDI clock and Link are not implemented.

## Keep the shared engine boundary small

Use a platform-independent engine where practical, with explicit inputs and bounded state. Keep hardware adaptation, drawing, storage, and communication around it:

```text
controls / MIDI -> validated commands -> musical state and DSP
                                              |
display / diagnostics <- coherent observations-+
```

Audio callbacks should not allocate, access files, serialize JSON, draw, wait for USB, or perform unbounded work. Validate external values before DSP. On a target where UI and audio share state, use the platform's documented synchronization mechanism and keep protected work short. Capture a coherent minimal state, then normalize, format, or transmit it outside the critical section.

Measure both average processing and the longest interruption of audio service. An audio-only CPU meter can miss costly main-loop code. Test the deployed device under maximum polyphony, rapid input, display work, storage activity where supported, and transport backpressure. Native and WASM performance help iteration but do not establish firmware headroom.

Start with direct setters and a small state structure. Introduce event queues, caches, or generalized parameter systems only for a concrete timing/correctness requirement or measured bottleneck. Preserve the simpler design when both satisfy the contract.

## Make parameters, files, and saved state explicit

The Norns [`paramset` API](https://monome.org/docs/norns/api/modules/paramset.html) includes values, actions, persistence, and action replay. Port both the values and the actions they trigger. Record units, scaling and limits for each parameter, along with its default and display format. Check whether restoring the value triggers an action. Apply dependent settings in a deliberate order.

Save configuration in a defined format. Version the format, validate restored values, and rebuild transient state. Usually held buttons, pointers, active tails, and queued gestures should not survive reboot. Decide explicitly whether a performance resumes. Keep flash/file writes out of real-time callbacks; if saves cannot safely run during playback, make that restriction clear.

Audit file services separately: directory scanning, file selection, arbitrary sample loading, sidecar metadata, recordings, presets, and shell/network helpers. A POSIX path or Lua `io` call is not a portable storage API. Choose among compiled assets, supported device storage, a host-assisted import flow, or an explicit omission. Define missing/corrupt-file behavior and maximum sizes before implementation.

For compiled sample banks, preserve originals and generate target assets reproducibly. Record source hashes, conversion settings/tool version, resulting sample rate, frame count, and encoding. Test durations and decoding independently. Do not mix source frames, converted frames, seconds, and shared-buffer offsets. Track code licenses and sample provenance separately.

> Beets example: [prepare_samples.py](../tools/prepare_samples.py) packs ten original WAVs into a shared 328,722-byte mono 22.05 kHz mu-law bank. The conversion saves flash at an audible fidelity cost. The port saves configuration while stopped and boots with transport stopped.

## Rebuild the interaction model for the available surface

List the most frequent gestures before assigning controls. Preserve simultaneous access where it matters, distinguish held and latched actions, show editing focus, and provide an obvious recovery action. Assign controls around those gestures and the available buttons, keys and knobs.

For grid input, inspect chords, release semantics, orientation, paging, and LED feedback. For arc, distinguish continuous relative motion from absolute values and decide how ring feedback translates. Norns' [device references](https://monome.org/docs/norns/reference/#devices) are a starting point; the app's actual gestures determine the port.

Capture an action's target when pressed if focus may change while it is held. Test release after focus/page changes, repeated rhythmic taps, all controls held, and disconnection. Avoid mode combinations that leave the performer unable to tell what a control will change. A remote display can help visibility, but decide whether the physical instrument must remain usable without it.

> Beets example: fixed white-key banks play the two decks; focus changes only shared editing controls. Held effects retain their original deck until release. A double-tap lock was replaced with a deliberate action because rhythmic repeated taps caused accidental locking. The full mapping belongs in the [player manual](../app/docs/MANUAL.md).

## Treat external interfaces as independent features

The source's [MIDI](https://monome.org/docs/norns/api/modules/midi.html) and [OSC](https://monome.org/docs/norns/api/modules/osc.html) APIs hide services that the port must assess explicitly. Identify message semantics, transport, latency, routing, and failure behavior.

| Interface | Decision to make and verify |
|---|---|
| MIDI notes / CC / pitch bend | Channels, scaling, note-off/velocity-zero handling, repeated notes, panic, reconnect |
| MIDI clock / transport | Source and direction, phase policy, missing-clock handling; separate from note support |
| Grid / arc hardware | Actual USB host or bridge route, input protocol, feedback, power; panel remapping is another option |
| crow / ii | CV/trigger roles, electrical route, timing, calibration; MIDI substitution changes the interface |
| OSC / network helpers | Required network service, address/schema, reconnect, whether a host bridge is acceptable |
| Link or Bluetooth | Explicit implementation and build support; do not infer either from Norns behavior |

USB MIDI device support does not by itself provide USB hosting for another controller. Likewise, a browser simulator or network-capable dependency does not prove the deployed firmware provides OSC, Link, CV, or Bluetooth. Mark each interface supported, adapted, omitted, or unverified.

## Add observation only at useful rates

Expose enough state to diagnose and render the instrument without creating a second sequencer in the UI. Distinguish requested state from actual sounding state. Use bounded snapshots for current state and bounded events when short gestures would otherwise disappear. Define reconnect recovery, queue overflow, stale data, and versioning.

Gate work before taking expensive snapshots; format outside audio-critical sections. Prefer coalescing obsolete visual state over accumulating a backlog. The [Softcut API](https://monome.org/docs/norns/api/modules/softcut.html) similarly advises careful phase-reporting granularity instead of excessive polling.

> Beets example: a versioned ASCII-JSON SysEx feed provides state and input events over USB MIDI. It is an optional app protocol. Its [protocol](TELEMETRY.md) and [TouchOSC adoption guide](TELEMETRY-BEST-PRACTICES.md) cover framing, bounded transport, and the receive-only display. TouchOSC testing found that its Lua sandbox lacks `pcall`; the Lua mock had missed that difference. macOS reception was verified; iPad needs separate validation.

## Validate in layers and record the limits

Use the same engine and asset representation across firmware, native host, and WASM unless divergence is intentional. Keep deterministic command traces and compare outcomes at multiple buffer sizes. Test probability extremes and operation ordering, not only typical settings. Add reference-audio comparisons where sound fidelity is a requirement.

| Layer | Useful evidence | Remaining question |
|---|---|---|
| Asset and engine tests | Hashes, lengths, bounds, state invariants, deterministic timing | Source fidelity and device behavior |
| Native app tests | Mapped inputs, persistence, rendered audio | Physical controls and embedded timing |
| Actual WASM/browser session | Built artifact, UI, audio, browser persistence | Firmware and peripherals |
| Firmware compile | Compiler compatibility, static memory/flash fit | Runtime success |
| Installed device | Queried image identity, exercised controls/audio/transport | Untested paths and worst-case load |
| Companion app/controller | Real transport and visible behavior | Other OS/device combinations |

Record revision, target, device identity, date, capture/log locations, and coverage gaps. Record build success separately from installation and performance tests. Historical reviews should retain their dates and link to a current acceptance record.

> Beets example: [PORT_REVIEW.md](PORT_REVIEW.md) records the original source-fidelity and test work; its hardware caveats predate later flashing and USB telemetry tests. The project subsequently verified physical installation and macOS TouchOSC reception. The later [telemetry performance report](TELEMETRY-PERFORMANCE.md) records measured device overhead and validation for production 0.2.3. Those results do not establish every audio/peripheral path or iPad compatibility. Its repository-specific entry points are `tools/build-local.sh --all` and `tests/run-tests.sh --no-build`.

The Beets [boundary fixtures](../tests/fixtures/README.md) and
[verification record](VERIFICATION.md) provide examples of maintaining tests
and evidence alongside the port.

## Keeping the port maintainable

Use a few source-derived gesture traces to check behavior, with intentional differences labelled alongside them. A small fixture can make a sequencing change reviewable without adding a Lua runtime.

Check whether an observation is due before capturing full state. Measure its processing cost and interrupt duration on hardware before adding caches or queues.

Keep a current feature/test table with evidence paths, and link older reviews to it. Extract shared playback or timing code when another instrument needs the same behavior. Recording, filters and external sync each need their own requirements before they become shared components.
