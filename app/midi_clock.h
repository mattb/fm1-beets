/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
void beets_clock_init(void);
/* Call alongside engine setters/render, under the same lock. */
int beets_clock_receive(uint32_t packet, int port, uint32_t from, uint32_t ms);
void beets_clock_poll(uint32_t ms);
void beets_clock_play(int on);
int beets_clock_external(void); /* 0 internal, 1 USB, 2 TRS */
