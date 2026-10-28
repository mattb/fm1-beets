/* SPDX-License-Identifier: MIT
 * Included only by the device app unity TU. 24 MHz TIMER4, unsigned differences.
 * BP command 0 resets/enables, 1 reports, 2 disables telemetry, 3 forces would-block.
 * Report: F0 7D 'B' 'P' 1, uint32 values in five low-first 7-bit groups, F7.
 * Categories: whole loop, full snapshot, serializer, USB enqueue; each count/sum/max.
 * Elapsed scopes include interrupt preemption; sums require sessions <178 seconds. */
#if BEETS_PROFILE && !defined(PLAT_HOST)
static uint32_t bp_count[4], bp_sum[4], bp_max[4], bp_since, bp_rejected, bp_xruns;
static int bp_mode;
uint32_t beets_profile_begin(void) { return fm1_ticks(); }
void beets_profile_end(int c, uint32_t start)
{
    uint32_t dt = fm1_ticks() - start;
    bp_count[c]++; bp_sum[c] += dt;
    if (dt > bp_max[c]) bp_max[c] = dt;
}
int beets_profile_disabled(void) { return bp_mode == 2; }
int beets_profile_send(const uint8_t *frame, uint32_t n)
{
    int r; BP_BEGIN(t);
    r = bp_mode == 3 ? -1 : plat_sysex_try_send(frame, n);
    BP_END(3, t); if (r) bp_rejected++; return r;
}
int app_sysex(const uint8_t *p, uint32_t n)
{
    uint32_t values[17], i, j, k = 5;
    uint8_t out[91] = {0xf0, 0x7d, 'B', 'P', 1};
    if (n != 4 || p[0] != 0x7d || p[1] != 'B' || p[2] != 'P') return 0;
    if (p[3] != 1) {
        bp_mode = p[3]; bp_since = fm1_ticks(); bp_rejected = 0;
        memset(bp_count, 0, sizeof bp_count); memset(bp_sum, 0, sizeof bp_sum); memset(bp_max, 0, sizeof bp_max);
        lock_max = 0; bp_xruns = audio_xruns;
        return 1;
    }
    values[0] = fm1_ticks() - bp_since;
    for (i = 0; i < 4; i++) { values[1+i*3] = bp_count[i]; values[2+i*3] = bp_sum[i]; values[3+i*3] = bp_max[i]; }
    values[13] = bp_rejected; values[14] = lock_max; values[15] = audio_xruns - bp_xruns; values[16] = audio_cpu_pct;
    for (i = 0; i < 17; i++) for (j = 0; j < 5; j++) out[k++] = (values[i] >> (j*7)) & 127;
    out[k++] = 0xf7; plat_sysex_send(out,k); return 1;
}
#endif
