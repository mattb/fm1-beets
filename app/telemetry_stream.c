/* SPDX-License-Identifier: MIT
 * Main-loop-only telemetry. Whole frames are queued atomically or retried later;
 * never serialize/write USB from the audio ISR or use the ordinary MIDI ring.
 */
#include "telemetry_stream.h"
#include "plat.h"
#include "telemetry_profile.h"
#define INPUT_CAP 32u
#define STATE_DEADLINE_MS 100u
#define RETRY_MS 20u
static BeetsTelemetryInput inputs[INPUT_CAP];
static uint32_t in_write, in_read, dropped, sequence, link, last_ms, last_tick, last_poll, poll_interval;
static int last_half, last_playing, pending, connected, first_snapshot, polled;
static uint8_t frame[BEETS_TELEMETRY_CAPACITY];
void beets_stream_init(void)
{
    in_write = in_read = dropped = sequence = link = last_ms = last_tick = last_poll = 0;
    polled = 0; poll_interval = 1;
    last_half = last_playing = connected = 0;
    pending = first_snapshot = 1;
}
void beets_stream_changed(void) { pending = 1; }
void beets_stream_input(const BeetsTelemetryInput *event)
{
    if (!connected || !event) return;
    if (in_write - in_read == INPUT_CAP) { dropped++; pending = 1; return; }
    inputs[in_write++ % INPUT_CAP] = *event;
    pending = 1;
}
void beets_stream_poll(uint32_t ms)
{
    uint32_t current_link, n, irq;
    BeetsClock clock;
    /* Bound idle checks and would-block retries, independent of main-loop speed. */
    if (polled && ms - last_poll < poll_interval) return;
    polled = 1; last_poll = ms; poll_interval = 1;
    current_link = plat_midi_out_link();
    if (!plat_usb_connected()) {
        dropped += in_write - in_read;
        in_read = in_write;
        connected = 0;
        pending = first_snapshot = 1;
        return;
    }
    if (!connected || current_link != link) {
        dropped += in_write - in_read;
        in_read = in_write;
        link = current_link;
        connected = 1;
        pending = first_snapshot = 1;
    }
    /* Give input FIFO priority, but guarantee a fresh state at least every
     * 100 ms under sustained input, provided the transport accepts a frame. */
    if (!first_snapshot && in_read != in_write && ms - last_ms < STATE_DEADLINE_MS) {
        BP_BEGIN(serial);
        n = beets_telemetry_input_serialize(frame, sizeof frame, &inputs[in_read % INPUT_CAP], sequence);
        BP_END(2, serial);
        if (!n) { in_read++; dropped++; pending = 1; }
        else if (!BP_SEND(frame, n)) { in_read++; sequence++; }
        else poll_interval = RETRY_MS;
        return;
    }
    irq = plat_lock(); engine_clock_snapshot(&clock); plat_unlock(irq);
    int half = clock.phase >= 1323000u;
    if (!pending && in_read == in_write && clock.playing == last_playing &&
        (!clock.playing || (clock.ticks == last_tick && half == last_half)) &&
        ms - last_ms < 1000u) return;
    BeetsState state;
    BeetsTelemetryUI ui;
    app_telemetry_snapshot(&state, &ui);
    ui.input_dropped = dropped;
    BP_BEGIN(serial);
    n = beets_telemetry_serialize(frame, sizeof frame, &state, &ui, sequence, ms);
    BP_END(2, serial);
    if (!n || BP_SEND(frame, n)) { poll_interval = RETRY_MS; return; }
    sequence++;
    pending = first_snapshot = 0;
    last_ms = ms;
    last_tick = state.ticks;
    last_half = ui.clock_phase >= 1323000u;
    last_playing = state.playing;
}
