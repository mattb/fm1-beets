/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "telemetry.h"
static void emit(const uint8_t *p, uint32_t n) {
    uint8_t length[4] = {(uint8_t)n,(uint8_t)(n>>8),(uint8_t)(n>>16),(uint8_t)(n>>24)};
    assert(fwrite(length,1,4,stdout)==4); assert(fwrite(p,1,n,stdout)==n);
}
int main(void) {
    uint8_t out[BEETS_TELEMETRY_CAPACITY], tiny[34];
    BeetsState s = {0}; BeetsTelemetryUI ui = {0}; BeetsTelemetryInput in = {0};
    uint32_t n; int i,p;
    s.tempo=160;s.blend=50;s.playing=1;s.step=7;s.frames=UINT32_MAX;s.ticks=UINT32_MAX;s.rng=UINT32_MAX;
    for(i=0;i<2;i++) { s.lane[i].loop=i+8;s.lane[i].queued_loop=-1;s.lane[i].queued_slice=-1;s.lane[i].last=7;
        s.lane[i].slice_mask=255;s.lane[i].loop_mask=1023;for(p=0;p<5;p++)s.lane[i].probability[p]=p*25;
    }
    ui.playback[0].position_ppm=500000;ui.playback[0].direction=-1;ui.playback[0].active=1;ui.playback[0].sounding_loop=9;
    ui.focus=1;ui.dirty=1;ui.clock_phase=2645999;ui.master=4096;ui.input_dropped=UINT32_MAX;
    strcpy(ui.message,"caf\xc3\xa9 \xe2\x99\xab \xf0\x9f\xa5\x81 \"\\\n\t");
    n=beets_telemetry_serialize(out,sizeof out,&s,&ui,UINT32_MAX,UINT32_MAX);assert(n);emit(out,n);
    memset(tiny,0xa5,sizeof tiny);
    assert(!beets_telemetry_serialize(tiny+1,32,&s,&ui,0,0));assert(tiny[0]==0xa5&&tiny[33]==0xa5);
    assert(!beets_telemetry_serialize(NULL,0,&s,&ui,0,0));
    assert(!beets_telemetry_serialize(out,sizeof out,NULL,&ui,0,0));
    assert(!beets_telemetry_serialize(out,sizeof out,&s,NULL,0,0));
    assert(beets_telemetry_serialize(out,n,&s,&ui,UINT32_MAX,UINT32_MAX)==n);
    assert(!beets_telemetry_serialize(out,n-1,&s,&ui,UINT32_MAX,UINT32_MAX));
    /* Bounded non-terminated message and maximum signed values remain representable. */
    memset(ui.message,1,sizeof ui.message);s.tempo=INT_MIN;s.blend=INT_MAX;
    n=beets_telemetry_serialize(out,sizeof out,&s,&ui,1,0);assert(n);emit(out,n);
    memset(ui.message,0,sizeof ui.message);memcpy(ui.message,"\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80\xe2\x82",11);
    n=beets_telemetry_serialize(out,sizeof out,&s,&ui,2,0);assert(n);emit(out,n);
    in.ms=UINT32_MAX;in.keys_down=UINT32_MAX;in.buttons_up=UINT32_MAX;in.enc[0]=INT_MIN;in.enc[6]=INT_MAX;
    n=beets_telemetry_input_serialize(out,sizeof out,&in,0);assert(n);emit(out,n);
    assert(!beets_telemetry_input_serialize(out,1,&in,0));
    assert(!beets_telemetry_input_serialize(out,sizeof out,NULL,0));
    return 0;
}
