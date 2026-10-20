# Source audio and behavior

Original Beets: https://github.com/mattb/beets
Pinned source commit: 226c2b892658bf7418555677a6a5aeab6fb3cfc6
Beets source is MIT, Copyright (c) 2020 Matt Biddulph; see BEETS-LICENSE.
The original README thanks https://llllllll.co/u/vcvcvc_val for the demo loops.
Original WAVs and their metadata are preserved unmodified under demo-loops/.

prepare_samples.py converts these actual loops to 22050-Hz mono G.711 mu-law
for flash residency on FM-1. All three targets use exactly the same generated
sample data; no substitute synthesized drum sounds are used. Audio conversion
is lossy (11-kHz Nyquist limit and 8-bit logarithmic quantization).

The FM-1 engine is a C adaptation of lib/libbeets.lua, not a bit-identical Norns
softcut implementation. It retains two synchronized eight-slice voices and the
five independent mutation probabilities. It adds bounded mask handling, seeded
randomness, click-suppressed transitions, quantized manual slice cues, stopped
slice auditions, and explicit loop selection taking priority over random/auto
selection for one boundary. Slice eligibility is per deck, rather than stored
separately for every loop. Original kick/snare annotations are retained in JSON;
they were used for the original optional Crow callbacks, not sample playback.
