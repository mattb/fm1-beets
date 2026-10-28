/* SPDX-License-Identifier: MIT
 * Beets FM-1, based on lib/libbeets.lua by Matt Biddulph (2020).
 * Shared musical engine: two synchronized eight-slice loop voices, with the
 * original independent repeat/skip/reverse/stutter/loop-switch probabilities.
 */
#include "engine.h"
#include "beets_samples.h"

#define CLOCK_LIMIT 2646000u /* 44100 frames * 60; add BPM*2 each sample */
#define CROSSFADE 96u
#define FULL_SCALE 8388607.0f

typedef struct {
    float position, from, end, increment, last_output, fade_from, gain;
    uint32_t fade, audition;
    int direction, sample_index;
} Voice;
static BeetsState state;
static Voice voice[BEETS_LANES];
static uint32_t clock_phase;

static int bound(int n, int lo, int hi) { return n < lo ? lo : n > hi ? hi : n; }
static int valid(int lane) { return lane >= 0 && lane < BEETS_LANES; }
static uint32_t random_u32(void)
{
    uint32_t x = state.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    state.rng = x;
    return x;
}
static int chance(BeetsLaneState *s, int kind)
{
    return !s->bypass && (int)(random_u32() % 100u) < s->probability[kind];
}
static int eligible_loop(BeetsLaneState *s, int from, int direction)
{
    int i;
    for (i = 0; i < BEETS_LOOPS; i++) {
        from += direction;
        if (from >= BEETS_LOOPS) from = 0;
        if (from < 0) from = BEETS_LOOPS - 1;
        if (s->loop_mask & (1u << from)) return from;
    }
    return s->loop; /* bounded even if a corrupted caller clears all eligibility */
}
static int advance(BeetsLaneState *s, int from, int direction)
{
    int i;
    for (i = 0; i < BEETS_SLICES; i++) {
        from += direction;
        if (from > s->last || from < s->first) {
            from = direction > 0 ? s->first : s->last;
            if (s->auto_next) s->loop = eligible_loop(s, s->loop, direction);
        }
        if (s->slice_mask & (1u << from)) return from;
    }
    return s->first;
}
static int first_slice(BeetsLaneState *s)
{
    int i;
    for (i = s->first; i <= s->last; i++) if (s->slice_mask & (1u << i)) return i;
    return s->first;
}
static void fire(int lane, int slice, int mutations)
{
    BeetsLaneState *s = &state.lane[lane];
    Voice *v = &voice[lane];
    const BeetsSample *sample;
    float size;
    int stutter, reverse;
    if (mutations == 1 && chance(s, BEETS_SWITCH)) {
        int candidate = (int)(random_u32() % BEETS_LOOPS);
        s->loop = eligible_loop(s, candidate - 1, 1);
        s->events |= 1u << BEETS_SWITCH;
    }
    v->sample_index = s->loop;
    sample = &beets_samples[v->sample_index];
    v->increment = (float)sample->frames * (float)state.tempo / 10584000.0f;
    size = (float)sample->frames / 8.0f;
    s->slice = bound(slice, 0, 7);
    stutter = s->force_stutter || (mutations && chance(s, BEETS_STUTTER));
    reverse = s->force_reverse || (mutations && chance(s, BEETS_REVERSE));
    if (stutter) {
        int amount = s->force_stutter ? 1 : (int)(random_u32() % 4u) + 1;
        v->from = (float)s->slice * size;
        v->end = v->from + size * ((float)amount / 8.0f);
        s->events |= 1u << BEETS_STUTTER;
    } else {
        v->from = 0.0f;
        v->end = (float)sample->frames;
    }
    v->position = (float)s->slice * size;
    v->direction = reverse ? -1 : 1;
    if (reverse) s->events |= 1u << BEETS_REVERSE;
    /* Same boundary-based reverse as softcut: step backward from slice onset,
     * wrapping within the full loop (or the stutter window). */
    v->fade_from = v->last_output;
    v->fade = CROSSFADE;
}
static void tick(void)
{
    int lane;
    state.ticks++;
    for (lane = 0; lane < BEETS_LANES; lane++) {
        BeetsLaneState *s = &state.lane[lane];
        int next, manual_loop = s->queued_loop;
        s->events = 0;
        if (manual_loop >= 0) { s->loop = manual_loop; s->queued_loop = -1; }
        next = advance(s, s->slice, 1);
        if (chance(s, BEETS_SKIP)) { next = advance(s, next, 1); s->events |= 1u << BEETS_SKIP; }
        if (chance(s, BEETS_REPEAT)) { next = advance(s, next, -1); s->events |= 1u << BEETS_REPEAT; }
        if (state.step == 0) next = first_slice(s); /* firm musical downbeat */
        if (s->queued_slice >= 0) { next = s->queued_slice; s->queued_slice = -1; }
        /* An explicit player's loop request wins over auto-next and random
         * switching on this one boundary; mutations resume on the next tick. */
        if (manual_loop >= 0) s->loop = manual_loop;
        fire(lane, next, manual_loop >= 0 ? 2 : 1);
    }
}
void engine_init(uint32_t seed)
{
    int lane;
    /* C99 aggregate zeroing needs no freestanding string.h; the BSP supplies
     * memset/memcpy if the compiler chooses to lower aggregate assignments. */
    state = (BeetsState){0};
    for (lane = 0; lane < BEETS_LANES; lane++) voice[lane] = (Voice){0};
    state.tempo = 160; state.blend = 50; state.rng = seed ? seed : 0xBE375123u;
    clock_phase = 0;
    for (lane = 0; lane < BEETS_LANES; lane++) {
        BeetsLaneState *s = &state.lane[lane];
        s->loop = lane; s->slice = 0; s->queued_loop = s->queued_slice = -1;
        s->first = 0; s->last = 7; s->slice_mask = 255u; s->loop_mask = 1023u;
        s->muted = lane == 1;
        voice[lane].direction = 1;
        voice[lane].sample_index = lane;
        voice[lane].increment = (float)beets_samples[lane].frames * (float)state.tempo / 10584000.0f;
    }
}
void engine_snapshot(BeetsState *out) { if (out) *out = state; }
void engine_play(int on)
{
    int lane;
    on = !!on;
    if (!on) for (lane = 0; lane < BEETS_LANES; lane++) voice[lane].audition = 0;
    if (on == state.playing) return;
    state.playing = on;
    if (on) { state.step = 0; clock_phase = 0; tick(); }
    for (lane = 0; lane < BEETS_LANES; lane++) voice[lane].audition = 0;
}
void engine_panic(void)
{
    int lane;
    state.playing = 0;
    for (lane = 0; lane < BEETS_LANES; lane++) {
        voice[lane].audition = 0;
        voice[lane].gain = 0.0f;
        voice[lane].last_output = 0.0f;
        voice[lane].fade_from = 0.0f;
        voice[lane].fade = 0;
    }
}
void engine_tempo(int bpm)
{
    int lane;
    state.tempo = bound(bpm, 60, 220);
    for (lane = 0; lane < BEETS_LANES; lane++)
        voice[lane].increment = (float)beets_samples[voice[lane].sample_index].frames * (float)state.tempo / 10584000.0f;
}
void engine_blend(int percent) { state.blend = bound(percent, 0, 100); }
void engine_loop(int lane, int loop)
{
    BeetsLaneState *s;
    if (!valid(lane)) return;
    s = &state.lane[lane]; loop = bound(loop, 0, BEETS_LOOPS - 1);
    s->loop_mask |= 1u << loop;
    if (state.playing) s->queued_loop = loop;
    else { s->loop = loop; s->queued_loop = -1; voice[lane].audition = 0; }
}
void engine_range(int lane, int first, int last)
{
    BeetsLaneState *s;
    int t;
    if (!valid(lane)) return;
    first = bound(first, 0, 7); last = bound(last, 0, 7);
    if (first > last) { t = first; first = last; last = t; }
    s = &state.lane[lane]; s->first = first; s->last = last;
    if (!(s->slice_mask & (((1u << (last + 1)) - 1u) & ~((1u << first) - 1u)))) s->slice_mask |= 1u << first;
}
void engine_trigger(int lane, int slice)
{
    BeetsLaneState *s;
    if (!valid(lane)) return;
    s = &state.lane[lane]; slice = bound(slice, 0, 7);
    if (state.playing) s->queued_slice = slice;
    else {
        s->events = 0; s->queued_slice = -1;
        fire(lane, slice, 0);
        voice[lane].audition = (1323000u + (uint32_t)state.tempo - 1u) / (uint32_t)state.tempo;
    }
}
void engine_probability(int lane, int kind, int percent)
{
    if (valid(lane) && kind >= 0 && kind < BEETS_PROBS) state.lane[lane].probability[kind] = bound(percent, 0, 100);
}
void engine_mute(int lane, int muted) { if (valid(lane)) state.lane[lane].muted = !!muted; }
void engine_bypass(int lane, int bypass) { if (valid(lane)) state.lane[lane].bypass = !!bypass; }
void engine_force(int lane, int reverse, int stutter)
{
    if (valid(lane)) {
        BeetsLaneState *s = &state.lane[lane];
        s->force_reverse = !!reverse; s->force_stutter = !!stutter;
    }
}
void engine_masks(int lane, uint32_t slice_mask, uint32_t loop_mask)
{
    BeetsLaneState *s;
    if (!valid(lane)) return;
    s = &state.lane[lane];
    s->slice_mask = slice_mask & 255u; s->loop_mask = loop_mask & 1023u;
    if (!s->slice_mask) s->slice_mask = 1u << s->first;
    if (!s->loop_mask) s->loop_mask = 1u << s->loop;
    engine_range(lane, s->first, s->last);
}
void engine_auto(int lane, int on) { if (valid(lane)) state.lane[lane].auto_next = !!on; }
const char *engine_loop_name(int loop) { return beets_samples[bound(loop, 0, BEETS_LOOPS - 1)].name; }

