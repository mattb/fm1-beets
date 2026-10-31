#!/usr/bin/env python3
"""Muted diagnostic six-channel digital roundtrip. Explicit diagnostic product guard.
Requires sounddevice, numpy, pyusb, libusb and compiled coreaudio-inspect beside this file.
"""
import argparse,json,pathlib,subprocess,sys,threading,time,wave,hashlib
import numpy as np
import sounddevice as sd
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--device',default='FM1 USB Audio Test');p.add_argument('--seconds',type=float,default=10)
p.add_argument('--signal',choices=['tones','impulses','alignment'],default='tones');p.add_argument('--mask',type=int,default=63)
p.add_argument('--blocksize',type=int,default=0);p.add_argument('--mono-wallclock',action='store_true');p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
assert 3<=a.seconds<=600 and 0<=a.mask<64
root=pathlib.Path(__file__).resolve().parent
def probe():
 return json.loads(subprocess.check_output([sys.executable,str(root/'usb-descriptors.py'),'--product','FM1 USB Audio Test','--get-rate'],text=True))
identity=probe();assert identity['product']=='FM1 USB Audio Test'
devices=sd.query_devices()
inputs=[(i,d) for i,d in enumerate(devices) if a.device in d['name'] and d['max_input_channels']==6]
outputs=[(i,d) for i,d in enumerate(devices) if a.device in d['name'] and d['max_output_channels']==6]
assert len(inputs)==1 and len(outputs)==1,(inputs,outputs)
input_index,input_device=inputs[0];output_index,output_device=outputs[0]
index=(input_index,output_index);device={'input':dict(input_device),'output':dict(output_device)};rate=44100;target=round(a.seconds*rate);position=0;out_position=0;blocks=[];timings=[];out_timings=[];done=threading.Event()
pulse_positions=[];pos=4096
while pos<target-rate//2:
 pulse_positions.append(pos);pos+=8820+(len(pulse_positions)*997)%4410
pulse_positions=np.array(pulse_positions)
freq=np.array([331,487,683,947,1289,1747]);active=np.array([(a.mask>>i)&1 for i in range(6)])
def signal(start,count):
 if a.mono_wallclock:
  y=np.zeros((count,6),dtype='<i2');positions=pulse_positions[(pulse_positions>=start)&(pulse_positions<start+count)]
  y[positions-start,0]=4125;return y
 n=np.arange(start,start+count)[:,None]
 if a.signal=='tones':return np.rint(4125*np.sin(2*np.pi*n*freq[None,:]/rate)*active).astype('<i2')
 offsets=np.arange(6)*97 if a.signal=='impulses' else np.zeros(6,dtype=int)
 return (((n-offsets[None,:]-2048)%11025==0)*4125*active).astype('<i2')
def input_callback(indata,frames,timing,status):
 global position
 stamp=time.perf_counter_ns()
 blocks.append(bytes(indata));timings.append({'frame':position,'frames':frames,'wall_ns':stamp,'adc':timing.inputBufferAdcTime,'status':str(status)})
 position+=frames
 if position>=target:done.set();raise sd.CallbackStop
def output_callback(outdata,frames,timing,status):
 global out_position
 stamp=time.perf_counter_ns()
 outdata[:]=signal(out_position,frames).tobytes();out_timings.append({'frame':out_position,'frames':frames,'wall_ns':stamp,'dac':timing.outputBufferDacTime,'status':str(status)})
 out_position+=frames
start=time.monotonic()
with sd.RawInputStream(device=input_index,samplerate=rate,channels=6,dtype='int16',blocksize=a.blocksize,callback=input_callback) as ins, sd.RawOutputStream(device=output_index,samplerate=rate,channels=6,dtype='int16',blocksize=a.blocksize,callback=output_callback) as outs:
 latency=(ins.latency,outs.latency)
 core=json.loads(subprocess.check_output([str(root/'coreaudio-inspect'),a.device],text=True));usb=probe()
 if not done.wait(a.seconds+15):raise RuntimeError('capture timeout')
pcm=b''.join(blocks)[:target*12];a.output.parent.mkdir(parents=True,exist_ok=True)
for suffix,data in [('.wav',pcm),('.sent.wav',signal(0,target).tobytes())]:
 with wave.open(str(a.output)+suffix,'wb') as f:f.setparams((6,2,rate,0,'NONE','not compressed'));f.writeframes(data)
errors=[]
for direction,names in [('input',['Tone '+str(x) for x in freq]),('output',['Input '+str(i) for i in range(1,7)])]:
 matching=[d for d in core if d[direction+'_channels']==6]
 if len(matching)!=1:errors.append(direction+' expected one CoreAudio device');continue
 info=matching[0]
 if info[direction+'_channel_names']!=names:errors.append(direction+' names differ')
 streams=info[direction+'_streams']
 if not streams or any(s['physical'].get('channels')!=6 or s['physical'].get('rate')!=rate for s in streams):errors.append(direction+' physical format differs')
for direction in ['capture','playback']:
 endpoints=usb[direction+'_endpoints']
 if len(endpoints)!=1:errors.append(direction+' expected one USB endpoint')
 elif direction=='capture' and endpoints[0].get('current_rate')!=rate:errors.append('capture USB GET_CUR differs/unavailable')
formats=[r['pcm_format'] for r in usb['configuration'] if 'pcm_format' in r]
if len(formats)!=2 or any(f['channels']!=6 or f['rates']!=[44100] for f in formats):errors.append('USB formats not fixed6channel44100')
if any(t['status'] and t['frame']>=rate//2 for t in timings+out_timings):errors.append('host callback status flags after0.5s priming')
r={'result':'FAIL' if errors else 'PASS','errors':errors,'signal':'mono-wallclock' if a.mono_wallclock else a.signal,'pulse_positions':pulse_positions.tolist() if a.mono_wallclock else None,'requested_blocksize':a.blocksize,'actual_callback_frames':sorted(set(t['frames'] for t in timings)),'portaudio_latency_seconds':latency,'mask':a.mask,'frames':target,'rate':rate,'wall_seconds':time.monotonic()-start,'device':dict(device),'coreaudio':core,'usb':usb,'callback_errors':[t for t in timings+out_timings if t['status'] and t['frame']>=rate//2],'startup_callback_flags':[t for t in timings+out_timings if t['status'] and t['frame']<rate//2],'pcm_sha256':hashlib.sha256(pcm).hexdigest(),'limits':['Diagnostic must already be in loopback mode2; analog muted by firmware contract','Fixed-rate playback endpoint has no GET_CUR; verified via descriptor and physical CoreAudio format','First0.5s startupflags reported separately; PCM analysis excludes priming','No firmware counters or analog measurement','Host callback timing is not a USB wire trace']}
a.output.with_suffix('.json').write_text(json.dumps(r,indent=2)+'\n');a.output.with_suffix('.timing.json').write_text(json.dumps({'input':timings,'output':out_timings},indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(bool(errors))
