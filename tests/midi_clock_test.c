/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "midi_clock.h"
#include "dsp/engine.h"
#include "plat.h"
static unsigned count, clocks, starts, stops, continues, sample, last_sample, min_gap, max_gap;
void plat_midi_send(uint32_t packet, uint32_t to)
{
    unsigned s = packet >> 8 & 255u;
    assert(to == TO_USB && (packet & 255u) == 15u);
    count++;
    if (s == 0xf8) {
        if (clocks++) { unsigned gap = sample - last_sample; if (gap < min_gap) min_gap = gap; if (gap > max_gap) max_gap = gap; }
        last_sample = sample;
    }
    if (s == 0xfa) starts++;
    if (s == 0xfb) continues++;
    if (s == 0xfc) stops++;
}
static BeetsState state(void) { BeetsState s; engine_snapshot(&s); return s; }
static void reset(void)
{
    engine_init(123); beets_clock_init(); engine_tempo(120);
    count = clocks = starts = stops = continues = sample = last_sample = max_gap = 0; min_gap = ~0u;
}
static void render(unsigned n)
{
    int32_t out[2];
    while (n--) { sample++; engine_render(out, 1, 4096); }
}
static void midi(unsigned s, int port, uint32_t ms)
{
    assert(beets_clock_receive(15u | s << 8, port, port == PORT_USB ? TO_USB : 0, ms));
}
int main(void)
{
    reset(); beets_clock_play(1); render(88200);
    assert(starts == 1 && clocks == 97 && state().ticks == 9);
    assert(min_gap == 918 && max_gap == 919); /* exact rational 24 PPQN at 120 BPM */
    beets_clock_play(0); unsigned n = count; render(44100); assert(count == n && stops == 1);
    puts("PASS internal clock: 24 PPQN, rational spacing, Start/Stop, stopped silence");

    reset(); midi(0xfa, PORT_USB, 0); assert(!state().playing);
    midi(0xf8, PORT_USB, 0); assert(state().playing && state().step == 0);
    for (unsigned i = 1; i <= 12; i++) { render(919); midi(0xf8, PORT_USB, (i * 125u + 3u) / 6u); }
    assert(state().ticks == 2 && state().step == 1 && state().tempo == 120 && count == 0);
    render(44100); assert(state().ticks == 2); /* no free-running sequencer */
    uint32_t ticks = state().ticks, phase = engine_clock_phase();
    midi(0xfc, PORT_USB, 260); render(100); assert(!state().playing);
    midi(0xfb, PORT_USB, 270); midi(0xf8, PORT_USB, 280);
    assert(state().playing && state().ticks == ticks && engine_clock_phase() == phase);
    midi(0xfa, PORT_USB, 290); midi(0xf8, PORT_USB, 300);
    assert(state().step == 0 && state().ticks == ticks + 1);
    assert(count == 0);
    puts("PASS USB follower: 12 clocks/eighth, tempo, restart/continue, no echo");

    reset(); midi(0xfa, PORT_TRS, 100); midi(0xf8, PORT_TRS, 100);
    n = count; midi(0xfc, PORT_USB, 110); midi(0xf8, PORT_USB, 120);
    assert(state().playing && count == n && beets_clock_external() == 2);
    midi(0xfc, PORT_TRS, 130); assert(!state().playing && stops == 1);
    midi(0xfb, PORT_TRS, 140); midi(0xf8, PORT_TRS, 150);
    assert(state().playing && continues == 1 && starts == 1 && clocks == 2);
    beets_clock_poll(649); assert(state().playing);
    beets_clock_poll(650); assert(!state().playing && beets_clock_external() == 0 && stops == 2);
    puts("PASS TRS input forwards USB, competing source ignored, loss stops after 500ms");

    reset(); midi(0xfa, PORT_USB, 0xfffffff0u); midi(0xf8, PORT_USB, 0xfffffff0u);
    beets_clock_poll(0x100); assert(state().playing);
    beets_clock_poll(0x200); assert(!state().playing);
    beets_clock_play(1); assert(state().playing && starts == 1);
    puts("PASS timestamp wrap and return to internal playback");
    return 0;
}
