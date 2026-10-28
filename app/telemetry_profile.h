/* SPDX-License-Identifier: MIT
 * Optional device-only instrumentation. Enable for bounded profiling sessions;
 * disabled builds contain no counters, timer reads or diagnostic MIDI commands. */
#pragma once
#include <stdint.h>
#define BEETS_PROFILE 0
#if BEETS_PROFILE
uint32_t beets_profile_begin(void);
void beets_profile_end(int category, uint32_t start);
int beets_profile_disabled(void);
int beets_profile_send(const uint8_t *frame, uint32_t n);
#define BP_BEGIN(name) uint32_t name = beets_profile_begin()
#define BP_END(category, name) beets_profile_end(category, name)
#define BP_SEND(frame, n) beets_profile_send(frame, n)
#else
#define BP_BEGIN(name) ((void)0)
#define BP_END(category, name) ((void)0)
#define BP_SEND(frame, n) plat_sysex_try_send(frame, n)
#endif
