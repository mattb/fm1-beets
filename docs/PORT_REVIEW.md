# Beets FM-1: independent playability and fidelity review

Historical review from the initial port, before device installation. For later
hardware evidence and current gaps, see [the verification record](VERIFICATION.md).

The C engine implements the musical core of Matt Biddulph's Norns Beets with controls adapted for FM-1. The source reference is `mattb/beets` commit `226c2b892658bf7418555677a6a5aeab6fb3cfc6`, principally `lib/libbeets.lua` and `beets.lua`. The upstream `docs-src` text is generic Docz starter content; it was not used as instrument documentation.

## Musical behavior retained

- Two independently configurable lanes of eight slices, sharing a tempo and eighth-note clock. Each lane selects slices within its loop.
- Natural forward slice order, anchored to the range start at the beginning of each eight-step bar.
- Independent repeat, skip, reverse, stutter and loop-switch probabilities. Repeat cancels one forward step; skip adds one. Their combination and bar anchoring follow the original order of operations.
- Stutter lengths of one through four sixty-fourths of the complete loop; reverse proceeds backward from the slice onset, wrapping at the full-loop or stutter boundary.
- Per-deck mute, sample selection, slice range, sample/slice eligibility, automatic sample advance and a way to bypass mutations without erasing the probability settings.
- All ten original demo WAV files are preserved in `assets/source/demo-loops`; hashes identify the exact inputs.

## Deliberate adaptations and omissions

- White keys are permanently split into A1-A8 and B1-B8. OCT- / OCT+ select the deck edited by knobs and black sample keys. Playing a white key on the other deck does not silently redirect those knobs.
- A two-key chord sets a persistent slice range. LFO explicitly locks one slice; HOME restores the full range. The original double-tap lock was removed because repeated performance taps could accidentally trap playback.
- Running cues and sample choices are quantized to the next eighth note. An explicit sample choice wins over automatic/random sample changes on that boundary. A stopped pad auditions one eighth note; stopped sample selection alone does not start playback.
- Held ENV / ARP apply reverse / stutter to the deck selected when pressed, and clear on release even if focus changes. They do not rewrite the saved probability values.
- One sample bank contains all ten demos. Eligibility is per deck in this port; the original kept slice eligibility separately for every loop. User sample loading, sample/beat-marker editing, Norns parameter pages, crow outputs, arc integration and softcut's filter controls are not implemented.
- The app handles USB/TRS MIDI notes and transport messages. External MIDI clock synchronization was omitted in this initial review; [firmware 0.3.0 adds it](MIDI-CLOCK.md). Physical MIDI and USB-audio behavior had not been verified at the time of this review.
- The full 48 kHz PCM16 collection cannot fit in the FM-1 app slot. All targets use the same 22.05 kHz mono G.711 μ-law pack (328,722 bytes), expanded and linearly interpolated during playback. This is lossy: bandwidth is reduced, then amplitudes are companded. Measured μ-law quantization SNR is at least 37.42 dB relative to the already-resampled PCM; that number does not measure the loss from resampling or claim transparency to the originals.
- The output remains stereo-format dual mono. A constant-sum A/B blend, fixed headroom and short transition fades replace softcut's mixing/fades. The rendered audio differs from softcut.
- SAVE is available while stopped, avoiding synchronous flash writes during playback. Restart restores configuration while leaving transport stopped; it does not resume a performance at an old playhead position.

## Checks and evidence

Run `tests/run-tests.sh` to rebuild host and WASM and run all checks, or `tests/run-tests.sh --no-build` to test existing artifacts. Firmware compilation is separately performed by `tools/build-local.sh` / `--all`.

- `tests/sample_pack_test.py` verifies every original WAV SHA-256, packed asset hash and length, preserved loop duration (within one output sample), and independent μ-law expansion against resampled PCM.
- `tests/engine_test.c`, compiled with AddressSanitizer and UndefinedBehaviorSanitizer, checks natural order/bar reset; repeat/skip at 100%; combined probabilities; reversed and single-slice ranges; sparse and all-zero eligibility masks; explicit sample choice with random switching and automatic advance enabled; all ten samples at tempo extremes with forced reverse/stutter; bounded nonzero two-deck audio; exact seeded output across 128/257-frame partitions; silence after the stop fade; a shorter sample selected while the previous audition tail is fading; idempotent stop canceling stopped auditions; and immediate hard silence on update panic from both running and stopped audition states.
- `app/tests/basic.txt` drives real mapped host controls. It covers each encoder, separate white-key banks, eligibility modifiers, deck focus, independent mutes, momentary effects across focus changes, explicit lock/recovery, repeated taps that must not lock, stopped SAVE/reboot, running SAVE refusal, MIDI audition, and actual rendered sample audio.
- `tests/wasm-smoke.mjs` instantiates the actual built WASM in fresh runtimes. It tests all ten black-key sample choices for nonzero bounded audio, screen updates, two-deck/mutation/range performance, stop silence, fresh-state output equality across 128/256-frame quanta, and saved tempo/sample restoration against an independently configured instance on the same input/audio timeline.
- Evidence under `app/build/tests/`: test logs, `sample-pack.json`, `wasm-smoke.json`, `wasm-performance.wav`, and native `host/performance.wav` plus boot/configured/playing screenshots. Generated evidence is intentionally outside source control.

The 240 x 240 native screenshot was inspected: both decks, playheads, selected editing deck, probability values, blend and HOME recovery remain visible together. The browser wrapper provides the physical legend and controls; browser interaction validation is recorded by the integrating agent.

## Hardware checks outstanding at the time of review

This review covered source and simulator tests. Realtime CPU headroom, touch reliability, screen refresh cost, DAC audio quality and electrical MIDI/USB operation still required hardware tests. The first device session should verify simultaneous two-deck playback at 220 BPM with reverse/stutter, rapid range/sample changes, mute/unmute and MASTER motion, followed by stopped SAVE and power-cycle recovery. Listen for transition clicks and timing instability and measure underruns before treating it as performance-ready hardware.

## Final automated run

The final `tests/run-tests.sh --no-build` run completed successfully against the frozen app/engine builds. The WASM artifact was 374,028 bytes. All ten sample selections produced nonzero bounded audio; the five-second WASM performance peaked at 0.338962 full scale and produced 1,000 framebuffer blits. The native scenario completed 16,376 simulated milliseconds with 2,565 blits and a recorded peak of −18.3 dBFS at the test's MASTER setting. The full log is `app/build/tests/full-suite.log`; it includes the sanitizer engine, sourcepack, host-control/persistence and WASM checks.