void engine_render(int32_t *lr, uint32_t frames, uint32_t master_q12)
{
    uint32_t f;
    float master = (float)(master_q12 > 4096u ? 4096u : master_q12) / 4096.0f;
    float weights[2] = {(float)(100 - state.blend) * 0.01f, (float)state.blend * 0.01f};
    for (f = 0; f < frames; f++) {
        float mixed = 0.0f;
        int lane;
        for (lane = 0; lane < BEETS_LANES; lane++) {
            BeetsLaneState *s = &state.lane[lane];
            Voice *v = &voice[lane];
            const BeetsSample *sample = &beets_samples[v->sample_index];
            int active = state.playing || v->audition;
            float target = active && !s->muted ? 1.0f : 0.0f;
            float out = 0.0f;
            v->gain += (target - v->gain) * 0.005f;
            if (active || v->gain > 0.00001f) {
                uint32_t a, b;
                float frac, x, y;
                /* The bounds are initialized at the first trigger. */
                if (v->end <= v->from) { v->from = 0.0f; v->end = (float)sample->frames; }
                if (v->position < v->from || v->position >= v->end) {
                    if (v->position < v->from) v->position += v->end - v->from;
                    else v->position -= v->end - v->from;
                    v->fade_from = v->last_output;
                    v->fade = CROSSFADE;
                }
                a = (uint32_t)v->position;
                if (a >= sample->frames) a = sample->frames - 1u;
                b = a + 1u;
                if ((float)b >= v->end) b = (uint32_t)v->from;
                if (b >= sample->frames) b = 0;
                frac = v->position - (float)a;
                x = (float)beets_mulaw[sample->data[a]] / 32768.0f;
                y = (float)beets_mulaw[sample->data[b]] / 32768.0f;
                out = x + (y - x) * frac;
                if (v->fade) {
                    float t = (float)(CROSSFADE - v->fade) / (float)CROSSFADE;
                    out = v->fade_from + (out - v->fade_from) * t;
                    v->fade--;
                }
                v->last_output = out;
                v->position += (float)v->direction * v->increment;
            }
            mixed += out * v->gain * weights[lane];
            if (v->audition) v->audition--;
        }
        mixed *= master * 0.9f; /* fixed headroom; equal-sum crossfade cannot overload */
        if (mixed > 1.0f) mixed = 1.0f;
        if (mixed < -1.0f) mixed = -1.0f;
        lr[2u*f] = lr[2u*f+1u] = (int32_t)(mixed * FULL_SCALE);
        state.frames++;
        if (state.playing) {
            clock_phase += (uint32_t)state.tempo * 2u;
            if (clock_phase >= CLOCK_LIMIT) {
                clock_phase -= CLOCK_LIMIT;
                state.step = (state.step + 1) & 7;
                tick();
            }
        }
    }
}

