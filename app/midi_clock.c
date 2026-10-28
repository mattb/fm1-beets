/* SPDX-License-Identifier: MIT
 * Auto clock source; USB output follows fm1-base's source-mask no-echo rule.
 * TRS is input-only. No allocation, queues or persistent settings. */
#include "midi_clock.h"
#include "dsp/engine.h"
#include "plat.h"
static int source, pending, have_pulse;
static uint32_t source_from, last_ms, measured_ms, intervals;
static void output(uint8_t status) { plat_midi_send(0x0fu | (uint32_t)status << 8, TO_USB); }
void beets_clock_init(void)
{
    source = -1; pending = have_pulse = 0;
    source_from = last_ms = measured_ms = intervals = 0;
    engine_clock_output(output);
}
int beets_clock_external(void) { return source < 0 ? 0 : source == PORT_USB ? 1 : 2; }
void beets_clock_play(int on)
{
    if (source < 0) engine_play(on);
    else {
        engine_play(0);
        pending = on ? 1 : 0; /* panel Start waits for the next incoming pulse */
        if (!(source_from & TO_USB)) output(on ? 0xfa : 0xfc);
    }
}
void beets_clock_poll(uint32_t ms)
{
    if (source < 0 || ms - last_ms < 500u) return;
    engine_play(0);
    if (!(source_from & TO_USB)) output(0xfc);
    source = -1; pending = have_pulse = 0; intervals = 0;
    engine_external_clock(0);
}
int beets_clock_receive(uint32_t packet, int port, uint32_t from, uint32_t ms)
{
    unsigned status = packet >> 8 & 255u;
    if (status != 0xf8u && status != 0xfau && status != 0xfbu && status != 0xfcu) return 0;
    if (port != PORT_USB && port != PORT_TRS) return 1;
    beets_clock_poll(ms);
    if (source >= 0 && (source != port || source_from != from)) return 1;
    if (source < 0) {
        source = port; source_from = from; have_pulse = 0; intervals = 0;
        engine_external_clock(1);
    }
    last_ms = ms;
    /* MIDI realtime can be passed through; never echo to the sender. */
    if (!(from & TO_USB)) output((uint8_t)status);
    if (status == 0xfau) { engine_play(0); pending = 1; intervals = 0; have_pulse = 0; }
    else if (status == 0xfbu) pending = 2;
    else if (status == 0xfcu) { engine_play(0); pending = 0; }
    else {
        if (!have_pulse) { measured_ms = ms; have_pulse = 1; }
        else if (++intervals == 12u) {
            uint32_t dt = ms - measured_ms;
            if (dt) engine_tempo((int)((30000u + dt / 2u) / dt));
            measured_ms = ms; intervals = 0;
        }
        if (pending == 1) engine_play(1);
        else if (pending == 2) {
            BeetsState s; engine_snapshot(&s);
            if (s.ticks) engine_continue(); else engine_play(1);
        } else engine_clock_pulse();
        pending = 0;
    }
    return 1;
}
