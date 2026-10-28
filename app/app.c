/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Beets control/display integration adapted from fm1-base's template.
 * Musical engine: dsp/engine.c, derived from Matt Biddulph's MIT Beets. */
#include "dsp/engine.h"
#include "telemetry_stream.h"
#define BEETS_SAVE_MAGIC 0x42545332u
static const uint8_t beets_white[16] = PLAT_WHITE_KEYS;
static const uint8_t beets_black[11] = PLAT_BLACK_KEYS;
static struct {
    uint32_t keys, buttons, msg_until, last_draw;
    uint32_t lights_buttons, lights_keys, dim_buttons, dim_keys;
    uint32_t telemetry_master, telemetry_master_ms;
    int focus, dirty, reverse_deck, stutter_deck;
    char message[48];
    BeetsState state;
} beets_ui;
typedef struct { uint32_t magic; int focus; BeetsState state; } BeetsSave;
static void beets_cat(char *d, const char *s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; }
static void beets_number(int value, char *out)
{
    char tmp[12]; int n = 0; unsigned v;
    if (value < 0) { *out++ = '-'; v = (unsigned)-value; } else v = (unsigned)value;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *out++ = tmp[--n]; *out = 0;
}
static void beets_snapshot(void) { uint32_t f = plat_lock(); engine_snapshot(&beets_ui.state); plat_unlock(f); }
void app_say(const char *a, const char *b)
{
    int i = 0;
    while (a && *a && i < 45) beets_ui.message[i++] = *a++;
    if (b && *b && i < 46) beets_ui.message[i++] = ' ';
    while (b && *b && i < 47) beets_ui.message[i++] = *b++;
    beets_ui.message[i] = 0;
    beets_ui.msg_until = plat_ms() + 1400;
    beets_stream_changed();
    if (ann_on()) ann_say(beets_ui.message, 0, ANN_NOW);
}
static void beets_restore(const BeetsSave *s)
{
    int d, p;
    engine_tempo(s->state.tempo); engine_blend(s->state.blend);
    beets_ui.focus = s->focus == 1;
    for (d = 0; d < 2; d++) {
        const BeetsLaneState *l = &s->state.lane[d];
        engine_loop(d, l->loop); engine_range(d, l->first, l->last);
        engine_mute(d, l->muted); engine_bypass(d, l->bypass); engine_auto(d, l->auto_next);
        engine_masks(d, l->slice_mask, l->loop_mask);
        for (p = 0; p < 5; p++) engine_probability(d, p, l->probability[p]);
    }
}
void app_init(void)
{
    BeetsSave saved;
    memset(&beets_ui, 0, sizeof beets_ui);
    beets_ui.reverse_deck = beets_ui.stutter_deck = -1;
    engine_init(0xBE375u);
    beets_stream_init();
    if (plat_store_load(OBJ_SETTINGS, &saved, sizeof saved) == (int)sizeof saved && saved.magic == BEETS_SAVE_MAGIC)
        beets_restore(&saved);
    beets_snapshot();
}
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t pkt; int port;
    while (plat_midi_in(&pkt, &port)) {
        unsigned status = pkt >> 8 & 255u, note = pkt >> 16 & 127u, vel = pkt >> 24 & 127u;
        if ((status & 0xf0u) == 0x90u && vel && note >= 36u && note < 44u)
            engine_trigger((status & 15u) == 1u ? 1 : 0, (int)note - 36);
        else if (status == 0xFAu) engine_play(1);
        else if (status == 0xFCu) engine_play(0);
    }
    engine_render(lr, n, master_q12);
}
void app_panic(void) { uint32_t irq = plat_lock(); engine_panic(); plat_unlock(irq); }
static void beets_save(void)
{
    BeetsSave saved; int d;
    beets_snapshot();
    if (beets_ui.state.playing) { app_say("STOP TO SAVE", ""); return; }
    app_panic(); /* stopped previews must also be silent before flash writes */
    saved.magic = BEETS_SAVE_MAGIC; saved.focus = beets_ui.focus; saved.state = beets_ui.state;
    for (d = 0; d < 2; d++) {
        if (saved.state.lane[d].queued_loop >= 0) saved.state.lane[d].loop = saved.state.lane[d].queued_loop;
    }
    if (plat_store_save(OBJ_SETTINGS, &saved, sizeof saved) == 0) { beets_ui.dirty = 0; app_say("SAVED", ""); }
    else app_say("SAVE", "FAILED");
}
static void beets_lights(void)
{
    uint32_t bright = beets_ui.buttons, dim = 0, keys = beets_ui.keys, kd = 0;
    int d, i; BeetsState *s = &beets_ui.state;
    for (d = 0; d < 2; d++) for (i = 0; i < 8; i++) {
        uint32_t bit = 1u << beets_white[d * 8 + i];
        if ((s->lane[d].slice_mask >> i & 1u) && i >= s->lane[d].first && i <= s->lane[d].last) kd |= bit;
        if (s->playing && i == s->lane[d].slice && !s->lane[d].muted) keys |= bit;
    }
    for (i = 0; i < 10; i++) {
        uint32_t bit = 1u << beets_black[i];
        if (s->lane[beets_ui.focus].loop_mask >> i & 1u) kd |= bit;
        if (i == s->lane[beets_ui.focus].loop) keys |= bit;
    }
    bright |= 1u << (beets_ui.focus ? B_OCTUP : B_OCTDN);
    dim |= (1u << B_OCTDN) | (1u << B_OCTUP) | (1u << B_HOME) | (1u << B_PLAY);
    if (s->playing) bright |= LED_PLAY_GREEN;
    if (s->lane[0].muted) bright |= 1u << B_SEL;
    if (s->lane[1].muted) bright |= 1u << B_GLO;
    if (s->lane[beets_ui.focus].bypass) bright |= 1u << B_FX;
    if (s->lane[beets_ui.focus].auto_next) bright |= 1u << B_SEQ;
    if (s->lane[beets_ui.focus].first == s->lane[beets_ui.focus].last) bright |= 1u << B_LFO;
    if (beets_ui.dirty) bright |= 1u << B_SAVE;
    beets_ui.lights_buttons = bright; beets_ui.lights_keys = keys;
    beets_ui.dim_buttons = dim; beets_ui.dim_keys = kd;
    plat_lights(bright, keys, dim, kd, 1);
}
static void beets_input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), down = btn & ~beets_ui.buttons, fresh = keys & ~beets_ui.keys;
    int d, i, e, knob = -1, save = 0, edit = (btn >> B_EDIT) & 1u;
    char msg[48] = ""; uint32_t irq;
    BeetsTelemetryInput input = {0};
    input.ms = plat_ms();
    input.keys_down = fresh; input.keys_up = beets_ui.keys & ~keys;
    input.buttons_down = down; input.buttons_up = beets_ui.buttons & ~btn;
    beets_snapshot();
    irq = plat_lock();
    if (down & (1u << B_OCTDN)) { beets_ui.focus = 0; beets_cat(msg, "FOCUS DECK A"); }
    if (down & (1u << B_OCTUP)) { beets_ui.focus = 1; beets_cat(msg, "FOCUS DECK B"); }
    d = beets_ui.focus;
    if (down & (1u << B_PLAY)) engine_play(!beets_ui.state.playing);
    if (down & (1u << B_SEL)) { engine_mute(0, !beets_ui.state.lane[0].muted); beets_ui.dirty = 1; }
    if (down & (1u << B_GLO)) { engine_mute(1, !beets_ui.state.lane[1].muted); beets_ui.dirty = 1; }
    if (down & (1u << B_FX)) { engine_bypass(d, !beets_ui.state.lane[d].bypass); beets_ui.dirty = 1; }
    if (down & (1u << B_HOME)) { engine_range(d, 0, 7); beets_ui.dirty = 1; }
    if (down & (1u << B_LFO)) {
        BeetsLaneState *l = &beets_ui.state.lane[d];
        if (l->first == l->last) engine_range(d, 0, 7); else engine_range(d, l->slice, l->slice);
        beets_ui.dirty = 1;
    }
    if (down & (1u << B_SEQ)) { engine_auto(d, !beets_ui.state.lane[d].auto_next); beets_ui.dirty = 1; }
    if (down & (1u << B_REC)) { engine_trigger(0, beets_ui.state.lane[0].first); engine_trigger(1, beets_ui.state.lane[1].first); }
    if (down & (1u << B_SAVE)) save = 1;
    {
        uint32_t mask = beets_ui.state.lane[d].loop_mask; int changed = 0;
        for (i = 0; i < 10; i++) if (fresh >> beets_black[i] & 1u) {
            if (edit) { mask ^= 1u << i; changed = 1; }
            else engine_loop(d, i);
            beets_ui.dirty = 1;
        }
        if (changed) engine_masks(d, beets_ui.state.lane[d].slice_mask, mask);
    }
    if (down & (1u << B_ENV)) beets_ui.reverse_deck = beets_ui.focus;
    if (down & (1u << B_ARP)) beets_ui.stutter_deck = beets_ui.focus;
    if (!(btn & (1u << B_ENV))) beets_ui.reverse_deck = -1;
    if (!(btn & (1u << B_ARP))) beets_ui.stutter_deck = -1;
    /* Each white bank always addresses its own deck; it never redirects the knobs. */
    for (d = 0; d < 2; d++) {
        int first = -1, last = -1, count = 0, changed = 0;
        uint32_t mask = beets_ui.state.lane[d].slice_mask;
        for (i = 0; i < 8; i++) {
            uint32_t bit = 1u << beets_white[d * 8 + i];
            if (keys & bit) { if (first < 0) first = i; last = i; count++; }
            if (fresh & bit) {
                changed = 1;
                if (edit) { mask ^= 1u << i; beets_ui.dirty = 1; }
                else {
                    engine_trigger(d, i);
                    if (!beets_ui.state.playing && beets_ui.state.lane[d].muted) {
                        msg[0] = 0; beets_cat(msg, d ? "B MUTED: GLO TO UNMUTE" : "A MUTED: SEL TO UNMUTE");
                    }
                }
            }
        }
        if (edit && changed) engine_masks(d, mask, beets_ui.state.lane[d].loop_mask);
        if (!edit && changed && count >= 2) { engine_range(d, first, last); engine_trigger(d, first); beets_ui.dirty = 1; }
        engine_force(d, d == beets_ui.reverse_deck, d == beets_ui.stutter_deck);
    }
    d = beets_ui.focus;
    for (i = 0; i < NE; i++) if ((e = plat_enc(i)) != 0) {
        int p = i == EN_ALGO ? BEETS_SWITCH : i - EN_K1;
        knob = i;
        input.enc[i] = e;
        if (i == EN_SELECT) engine_tempo(beets_ui.state.tempo + e);
        else if (i == EN_PRESET) engine_blend(beets_ui.state.blend + e);
        else if (p >= 0 && p < 5) engine_probability(d, p, beets_ui.state.lane[d].probability[p] + e * 5);
        beets_ui.dirty = 1;
    }
    engine_snapshot(&beets_ui.state);
    plat_unlock(irq);
    beets_ui.keys = keys; beets_ui.buttons = btn;
    if (down & (1u << B_PLAY)) app_say(beets_ui.state.playing ? "PLAYING" : "STOPPED", "");
    if (down & (1u << B_SEL)) app_say("DECK A", beets_ui.state.lane[0].muted ? "MUTED" : "UNMUTED");
    if (down & (1u << B_GLO)) app_say("DECK B", beets_ui.state.lane[1].muted ? "MUTED" : "UNMUTED");
    if (down & (1u << B_FX)) app_say("MUTATIONS", beets_ui.state.lane[beets_ui.focus].bypass ? "BYPASSED" : "ON");
    if (down & (1u << B_HOME)) app_say("FULL RANGE", "1 TO 8");
    if (down & (1u << B_LFO)) app_say("SLICE LOCK", beets_ui.state.lane[beets_ui.focus].first == beets_ui.state.lane[beets_ui.focus].last ? "ON" : "OFF");
    if (down & (1u << B_SEQ)) app_say("AUTO NEXT", beets_ui.state.lane[beets_ui.focus].auto_next ? "ON" : "OFF");
    if (down & (1u << B_REC)) app_say("RE-CUE", "BOTH DECKS");
    if (down & (1u << B_EDIT)) app_say("HOLD EDIT", "KEYS TOGGLE ELIGIBILITY");
    if (msg[0]) app_say(msg, "");
    if (knob >= 0) {
        static const char *const names[5] = {"REPEAT", "SKIP", "REVERSE", "STUTTER", "LOOP SWITCH"};
        char n[12]; int value;
        msg[0] = 0;
        if (knob == EN_SELECT) { beets_cat(msg, "TEMPO "); value = beets_ui.state.tempo; }
        else if (knob == EN_PRESET) { beets_cat(msg, "BLEND B "); value = beets_ui.state.blend; }
        else {
            int p = knob == EN_ALGO ? BEETS_SWITCH : knob - EN_K1;
            beets_cat(msg, beets_ui.focus ? "B " : "A "); beets_cat(msg, names[p]); beets_cat(msg, " ");
            value = beets_ui.state.lane[beets_ui.focus].probability[p];
        }
        beets_number(value, n); beets_cat(msg, n); beets_cat(msg, knob == EN_SELECT ? " BPM" : " PERCENT");
        beets_ui.message[0] = 0; beets_cat(beets_ui.message, msg); beets_ui.msg_until = plat_ms() + 1400;
        ann_later(msg, ANN_NOW, 140, plat_ms());
    }
    if (input.keys_down || input.keys_up || input.buttons_down || input.buttons_up || knob >= 0)
        beets_stream_input(&input);
    if (save) beets_save();
    /* Check the smoothed pot at 20 Hz; small jitter must not trigger snapshots. */
    uint32_t now = plat_ms();
    if (now - beets_ui.telemetry_master_ms >= 50u) {
        uint32_t master = plat_master();
        beets_ui.telemetry_master_ms = now;
        if (master > beets_ui.telemetry_master + 16u || beets_ui.telemetry_master > master + 16u) {
            beets_ui.telemetry_master = master;
            beets_stream_changed();
        }
    }
    beets_lights();
}
void app_input_only(void) { beets_input(); }
static void beets_draw_lane(int d, int y)
{
    BeetsLaneState *l = &beets_ui.state.lane[d];
    uint16_t color = d ? RGB(110,190,255) : RGB(220,238,110);
    char t[32], n[12]; int i;
    cv_begin(240, 56, C_BLACK);
    cv_text(5, 0, &FONT_B, d ? "B" : "A", color);
    cv_text(24, 1, &FONT_XS, engine_loop_name(l->loop), C_WHITE);
    if (l->muted) cv_text(166, 1, &FONT_XS, "MUTED", C_GRAY);
    else if (beets_ui.focus == d) cv_text(174, 1, &FONT_XS, "FOCUS", color);
    if (l->queued_loop >= 0) { beets_number(l->queued_loop + 1, n); t[0] = '>'; t[1] = 0; beets_cat(t, n); cv_text(140, 1, &FONT_XS, t, color); }
    for (i = 0; i < 8; i++) {
        int x = 5 + i * 29, in = i >= l->first && i <= l->last, on = i == l->slice && beets_ui.state.playing;
        int enabled = l->slice_mask >> i & 1u;
        cv_rect(x, 20, 26, 25, enabled ? (on ? color : in ? C_DIM : C_LINE) : C_BLACK);
        beets_number(i + 1, t); cv_text(x + 9, 24, &FONT_S, t, on ? C_BLACK : enabled ? C_WHITE : C_DIM);
        if (!enabled) cv_line(x + 5, 39, x + 21, 23, C_DIM);
        if (in) cv_rect(x, 48, 26, 2, color);
    }
    cv_blit(0, y);
}
static void beets_draw(void)
{
    char t[48], n[12]; int i; BeetsState *s = &beets_ui.state;
    BeetsLaneState *l = &s->lane[beets_ui.focus];
    cv_begin(240, 30, C_BLACK);
    cv_text(5, 3, &FONT_B, "BEETS", C_WHITE);
    beets_number(s->tempo, t); beets_cat(t, " BPM"); cv_text(82, 5, &FONT_XS, t, C_HI);
    cv_text(174, 5, &FONT_XS, s->playing ? "PLAYING" : "STOPPED", s->playing ? C_HI : C_GRAY);
    cv_blit(0, 0);
    beets_draw_lane(0, 30); beets_draw_lane(1, 88);
    cv_begin(240, 64, C_BLACK);
    cv_text(5, 0, &FONT_XS, beets_ui.focus ? "KNOBS: DECK B" : "KNOBS: DECK A", C_WHITE);
    if (l->bypass) cv_text(131, 0, &FONT_XS, "STRAIGHT", C_HI);
    else if (l->first == l->last) cv_text(148, 0, &FONT_XS, "LOCKED", C_HI);
    else if (l->auto_next) cv_text(148, 0, &FONT_XS, "AUTO", C_HI);
    for (i = 0; i < 5; i++) {
        static const char *const labels[5] = {"REPT", "SKIP", "REV", "STUT", "LOOP"};
        int x = 5 + i * 47;
        cv_text(x, 18, &FONT_XS, labels[i], C_GRAY);
        beets_number(l->probability[i], t); beets_cat(t, "%");
        cv_text(x, 32, &FONT_XS, t, C_WHITE);
        cv_rect(x, 50, 40, 4, C_LINE); cv_rect(x, 50, l->probability[i] * 40 / 100, 4, C_HI);
        if (l->events >> i & 1u) cv_rect(x, 58, 40, 2, C_WHITE);
    }
    cv_blit(0, 147);
    cv_begin(240, 29, C_BLACK);
    if (beets_ui.msg_until > plat_ms()) cv_text(5, 4, &FONT_XS, beets_ui.message, C_HI);
    else {
        t[0] = 0; beets_cat(t, "A "); beets_number(100 - s->blend, n); beets_cat(t, n); beets_cat(t, " / B ");
        beets_number(s->blend, n); beets_cat(t, n); cv_text(5, 3, &FONT_XS, t, C_GRAY);
        cv_text(132, 3, &FONT_XS, "HOME: FULL", C_GRAY);
    }
    cv_blit(0, 211);
}
void app_ui_init(void) { lcd_fill(0, 0, 240, 240, C_BLACK); beets_draw(); }
static void beets_say_cat(char *d, const char *s)
{
    uint32_t n = 0;
    while (n < ANN_MAX && d[n]) n++;
    while (*s && n < ANN_MAX) d[n++] = *s++;
    d[n] = 0;
}
static void beets_speak_screen(void)
{
    static const char *const names[5] = {"REPEAT ", "SKIP ", "REVERSE ", "STUTTER ", "LOOP "};
    char t[ANN_MAX + 1] = "BEETS. ", n[12]; int d, p;
    BeetsState *s = &beets_ui.state;
    beets_say_cat(t, s->playing ? "PLAYING. " : "STOPPED. ");
    beets_number(s->tempo, n); beets_say_cat(t, n); beets_say_cat(t, " BPM. ");
    for (d = 0; d < 2; d++) {
        BeetsLaneState *l = &s->lane[d];
        beets_say_cat(t, d ? "B " : "A "); beets_say_cat(t, engine_loop_name(l->loop));
        beets_say_cat(t, l->muted ? " MUTED " : " ON ");
        beets_number(l->first + 1, n); beets_say_cat(t, n); beets_say_cat(t, " TO ");
        beets_number(l->last + 1, n); beets_say_cat(t, n); beets_say_cat(t, ". ");
    }
    beets_say_cat(t, beets_ui.focus ? "FOCUS B. " : "FOCUS A. ");
    if (s->lane[beets_ui.focus].bypass) beets_say_cat(t, "BYPASSED. ");
    for (p = 0; p < 5; p++) {
        beets_say_cat(t, names[p]); beets_number(s->lane[beets_ui.focus].probability[p], n);
        beets_say_cat(t, n); beets_say_cat(t, ". ");
    }
    beets_say_cat(t, "BLEND B "); beets_number(s->blend, n); beets_say_cat(t, n); beets_say_cat(t, " PERCENT.");
    ann_say(t, 0, ANN_NOW);
}
void app_frame(void)
{
    beets_input();
    if (ann_on() && ann_where()) beets_speak_screen();
    ann_tick(plat_ms());
    if (plat_ms() - beets_ui.last_draw >= 32) { beets_ui.last_draw = plat_ms(); beets_draw(); }
}
#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    BeetsState s; BeetsLaneState *l; const char *field;
    engine_snapshot(&s);
    if (!strcmp(what, "playing")) *got = s.playing;
    else if (!strcmp(what, "tempo")) *got = s.tempo;
    else if (!strcmp(what, "blend")) *got = s.blend;
    else if (!strcmp(what, "focus")) *got = beets_ui.focus;
    else if (!strcmp(what, "dirty")) *got = beets_ui.dirty;
    else if ((what[0] == 'a' || what[0] == 'b') && what[1] == '_') {
        l = &s.lane[what[0] == 'b']; field = what + 2;
        if (!strcmp(field, "loop")) *got = l->loop;
        else if (!strcmp(field, "queued_loop")) *got = l->queued_loop;
        else if (!strcmp(field, "slice")) *got = l->slice;
        else if (!strcmp(field, "first")) *got = l->first;
        else if (!strcmp(field, "last")) *got = l->last;
        else if (!strcmp(field, "muted")) *got = l->muted;
        else if (!strcmp(field, "bypass")) *got = l->bypass;
        else if (!strcmp(field, "auto")) *got = l->auto_next;
        else if (!strcmp(field, "reverse")) *got = l->force_reverse;
        else if (!strcmp(field, "stutter")) *got = l->force_stutter;
        else if (!strcmp(field, "slice_mask")) *got = (int)l->slice_mask;
        else if (!strcmp(field, "loop_mask")) *got = (int)l->loop_mask;
        else if (!strncmp(field, "prob", 4) && field[4] >= '0' && field[4] <= '4') *got = l->probability[field[4] - '0'];
        else return 0;
    } else return 0;
    return 1;
}
#endif

