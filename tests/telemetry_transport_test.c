/* SPDX-License-Identifier: MIT
 * Transport model: success accepts a complete frame; failure accepts no bytes.
 * No hardware timing claims: this tests the app's queue/retry scheduling. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "telemetry_stream.h"
static int online = 1, blocked, count, attempts, snapshots, clocks;
static uint32_t epoch;
static char captured[128][BEETS_TELEMETRY_CAPACITY + 1u];
int plat_usb_connected(void) { return online; }
uint32_t plat_midi_out_link(void) { return epoch; }
int plat_sysex_try_send(const uint8_t *p, uint32_t n)
{
    attempts++;
    assert(n >= 9 && n <= BEETS_TELEMETRY_CAPACITY);
    assert(p[0] == 0xf0 && p[n - 1] == 0xf7);
    for (uint32_t i = 1; i + 1 < n; i++) assert(p[i] < 128);
    if (blocked) return -1;
    assert(count < 128);
    memcpy(captured[count], p, n); captured[count++][n] = 0;
    return 0;
}
static BeetsState state;
static BeetsTelemetryUI ui;
uint32_t plat_lock(void) { return 0; }
void plat_unlock(uint32_t f) { (void)f; }
void engine_clock_snapshot(BeetsClock *c)
{ clocks++; c->playing = state.playing; c->ticks = state.ticks; c->phase = ui.clock_phase; }
void app_telemetry_snapshot(BeetsState *s, BeetsTelemetryUI *u)
{ snapshots++; *s = state; *u = ui; }
static void reset(void)
{
    memset(&state, 0, sizeof state); memset(&ui, 0, sizeof ui);
    state.tempo = 160; state.blend = 50;
    online = 1; blocked = count = attempts = snapshots = clocks = 0; epoch = 1;
    beets_stream_init();
}
static void poll(uint32_t ms) { beets_stream_poll(ms); }
static void has(int index, const char *s) { assert(index < count); assert(strstr(captured[index], s)); }
static void event(uint32_t ms, uint32_t down, uint32_t up, int delta)
{
    BeetsTelemetryInput e = {0}; e.ms = ms; e.keys_down = down; e.keys_up = up; e.enc[0] = delta;
    beets_stream_input(&e);
}
int main(void)
{
    reset(); poll(0); assert(count == 1); has(0, "\"type\":\"state\""); has(0, "\"seq\":0,");
    poll(999); assert(count == 1); poll(1000); assert(count == 2);
    beets_stream_changed(); poll(1001); assert(count == 3);
    state.playing = 1; poll(1002); assert(count == 4);
    state.ticks++; poll(1003); assert(count == 5);
    ui.clock_phase = 1323000; poll(1004); assert(count == 6);
    poll(1005); assert(count == 6);

    reset(); blocked = 1; poll(0);
    for (uint32_t i = 1; i < 20; i++) poll(i);
    assert(!count && attempts == 1 && snapshots == 1);
    blocked = 0; poll(20); has(0, "\"seq\":0,");
    event(21, 4, 0, -5); event(22, 0, 4, 7);
    blocked = 1; poll(23); poll(24); assert(count == 1);
    blocked = 0; poll(43); has(1, "\"keys_down\":4,"); has(1, "\"enc\":[-5,"); has(1, "\"seq\":1,");
    poll(44); has(2, "\"keys_up\":4,"); has(2, "\"enc\":[7,"); has(2, "\"seq\":2,");
    poll(45); has(3, "\"type\":\"state\""); has(3, "\"input_dropped\":0");

    reset(); poll(0); for (int i = 0; i < 35; i++) event((uint32_t)i, (uint32_t)i + 1, 0, i);
    poll(100); has(1, "\"input_dropped\":3"); has(1, "\"type\":\"state\"");
    for (int i = 0; i < 32; i++) { poll((uint32_t)i + 101); char expected[48]; snprintf(expected, sizeof expected, "\"keys_down\":%d,", i + 1); has(i + 2, expected); }
    assert(count == 34);

    reset(); poll(0); event(1, 1, 0, 0); online = 0; poll(2); event(3, 0, 1, 0);
    online = 1; epoch++; poll(4); assert(count == 2); has(1, "\"type\":\"state\""); has(1, "\"input_dropped\":1");
    poll(5); assert(count == 2);
    event(6, 8, 0, 0); epoch++; poll(7); assert(count == 3); has(2, "\"input_dropped\":2");

    reset(); poll(0);
    for (uint32_t i = 1; i <= 99; i++) { event(i, 1, 0, 1); poll(i); }
    event(100, 1, 0, 1); poll(100); has(100, "\"type\":\"state\"");
    poll(101); has(101, "\"type\":\"input\"");

    reset(); poll(0); event(1, 1, 0, 0); event(2, 0, 1, 0);
    poll(100); has(1, "\"type\":\"state\"");
    poll(200); has(2, "\"type\":\"state\"");
    poll(201); has(3, "\"type\":\"input\"");
    poll(202); has(4, "\"keys_up\":1,");

    reset(); poll(0xfffffff0u); poll(0x000003d7u); assert(count == 1);
    poll(0x000003d8u); assert(count == 2); /* unsigned millisecond wrap */
    reset(); poll(0);
    for (int i = 0; i < 10000; i++) poll(0);
    assert(snapshots == 1 && clocks == 1 && attempts == 1);
    for (uint32_t i = 1; i < 1000; i++) poll(i);
    assert(snapshots == 1 && attempts == 1); /* only cheap clock reads while idle */
    online = 0; poll(1000); int previous_clocks = clocks;
    for (uint32_t i = 1001; i < 1100; i++) poll(i);
    assert(snapshots == 1 && clocks == previous_clocks); /* no engine reads disconnected */
    reset(); blocked = 1;
    for (uint32_t i = 0; i < 1000; i++) poll(i);
    assert(attempts == 50 && snapshots == 50); /* bounded retries without a frame cache */
    puts("Telemetry transport passed: atomic retry, edge FIFO, overflow, state deadline, reconnect, heartbeat and timer wrap.");
    return 0;
}
