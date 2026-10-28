/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "dsp/engine.h"
int main(void)
{
    BeetsState before, after;
    BeetsPlaybackState p[2];
    int32_t audio[400];
    engine_init(123);
    engine_playback_snapshot(p);
    assert(!p[0].active && !p[1].active && p[0].position_ppm==0);
    assert(p[0].window_end_ppm==1000000 && p[0].sounding_loop==0 && p[1].sounding_loop==1);
    engine_play(1); engine_render(audio,200,4096);
    engine_snapshot(&before);engine_playback_snapshot(p);engine_snapshot(&after);
    assert(!memcmp(&before,&after,sizeof before));
    assert(engine_clock_phase()==200u*320u);
    assert(p[0].active && p[1].active && p[0].direction==1);
    assert(p[0].position_ppm>=3020 && p[0].position_ppm<=3028);
    assert(p[0].gain_ppm>0 && p[1].gain_ppm==0); /* B muted but phase still advances */
    engine_play(0);engine_playback_snapshot(p);
    assert(!p[0].active && p[0].tail_active);
    engine_loop(0,9);engine_playback_snapshot(p);
    assert(p[0].sounding_loop==0); /* selected loop9 does not alter old release */
    engine_panic();engine_force(0,1,0);engine_trigger(0,4);engine_playback_snapshot(p);
    assert(p[0].sounding_loop==9 && p[0].direction==-1 && p[0].position_ppm==500000);
    assert(p[0].audition_remaining==8269 && p[0].active);
    engine_render(audio,100,4096);engine_playback_snapshot(p);
    assert(p[0].position_ppm<500000 && p[0].audition_remaining==8169);
    engine_panic();engine_force(0,0,1);engine_trigger(0,2);engine_playback_snapshot(p);
    assert(p[0].window_start_ppm==250000 && p[0].window_end_ppm==265625);
    engine_render(audio,200,4096);engine_playback_snapshot(p);
    assert(p[0].position_ppm>=p[0].window_start_ppm && p[0].position_ppm<=p[0].window_end_ppm);
    engine_panic();engine_playback_snapshot(p);
    assert(!p[0].active&&!p[0].tail_active&&p[0].gain_ppm==0&&p[0].audition_remaining==0);
    return 0;
}
