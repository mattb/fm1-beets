/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Beets app contract, adapted from the fm1-base template. */
#pragma once
#include <stdint.h>
#include "plat.h"
#define APP_NAME "BEETS"
#define APP_SPLASH_RGB RGB(220, 238, 110)
#define APP_BLOCK 256u
/* Room for an entire bounded telemetry SysEx; atomic nonblocking enqueue. */
#define SXQ 1024u
#define APP_CANVAS_PX (240u * 64u)
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);

#define APP_LOOP
void app_loop(void);

#include "telemetry_profile.h"
#if BEETS_PROFILE && !defined(PLAT_HOST)
#define APP_SYSEX
int app_sysex(const uint8_t *p, uint32_t n);
#endif
