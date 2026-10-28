/* SPDX-License-Identifier: MIT
 * Bounded ASCII JSON telemetry using jems; no IO, floats, heap, or engine mutation. */
#include "telemetry.h"
#include "vendor/jems/jems.h"

typedef struct { uint8_t *out; uint32_t capacity, length; int failed; } BeetsWriter;
static void bt_write(char ch, uintptr_t context)
{
    BeetsWriter *w = (BeetsWriter *)context;
    if (w->length < w->capacity) w->out[w->length++] = (uint8_t)ch;
    else w->failed = 1;
}
static void bt_uint(jems_t *j, uint32_t n)
{
    char text[10], reverse[10]; uint32_t count = 0, i;
    do { reverse[count++] = (char)('0' + n % 10u); n /= 10u; } while (n);
    for (i = 0; i < count; i++) text[i] = reverse[count - 1u - i];
    jems_literal(j, text, count);
}
static void bt_int(jems_t *j, int32_t n)
{
    char text[11], reverse[10]; uint32_t count = 0, pos = 0, i;
    uint32_t magnitude = n < 0 ? (uint32_t)(-(n + 1)) + 1u : (uint32_t)n;
    if (n < 0) text[pos++] = '-';
    do { reverse[count++] = (char)('0' + magnitude % 10u); magnitude /= 10u; } while (magnitude);
    for (i = 0; i < count; i++) text[pos++] = reverse[count - 1u - i];
    jems_literal(j, text, pos);
}
static void bt_ku(jems_t *j, const char *key, uint32_t n) { jems_string(j, key); bt_uint(j, n); }
static void bt_ki(jems_t *j, const char *key, int32_t n) { jems_string(j, key); bt_int(j, n); }
static void bt_begin(jems_t *j, jems_level_t *levels, BeetsWriter *w, const char *type, uint32_t seq, uint32_t ms)
{
    static const uint8_t prefix[] = {0xf0, 0x7d, 'B', 'E', 'E', 'T', 'S', 1};
    uint32_t i;
    for (i = 0; i < sizeof prefix; i++) bt_write((char)prefix[i], (uintptr_t)w);
    jems_init(j, levels, 6, bt_write, (uintptr_t)w);
    jems_object_open(j);
    bt_ku(j, "v", BEETS_TELEMETRY_VERSION);
    jems_key_string(j, "type", type);
    bt_ku(j, "seq", seq); bt_ku(j, "ms", ms);
}
static uint32_t bt_end(jems_t *j, BeetsWriter *w)
{
    jems_object_close(j);
    bt_write((char)0xf7, (uintptr_t)w);
    return w->failed ? 0 : w->length;
}
static const char *bt_loop_name(int loop)
{
    static const char *const names[10] = {"BEETS 1", "BEETS 2", "BEETS 3", "BEETS 4", "BEETS 5", "BEETS 6", "BEETS 7", "BEETS 8", "AMEN 1", "AMEN 2"};
    return loop >= 0 && loop < 10 ? names[loop] : "";
}
uint32_t beets_telemetry_serialize(uint8_t *out, uint32_t capacity,
    const BeetsState *state, const BeetsTelemetryUI *ui, uint32_t seq, uint32_t ms)
{
    jems_t j; jems_level_t levels[6];
    BeetsWriter w = {out, capacity, 0, 0};
    int lane, p; uint32_t message_len = 0;
    static const char *const probabilities[5] = {"repeat", "skip", "reverse", "stutter", "switch"};
    if (!out || !state || !ui || capacity < 9u) return 0;
    bt_begin(&j, levels, &w, "state", seq, ms);
    bt_ki(&j, "tempo", state->tempo); bt_ki(&j, "blend", state->blend);
    bt_ki(&j, "playing", state->playing); bt_ki(&j, "step", state->step);
    bt_ku(&j, "frames", state->frames); bt_ku(&j, "ticks", state->ticks); bt_ku(&j, "rng", state->rng);
    bt_ku(&j, "clock_phase", ui->clock_phase);
    bt_ki(&j, "clock_source", ui->clock_source);
    bt_ki(&j, "output_split", ui->output_split);
    jems_key_array_open(&j, "lanes");
    for (lane = 0; lane < BEETS_LANES; lane++) {
        const BeetsLaneState *s = &state->lane[lane];
        jems_object_open(&j);
        bt_ki(&j, "lane", lane); jems_key_string(&j, "name", lane ? "B" : "A");
        bt_ki(&j, "loop", s->loop); jems_key_string(&j, "loop_name", bt_loop_name(s->loop));
        bt_ki(&j, "queued_loop", s->queued_loop);
        jems_key_string(&j, "queued_loop_name", bt_loop_name(s->queued_loop));
        bt_ki(&j, "slice", s->slice); bt_ki(&j, "queued_slice", s->queued_slice);
        bt_ki(&j, "first", s->first); bt_ki(&j, "last", s->last);
        bt_ki(&j, "muted", s->muted); bt_ki(&j, "bypass", s->bypass); bt_ki(&j, "auto_next", s->auto_next);
        bt_ki(&j, "force_reverse", s->force_reverse); bt_ki(&j, "force_stutter", s->force_stutter);
        bt_ku(&j, "loop_mask", s->loop_mask); bt_ku(&j, "slice_mask", s->slice_mask); bt_ku(&j, "events", s->events);
        jems_key_object_open(&j, "probability");
        for (p = 0; p < BEETS_PROBS; p++) bt_ki(&j, probabilities[p], s->probability[p]);
        jems_object_close(&j); jems_object_close(&j);
    }
    jems_array_close(&j);
    jems_key_array_open(&j, "playheads");
    for (lane = 0; lane < BEETS_LANES; lane++) {
        const BeetsPlaybackState *v = &ui->playback[lane];
        jems_object_open(&j);
        bt_ki(&j, "lane", lane); bt_ki(&j, "sounding_loop", v->sounding_loop);
        jems_key_string(&j, "sounding_loop_name", bt_loop_name(v->sounding_loop));
        bt_ku(&j, "position_ppm", v->position_ppm); bt_ki(&j, "direction", v->direction);
        bt_ki(&j, "active", v->active); bt_ki(&j, "tail_active", v->tail_active);
        bt_ku(&j, "audition_remaining", v->audition_remaining);
        bt_ku(&j, "window_start_ppm", v->window_start_ppm); bt_ku(&j, "window_end_ppm", v->window_end_ppm);
        bt_ku(&j, "gain_ppm", v->gain_ppm);
        jems_object_close(&j);
    }
    jems_array_close(&j);
    jems_key_object_open(&j, "ui");
    bt_ki(&j, "focus", ui->focus); bt_ki(&j, "dirty", ui->dirty);
    bt_ku(&j, "keys", ui->keys); bt_ku(&j, "buttons", ui->buttons);
    bt_ku(&j, "frame_ms", ui->frame_ms); bt_ku(&j, "message_remaining_ms", ui->message_remaining_ms);
    bt_ku(&j, "master", ui->master); bt_ku(&j, "lights_buttons", ui->lights_buttons); bt_ku(&j, "lights_keys", ui->lights_keys);
    bt_ku(&j, "dim_buttons", ui->dim_buttons); bt_ku(&j, "dim_keys", ui->dim_keys); bt_ku(&j, "input_dropped", ui->input_dropped);
    while (message_len < sizeof ui->message && ui->message[message_len]) message_len++;
    jems_key_bytes(&j, "message", (const uint8_t *)ui->message, message_len);
    jems_object_close(&j);
    return bt_end(&j, &w);
}
uint32_t beets_telemetry_input_serialize(uint8_t *out, uint32_t capacity,
    const BeetsTelemetryInput *input, uint32_t seq)
{
    jems_t j; jems_level_t levels[6]; BeetsWriter w = {out, capacity, 0, 0}; int i;
    if (!out || !input || capacity < 9u) return 0;
    bt_begin(&j, levels, &w, "input", seq, input->ms);
    bt_ku(&j, "keys_down", input->keys_down); bt_ku(&j, "keys_up", input->keys_up);
    bt_ku(&j, "buttons_down", input->buttons_down); bt_ku(&j, "buttons_up", input->buttons_up);
    jems_key_array_open(&j, "enc"); for (i = 0; i < 7; i++) bt_int(&j, input->enc[i]); jems_array_close(&j);
    return bt_end(&j, &w);
}
