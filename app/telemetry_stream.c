/* SPDX-License-Identifier: MIT
 * Main-loop-only telemetry. Whole frames are queued atomically or retried later;
 * never serialize/write USB from the audio ISR or use the ordinary MIDI ring.
 */
#include "telemetry_stream.h"
#include "plat.h"
#define INPUT_CAP 32u
#define STATE_DEADLINE_MS 100u
static BeetsTelemetryInput inputs[INPUT_CAP];
static uint32_t in_write, in_read, dropped, sequence, link, last_ms, last_tick;
static int last_half, last_playing, pending, connected, first_snapshot;
static uint8_t frame[BEETS_TELEMETRY_CAPACITY];
void beets_stream_init(void)
{
    in_write = in_read = dropped = sequence = link = last_ms = last_tick = 0;
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
void beets_stream_poll(const BeetsState *state, BeetsTelemetryUI *ui, uint32_t ms)
{
    uint32_t current_link = plat_midi_out_link(), n;
    int half = ui->clock_phase >= 1323000u;
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
        n = beets_telemetry_input_serialize(frame, sizeof frame, &inputs[in_read % INPUT_CAP], sequence);
        if (!n) { in_read++; dropped++; pending = 1; }
        else if (!plat_sysex_try_send(frame, n)) { in_read++; sequence++; }
        return;
    }
    if (!pending && in_read == in_write && state->playing == last_playing &&
        (!state->playing || (state->ticks == last_tick && half == last_half)) &&
        ms - last_ms < 1000u) return;
    ui->input_dropped = dropped;
    n = beets_telemetry_serialize(frame, sizeof frame, state, ui, sequence, ms);
    if (!n || plat_sysex_try_send(frame, n)) return;
    sequence++;
    pending = first_snapshot = 0;
    last_ms = ms;
    last_tick = state->ticks;
    last_half = half;
    last_playing = state->playing;
}
