/* SPDX-License-Identifier: MIT */
#pragma once
#include "telemetry.h"
void beets_stream_init(void);
/* Main loop only: input polling records all observed edges, including releases. */
void beets_stream_input(const BeetsTelemetryInput *event);
void beets_stream_changed(void);
void beets_stream_poll(const BeetsState *state, BeetsTelemetryUI *ui, uint32_t ms);