uint32_t engine_clock_phase(void) { return clock_phase; }

static uint32_t engine_ppm(float value, float maximum)
{
    float scaled;
    if (value <= 0.0f || maximum <= 0.0f) return 0;
    if (value >= maximum) return 1000000u;
    scaled = value * (1000000.0f / maximum);
    return (uint32_t)scaled;
}
void engine_playback_snapshot(BeetsPlaybackState out[BEETS_LANES])
{
    int lane;
    if (!out) return;
    for (lane = 0; lane < BEETS_LANES; lane++) {
        const Voice *v = &voice[lane];
        float length = (float)beets_samples[v->sample_index].frames;
        float end = v->end > v->from ? v->end : length;
        float position = v->position;
        if (position < v->from) position += end - v->from;
        if (position >= end) position -= end - v->from;
        out[lane].sounding_loop = v->sample_index;
        out[lane].direction = v->direction;
        out[lane].active = state.playing || v->audition;
        out[lane].tail_active = !out[lane].active && v->gain > 0.00001f;
        out[lane].position_ppm = engine_ppm(position, length);
        out[lane].audition_remaining = v->audition;
        out[lane].window_start_ppm = engine_ppm(v->from, length);
        out[lane].window_end_ppm = engine_ppm(end, length);
        out[lane].gain_ppm = engine_ppm(v->gain, 1.0f);
    }
}

void engine_clock_snapshot(BeetsClock *out)
{
    out->playing = state.playing; out->ticks = state.ticks; out->phase = engine_clock_phase();
}
