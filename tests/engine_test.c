/* SPDX-License-Identifier: MIT
 * Behavioral regression tests for the port's musical contract. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"

static BeetsState state(void) { BeetsState s; engine_snapshot(&s); return s; }
static void frames(unsigned count) {
    int32_t out[512];
    while(count) { unsigned n=count>256?256:count; engine_render(out,n,4096); count-=n; }
}
static BeetsState tick(void) {
    BeetsState before=state(); unsigned i; int32_t out[2];
    for(i=0;i<100000;i++) { engine_render(out,1,4096); if(state().ticks != before.ticks) return state(); }
    assert(!"clock failed to advance"); return state();
}
static void straight_and_mutations(void) {
    engine_init(123); engine_play(1);
    BeetsState s=state(); assert(s.playing && s.step==0 && s.lane[0].slice==0);
    for(int i=1;i<24;i++) { s=tick(); assert(s.step==i%8); assert(s.lane[0].slice==i%8); assert(s.lane[0].events==0); }
    engine_init(123); engine_probability(0,BEETS_SKIP,100); engine_play(1);
    for(int i=1;i<24;i++) { s=tick(); assert(s.lane[0].slice==(2*(i%8))%8); }
    engine_init(123); engine_probability(0,BEETS_REPEAT,100); engine_play(1);
    for(int i=1;i<24;i++) { s=tick(); assert(s.lane[0].slice==0); }
    engine_probability(0,BEETS_SKIP,100);
    for(int i=0;i<24;i++) { int prev=state().lane[0].slice; s=tick(); assert(s.lane[0].slice==(s.step==0?0:(prev+1)%8)); }
    puts("PASS natural order, bar anchor, repeat, skip, combined probabilities");
}
static void range_masks_and_queue(void) {
    engine_init(9); engine_range(0,6,2); engine_play(1);
    BeetsState s=state();
    assert(s.lane[0].first<=s.lane[0].last);
    for(int i=0;i<20;i++) { s=tick(); assert(s.lane[0].slice>=s.lane[0].first && s.lane[0].slice<=s.lane[0].last); }
    engine_range(0,3,3);
    for(int i=0;i<10;i++) { s=tick(); assert(s.lane[0].slice==3); }
    engine_masks(0,0,0);
    for(int i=0;i<16;i++) { s=tick(); assert(s.lane[0].slice_mask); assert(s.lane[0].loop_mask); }
    engine_range(0,0,7); engine_masks(0,(1u<<2)|(1u<<5),1u<<0);
    for(int i=0;i<24;i++) { s=tick(); assert(s.lane[0].slice==2 || s.lane[0].slice==5); }
    engine_probability(0,BEETS_SWITCH,100); engine_auto(0,1);
    engine_loop(0,7); s=tick(); assert(s.lane[0].loop==7);
    engine_trigger(0,5); s=tick(); assert(s.lane[0].slice==5);
    puts("PASS reversed/single ranges, empty/sparse masks, quantized sample and cue");
}
static void render_in_chunks(int32_t *out,unsigned frames_n,unsigned chunk) {
    for(unsigned i=0;i<frames_n;) { unsigned n=frames_n-i; if(n>chunk)n=chunk; engine_render(out+2*i,n,4096); i+=n; }
}
static void configure_random(void) {
    engine_init(0x12abcdef); engine_mute(1,0); engine_blend(50);
    for(int d=0;d<2;d++) for(int p=0;p<BEETS_PROBS;p++) engine_probability(d,p,37+p*11);
    engine_play(1);
}
static void audio_and_determinism(void) {
    enum { N=44100*3 };
    int32_t *a=malloc(N*2*sizeof(*a)), *b=malloc(N*2*sizeof(*b)); assert(a&&b);
    configure_random(); render_in_chunks(a,N,128);
    BeetsState expected=state(); configure_random(); render_in_chunks(b,N,257);
    assert(!memcmp(a,b,N*2*sizeof(*a))); BeetsState actual=state();
    assert(expected.rng==actual.rng && expected.ticks==actual.ticks);
    int32_t peak=0;
    for(int i=0;i<N*2;i++) { int64_t v=a[i]; if(v<0)v=-v; assert(v<=8388607); if(v>peak)peak=(int32_t)v; }
    assert(peak>10000);
    engine_play(0); frames(4410); engine_render(a,1024,4096);
    for(int i=0;i<2048;i++)assert(a[i]==0);
    free(a);free(b);
    puts("PASS nonzero bounded audio, seeded block-size invariance, transport silence");
}
static void stress(void) {
    int32_t out[514];
    for(int loop=0;loop<BEETS_LOOPS;loop++) for(int tempo_i=0;tempo_i<2;tempo_i++) {
        engine_init(100+loop); engine_tempo(tempo_i?10000:-10000); engine_mute(1,0);
        engine_loop(0,loop); engine_loop(1,loop); engine_force(0,1,1); engine_force(1,1,1);
        for(int d=0;d<2;d++) for(int p=0;p<BEETS_PROBS;p++) engine_probability(d,p,100);
        engine_play(1);
        int64_t energy=0;
        for(int i=0;i<200;i++) { engine_render(out,257,4096); for(int j=0;j<514;j++) { assert(out[j]>=-8388608&&out[j]<=8388607); energy += out[j]<0?-(int64_t)out[j]:out[j]; } }
        assert(energy>10000);
        BeetsState s=state(); assert(s.tempo>0&&s.tempo<1000);
    }
    puts("PASS all ten samples, tempo extremes, reverse/stutter bounds, two-deck mix");
}
static void stopped_sample_change(void) {
    int32_t out[2]; engine_init(42); engine_trigger(0,7); frames(7500);
    engine_loop(0,8); // shorter Amen file while the previous audition tail fades
    for(int i=0;i<5000;i++) { engine_render(out,1,4096); assert(out[0]>=-4000000&&out[0]<=4000000); }
    puts("PASS shorter sample selection during stopped audition tail");
}
static void panic_and_stop_audition(void) {
    int32_t out[512];
    engine_init(42);engine_trigger(0,0);frames(1000);engine_play(0);frames(4410);engine_render(out,256,4096);
    for(int i=0;i<512;i++)assert(out[i]==0);
    engine_trigger(0,0);frames(1000);engine_panic();engine_render(out,256,4096);
    for(int i=0;i<512;i++)assert(out[i]==0);
    engine_play(1);frames(1000);engine_panic();engine_render(out,256,4096);
    for(int i=0;i<512;i++)assert(out[i]==0);
    assert(state().playing==0);
    puts("PASS idempotent stop cancels audition; update panic silences immediately");
}
int main(void) {
    straight_and_mutations(); range_masks_and_queue(); audio_and_determinism(); stress(); stopped_sample_change(); panic_and_stop_audition();
    puts("ALL ENGINE TESTS PASSED"); return 0;
}