#include "telemetry_profile.inc"

/* Snapshot work is deferred until the stream actually needs a state message. */
void app_telemetry_snapshot(BeetsState *snapshot, BeetsTelemetryUI *ui)
{
    BP_BEGIN(snap);
    memset(ui, 0, sizeof *ui);
    uint32_t now = plat_ms(), irq = plat_lock();
    engine_snapshot(snapshot);
    ui->clock_phase = engine_clock_phase();
    engine_playback_snapshot(ui->playback);
    plat_unlock(irq);
    BP_END(1, snap);
    ui->focus = beets_ui.focus; ui->dirty = beets_ui.dirty;
    ui->keys = beets_ui.keys; ui->buttons = beets_ui.buttons;
    ui->master = plat_master(); ui->frame_ms = beets_ui.last_draw;
    ui->lights_buttons = beets_ui.lights_buttons; ui->lights_keys = beets_ui.lights_keys;
    ui->dim_buttons = beets_ui.dim_buttons; ui->dim_keys = beets_ui.dim_keys;
    if ((int32_t)(beets_ui.msg_until - now) > 0) {
        ui->message_remaining_ms = beets_ui.msg_until - now;
        memcpy(ui->message, beets_ui.message, sizeof ui->message);
    }
}
void app_loop(void)
{
    BP_BEGIN(total);
#if BEETS_PROFILE
    if (beets_profile_disabled()) { BP_END(0, total); return; }
#endif
    beets_stream_poll(plat_ms());
    BP_END(0, total);
}
