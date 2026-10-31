#!/usr/bin/env python3
"""Record raw six-channel PCM from one explicitly named device. No MIDI/control writes.
Requires sounddevice. Captures callback timing/status plus nominal/physical format
when a compiled coreaudio-inspect executable exists beside this script.
"""
import argparse, datetime, hashlib, json, pathlib, subprocess, sys, threading, time, wave
import sounddevice as sd
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--device',required=True);p.add_argument('--rate',type=int,choices=[44100],default=44100)
p.add_argument('--seconds',type=float,default=10);p.add_argument('--channels',type=int,default=6)
p.add_argument('--output',type=pathlib.Path,required=True)
p.add_argument('--expected-names',help='Comma-separated CoreAudio channel names to require')
p.add_argument('--usb-product',help='Also read real USB descriptors and GET_CUR during capture')
a=p.parse_args()
if not 1<=a.seconds<=600: p.error('seconds must be 1..600')
choices=[(i,d) for i,d in enumerate(sd.query_devices()) if a.device.casefold() in d['name'].casefold() and d['max_input_channels']>=a.channels]
if len(choices)!=1: p.error(f'need one matching input device; found {choices}')
index,device=choices[0]; target=round(a.seconds*a.rate)
a.output.parent.mkdir(parents=True,exist_ok=True)
blocks=[];timings=[];received=0;done=threading.Event()
def callback(data,frames,timing,status):
 global received
 blocks.append(bytes(data));timings.append({'frames':frames,'adc_time':timing.inputBufferAdcTime,'current_time':timing.currentTime,'status':str(status)})
 received+=frames
 if received>=target: done.set();raise sd.CallbackStop
start=time.monotonic(); inspection=None; usb_inspection=None
with sd.RawInputStream(device=index,samplerate=a.rate,channels=a.channels,dtype='int16',callback=callback) as stream:
 inspector=pathlib.Path(__file__).with_name('coreaudio-inspect')
 if inspector.exists():
  r=subprocess.run([str(inspector),a.device],capture_output=True,text=True)
  inspection=json.loads(r.stdout) if r.returncode==0 else {'error':r.stderr}
  if isinstance(inspection,list):inspection=[d for d in inspection if d.get('input_channels')==a.channels]
 if a.usb_product:
  r=subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('usb-descriptors.py')),'--product',a.usb_product,'--get-rate'],capture_output=True,text=True)
  usb_inspection=json.loads(r.stdout) if r.returncode==0 else {'error':r.stderr or r.stdout}
 if not done.wait(a.seconds+15): raise RuntimeError('capture timed out')
 duration=time.monotonic()-start
 pcm=b''.join(blocks)[:target*a.channels*2]
 if len(pcm)!=target*a.channels*2: raise RuntimeError('short capture')
with wave.open(str(a.output.with_suffix('.wav')),'wb') as f:
 f.setparams((a.channels,2,a.rate,0,'NONE','not compressed'));f.writeframes(pcm)
errors=[t for t in timings if t['status']]
format_errors=[]
if not isinstance(inspection,list) or len(inspection)!=1:
 format_errors.append('one CoreAudio device inspection required')
else:
 device_info=inspection[0]
 if device_info['input_channels']!=a.channels: format_errors.append('physical channel count mismatch')
 physical=[s['physical'] for s in device_info['input_streams']]
 if not physical or any(s.get('rate')!=a.rate for s in physical): format_errors.append('physical rate does not match requested rate')
 if a.expected_names and device_info['channel_names']!=a.expected_names.split(','): format_errors.append('channel names mismatch')
if a.usb_product:
 if not usb_inspection or 'error' in usb_inspection: format_errors.append('USB descriptor/current-rate inspection unavailable')
 elif not any(ep.get('current_rate')==a.rate for ep in usb_inspection['capture_endpoints']): format_errors.append('actual USB GET_CUR rate mismatch or unavailable')
gaps=[(b['adc_time']-x['adc_time'])-x['frames']/a.rate for x,b in zip(timings,timings[1:])]
report={'date':datetime.datetime.now(datetime.timezone.utc).isoformat(),'device':dict(device),'requested_rate':a.rate,'channels':a.channels,'frames':target,'wall_seconds':duration,'status':'PASS' if not errors and not format_errors else 'FAIL','format_errors':format_errors,'callback_errors':errors,'callback_count':len(timings),'adc_gap_max_abs_seconds':max(map(abs,gaps),default=0),'coreaudio_during_stream':inspection,'usb_during_stream':usb_inspection,'pcm_sha256':hashlib.sha256(pcm).hexdigest(),'limitations':['CoreAudio capture; not a USB wire trace','Frame count is requested, not an independent device-rate measurement','Clock/transport controls must be set separately; this tool never changes them']}
a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
a.output.with_suffix('.timing.json').write_text(json.dumps(timings,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(bool(errors or format_errors))
