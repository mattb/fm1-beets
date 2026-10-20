/* SPDX-License-Identifier: MIT
 * Beets FM-1 engine, derived from Matt Biddulph's Beets (2020). */
#pragma once
#include <stdint.h>
#define BEETS_LANES 2
#define BEETS_LOOPS 10
#define BEETS_SLICES 8
#define BEETS_PROBS 5
enum { BEETS_REPEAT, BEETS_SKIP, BEETS_REVERSE, BEETS_STUTTER, BEETS_SWITCH };
typedef struct {
    int loop, queued_loop, slice, queued_slice, first, last;
    int muted, bypass, auto_next, force_reverse, force_stutter;
    uint32_t loop_mask, slice_mask;
    int probability[BEETS_PROBS];
    uint32_t events;
} BeetsLaneState;
typedef struct {
    int tempo, blend, playing, step;
    uint32_t frames, ticks, rng;
    BeetsLaneState lane[BEETS_LANES];
} BeetsState;
/* Main-loop setters/snapshot must be enclosed in plat_lock/plat_unlock on device.
 * Audio callback is allocation-free and never accesses IO. Loop/range/cue changes
 * are applied at the next eighth-note boundary while running; stopped cues audition. */
void engine_init(uint32_t seed);
void engine_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
void engine_snapshot(BeetsState *state);
void engine_play(int on);
void engine_panic(void); /* immediate silence, including stopped auditions */
void engine_tempo(int bpm);
void engine_blend(int percent);
void engine_loop(int lane, int loop);
void engine_range(int lane, int first, int last);
void engine_trigger(int lane, int slice);
void engine_probability(int lane, int kind, int percent);
void engine_mute(int lane, int muted);
void engine_bypass(int lane, int bypass);
void engine_force(int lane, int reverse, int stutter);
void engine_masks(int lane, uint32_t slice_mask, uint32_t loop_mask);
void engine_auto(int lane, int on);
const char *engine_loop_name(int loop);
