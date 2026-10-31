#!/usr/bin/env python3
"""Separate application wallclock, sample timeline and ADC/DAC timestamp roundtrips."""
import argparse,pathlib,json,wave,numpy as np
p=argparse.ArgumentParser();p.add_argument('prefix',type=pathlib.Path);a=p.parse_args()
r=json.loads(a.prefix.with_suffix('.json').read_text());timings=json.loads(a.prefix.with_suffix('.timing.json').read_text());inputs=timings['input'];outputs=timings['output'];rate=r['rate']
with wave.open(str(a.prefix)+'.wav') as w:x=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,6).astype(float)
with wave.open(str(a.prefix)+'.sent.wav') as w:sent=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,6)[:,0].astype(float)
n=1<<(2*len(sent)-1).bit_length();corr=np.fft.irfft(np.fft.rfft(x[:,0],n)*np.conj(np.fft.rfft(sent,n)),n)
lag=int(np.argmax(corr[:rate//5]));ix=np.flatnonzero(np.abs(x[:,0])>500);groups=np.split(ix,np.flatnonzero(np.diff(ix)>8)+1) if len(ix) else []
peaks=np.array([g[np.argmax(np.abs(x[g,0]))] for g in groups]);in_starts=np.array([t['frame'] for t in inputs]);out_starts=np.array([t['frame'] for t in outputs]);rows=[]
for identity,outpos in enumerate(r['pulse_positions']):
 if outpos<rate//2 or outpos>len(x)-rate//2:continue
 if not len(peaks):continue
 inpos=int(peaks[np.argmin(np.abs(peaks-(outpos+lag)))])
 if abs(inpos-outpos-lag)>512:continue
 ot=outputs[int(np.searchsorted(out_starts,outpos,side='right')-1)];it=inputs[int(np.searchsorted(in_starts,inpos,side='right')-1)]
 rows.append({'pulse':identity,'sent_frame':outpos,'received_frame':inpos,'wallclock_ms':(it['wall_ns']-ot['wall_ns'])/1e6,'sample_lag_ms':(inpos-outpos)/rate*1000,'adc_dac_estimate_ms':((it['adc']+(inpos-it['frame'])/rate)-(ot['dac']+(outpos-ot['frame'])/rate))*1000})
def stats(key):
 v=np.array([row[key] for row in rows]);return {'min':float(v.min()),'median':float(np.median(v)),'p95':float(np.percentile(v,95)),'max':float(v.max())} if len(v) else None
errors=[]
if len(rows)<20:errors.append('fewer than20 matched pulses')
if rows and (min(row['wallclock_ms'] for row in rows)<0 or max(row['wallclock_ms'] for row in rows)>200):errors.append('wallclock delay outside0..200ms')
if np.max(np.abs(x[rate//2:,1:]))>2:errors.append('other channels not silent')
report={'result':'FAIL' if errors else 'PASS','errors':errors,'matched_pulses':len(rows),'requested_blocksize':r['requested_blocksize'],'actual_callback_frames':r['actual_callback_frames'],'portaudio_latency_seconds':r['portaudio_latency_seconds'],'application_wallclock_ms':stats('wallclock_ms'),'sample_timeline_lag_ms':stats('sample_lag_ms'),'adc_dac_timestamp_estimate_ms':stats('adc_dac_estimate_ms'),'crosscorrelation_lag_frames':lag,'pulses':rows,'limits':['Wallclock uses perf_counter_ns at output callback handoff and input callback receipt; includes host buffering and scheduling','ADC/DAC estimates use PortAudio CoreAudio timestamps; not independently measured hardware timestamps','Sample lag includes the independent streams starting at different times; it is not wallclock latency','Digital USB loopback only; no analog path or firmware-only latency claim']}
a.prefix.with_suffix('.wallclock.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(bool(errors))
