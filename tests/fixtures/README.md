# Beets boundary contract fixtures

`beets_contract.h` contains hand-derived expected decisions, not captured output
from the C engine. `contract_test.c` applies them through public engine calls.
Run `tests/run-tests.sh --no-build`; results are in `app/build/tests/contract.log`.
No Lua runtime, test DSL, or second sequencer implementation is required.

Reference: upstream Beets commit
[`226c2b892658bf7418555677a6a5aeab6fb3cfc6`](https://github.com/mattb/beets/tree/226c2b892658bf7418555677a6a5aeab6fb3cfc6).
These fixtures compare **boundary decisions**, not identical launch timing:
Norns receives an external clock callback; FM1 fires its first downbeat on Start.
Slices are zero-based in both implementations. Loop indices are normalized from
Norns' one-based indexing to the port's zero-based indexing.

| Fixture | Source rule and derivation |
|---|---|
| Natural | `calculate_next_slice` advances once; step zero resets to first eligible slice. |
| Skip | `jump` advances a second time: 0,2,4,6,0… |
| Repeat | `jump_back` moves back after forward advance: 0,0,0… |
| Skip then repeat | Forward, extra forward, backward gives one net step. |
| Sparse range | `step_forward` scans enabled slices in 2…6 (2,4,6). At the next downbeat, 4 resets to 2 rather than advancing to 6. |
| Auto-next | Range 0…1 wraps every second boundary, advancing the loop. On boundary eight, that advance happens before the downbeat reset. Auto is enabled after the initial boundary. |

These rules are in
[`step_forward`, `step_backward`, `step_first`, `calculate_next_slice`](https://github.com/mattb/beets/blob/226c2b892658bf7418555677a6a5aeab6fb3cfc6/lib/libbeets.lua#L267-L334).
Probabilities use only 0%/100%; this does not assert equivalence of Lua and C RNGs.
Lane B remains natural in each trace to check lane independence. The source has
per-loop slice masks; the port uses per-deck masks. The fixtures hold eligibility
constant so that difference does not masquerade as parity.

The final explicit command trace in `contract_test.c` is an **FM1 adaptation**:
while running, select loop 7 and cue slice 5 before a downbeat; nothing changes
until that boundary; both requests override auto/random switching and anchoring
once; random switching resumes on the next boundary. The eligible random loop
is fixed at 2 to avoid depending on a particular RNG sequence. This is a priority
test, not a typical user gesture or a claim about upstream behavior.

Existing `engine_test.c` and `telemetry_playback_test.c` retain reverse/stutter,
render-partition, audio-bound and position checks. These fixtures do not establish
Softcut audio equivalence, physical control timing, or DSP quality. See the
[current verification record](../../docs/VERIFICATION.md) for evidence and gaps.
