/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include "dsp/engine.h"
#define BEETS_TELEMETRY_CAPACITY 3072u
#define BEETS_TELEMETRY_VERSION 1u
/* UTF-8 message may occupy all 48 bytes; serializer never reads past it. */
typedef struct {
    int focus, dirty, clock_source, output_split;
    uint32_t keys, buttons, frame_ms, clock_phase, message_remaining_ms;
    uint32_t master, lights_buttons, lights_keys, dim_buttons, dim_keys;
    uint32_t input_dropped;
    char message[48];
    BeetsPlaybackState playback[BEETS_LANES];
} BeetsTelemetryUI;
typedef struct {
    uint32_t ms, keys_down, keys_up, buttons_down, buttons_up;
    int32_t enc[7];
} BeetsTelemetryInput;
/* Return complete SysEx frame length or 0 on invalid arguments/overflow.
 * On failure buffer contents are unspecified: callers MUST NOT transmit them.
 * Framing: F0 7D 'B' 'E' 'E' 'T' 'S' 01 <ASCII JSON> F7. No LF or NUL.
 * Caller captures state/UI atomically, then serializes outside the audio lock.
 * Pure, stateless, allocation-free; it does not read globals or call engine code. */
uint32_t beets_telemetry_serialize(uint8_t *out, uint32_t capacity,
    const BeetsState *state, const BeetsTelemetryUI *ui, uint32_t seq, uint32_t ms);
uint32_t beets_telemetry_input_serialize(uint8_t *out, uint32_t capacity,
    const BeetsTelemetryInput *input, uint32_t seq);
