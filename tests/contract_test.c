/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "engine.h"
#include "fixtures/beets_contract.h"

static BeetsState state(void) { BeetsState s; engine_snapshot(&s); return s; }
static BeetsState next_boundary(void)
{
    uint32_t before = state().ticks;
    int32_t audio[2];
    for (unsigned i = 0; i < 44100; i++) {
        engine_render(audio, 1, 4096);
        if (state().ticks != before) return state();
    }
    assert(!"clock stalled");
    return state();
}
static void expect(const char *name, int row, int slice, int loop)
{
    BeetsState s = state();
    if (s.lane[0].slice != slice || s.lane[0].loop != loop) {
        fprintf(stderr, "%s boundary %d: expected slice/loop %d/%d, got %d/%d\n",
                name, row, slice, loop, s.lane[0].slice, s.lane[0].loop);
        assert(0);
    }
}
int main(void)
{
    for (unsigned c = 0; c < sizeof contracts / sizeof contracts[0]; c++) {
        engine_init(123);
        engine_range(0, contracts[c].first, contracts[c].last);
        engine_masks(0, contracts[c].mask, 1023);
        engine_probability(0, BEETS_SKIP, contracts[c].skip);
        engine_probability(0, BEETS_REPEAT, contracts[c].repeat);
        engine_play(1);
        /* Enable auto after the initial boundary: no startup wrap ambiguity. */
        engine_auto(0, contracts[c].auto_next);
        for (int row = 0; row < 10; row++) {
            if (row) next_boundary();
            expect(contracts[c].name, row, contracts[c].slices[row], contracts[c].loops[row]);
            assert(state().step == (row & 7));
            /* Lane B remains natural: A's mutations/range must not leak. */
            assert(state().lane[1].slice == (row & 7));
        }
        printf("PASS source contract: %s\n", contracts[c].name);
    }
    /* FM1 adaptation: explicit requests beat auto/random switching for one
     * boundary, including a cue overriding the downbeat. */
    engine_init(123); engine_play(1);
    for (int i = 0; i < 7; i++) next_boundary();
    engine_auto(0, 1);
    engine_masks(0, 255, 1u << 2);
    engine_probability(0, BEETS_SWITCH, 100);
    engine_loop(0, 7); engine_trigger(0, 5);
    expect("requests stay queued", 7, 7, 0);
    assert(state().lane[0].queued_loop == 7 && state().lane[0].queued_slice == 5);
    /* Restrict random choice after enqueue so the next boundary is deterministic. */
    engine_masks(0, 255, 1u << 2);
    next_boundary(); expect("manual priority", 8, 5, 7);
    assert(state().lane[0].queued_loop == -1 && state().lane[0].queued_slice == -1);
    next_boundary(); expect("random switch resumes", 9, 6, 2);
    puts("PASS FM1 adaptation: queued manual priority, cue, and mutation resume");
    return 0;
}
