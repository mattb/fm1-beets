#!/usr/bin/env python3
"""Analyze controlled diagnostic PCM captures; requires numpy. No device access."""
import argparse,json,pathlib,wave
import numpy as np
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('wav',type=pathlib.Path);p.add_argument('--mode',choices=['tones','alignment'],default='tones')
p.add_argument('--frequencies',default='331,487,683,947,1289,1747')
p.add_argument('--mask',type=lambda x:int(x,0),default=63);p.add_argument('--skip',type=float,default=.5)
p.add_argument('--output',type=pathlib.Path)
p.add_argument('--expected-peak-dbfs',type=float,help='Require sine RMS within 1 dB of this peak level (diagnostic: -18)')
a=p.parse_args()
with wave.open(str(a.wav)) as w:
 assert w.getsampwidth()==2,'PCM16 expected'
 rate=w.getframerate();channels=w.getnchannels();pcm=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').reshape(-1,channels).astype(float)
pcm=pcm[round(a.skip*rate):];assert len(pcm)>rate,'need >1 second after settling'
freqs=list(map(float,a.frequencies.split(',')));assert channels==len(freqs)
fail=[];rows=[]
if a.mode=='alignment':
 peaks=np.max(np.abs(pcm),axis=0)
 deltas=np.max(np.abs(pcm-pcm[:,0,None]),axis=0)
 if np.min(peaks)<100:fail.append('alignment signal missing')
 if np.max(deltas)>2:fail.append('channels differ by more than two PCM16 counts')
 result={'peaks':peaks.tolist(),'max_difference_from_ch1_pcm16':deltas.tolist(),'scope':'Identical-input capture equality checks interchannel alignment; not audio/USB latency.'}
else:
 # A long FFT resolves actual oscillator frequency despite device/host clock drift.
 n=min(len(pcm),rate*8);win=np.hanning(n);bins=np.fft.rfftfreq(n,1/rate)
 spectra=np.abs(np.fft.rfft(pcm[:n]*win[:,None],axis=0))
 for ch,expected in enumerate(freqs):
  y=pcm[:,ch];rms=float(np.sqrt(np.mean(y*y)));peak=float(np.max(np.abs(y)));active=bool(a.mask&(1<<ch))
  if not active:
   if peak>2:fail.append(f'channel {ch+1} should be silent')
   rows.append({'channel':ch+1,'active':False,'peak_pcm16':peak,'rms_pcm16':rms});continue
  band=np.flatnonzero(np.abs(bins-expected)<=expected*.01+2)
  k=int(band[np.argmax(spectra[band,ch])]);actual=float(bins[k]);own=float(spectra[k,ch])
  unwanted=[]
  for other in freqs:
   if other==expected:continue
   ix=np.abs(bins-other)<=other*.01+2
   unwanted.append(float(np.max(spectra[ix,ch])))
  leakage_db=float(20*np.log10(max(max(unwanted),1e-12)/max(own,1e-12)))
  chunk=max(1,rate//100);blocks=y[:len(y)//chunk*chunk].reshape(-1,chunk)
  minimum_rms=float(np.min(np.sqrt(np.mean(blocks*blocks,axis=1))))
  # A sine cannot jump farther than its analytic per-sample maximum; allow margin
  # for interpolation/quantization. This detects many, not all, missing packets.
  jump_limit=2*peak*np.sin(np.pi*actual/rate)*1.5+4
  jumps=int(np.count_nonzero(np.abs(np.diff(y))>jump_limit))
  if a.expected_peak_dbfs is not None:
   expected_rms=32768*10**(a.expected_peak_dbfs/20)/np.sqrt(2)
   if abs(20*np.log10(max(rms,1e-12)/expected_rms))>1:fail.append(f'channel {ch+1} RMS level differs by more than 1 dB')
  if rms<100:fail.append(f'channel {ch+1} too quiet')
  if abs(actual/expected-1)>.005:fail.append(f'channel {ch+1} frequency error')
  if leakage_db>-50:fail.append(f'channel {ch+1} other-tone-band leakage > -50 dB')
  if minimum_rms<rms*.5:fail.append(f'channel {ch+1} quiet/dropout window')
  if jumps:fail.append(f'channel {ch+1} discontinuities')
  rows.append({'channel':ch+1,'active':True,'expected_hz':expected,'measured_hz':actual,'peak_pcm16':peak,'rms_pcm16':rms,'other_tone_leakage_db':leakage_db,'min_10ms_rms':minimum_rms,'large_sample_jumps':jumps})
 result={'channels':rows}
report={'result':'FAIL' if fail else 'PASS','wav':str(a.wav),'mode':a.mode,'sample_rate':rate,'frames_analyzed':len(pcm),'failures':fail,'analysis':result,'limits':['Host conversion may introduce <=2 PCM16 counts of difference','No analog output measurement','No proof against every possible dropped/repeated packet']}
path=a.output or a.wav.with_suffix('.analysis.json');path.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(bool(fail))
