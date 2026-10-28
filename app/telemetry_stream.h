/* SPDX-License-Identifier: MIT */
#pragma once
#include "telemetry.h"
void beets_stream_init(void);
/* Main loop only: input polling records all observed edges, including releases. */
void beets_stream_input(const BeetsTelemetryInput *event);
void beets_stream_changed(void);
void beets_stream_poll(uint32_t ms);
/* Called only when a full state frame is due, outside the audio interrupt. */
void app_telemetry_snapshot(BeetsState *state, BeetsTelemetryUI *ui);
