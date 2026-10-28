#!/usr/bin/env python3
"""Profile a BEETS_PROFILE=1 device build in bounded measurement windows.
Temporarily operates transport and test-only telemetry modes; restores transport.
Reports elapsed scope time (includes ISR preemption), not exclusive CPU cycles.
"""
import argparse, json, time
from pathlib import Path
import mido
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',default='X0X FM-1'); p.add_argument('--seconds',type=float,default=8)
p.add_argument('--output',required=True)
a=p.parse_args()
if not 1<=a.seconds<=30: p.error('seconds must be between 1 and 30')
results=[]; initial=None; last=None
with mido.open_input(a.port) as inp,mido.open_output(a.port) as out:
 def send(mode): out.send(mido.Message('sysex',data=[0x7d,66,80,mode]))
 def transport(playing):
  out.send(mido.Message('stop')); receive(.7)
  if playing:
   out.send(mido.Message('sysex',data=[0x7d,0x4c,0x43,1,0,8])); receive(.15)
   out.send(mido.Message('sysex',data=[0x7d,0x4c,0x43,1,0,0])); receive(.15)
 def receive(seconds):
  global last
  reports=[]; deadline=time.monotonic()+seconds
  while time.monotonic()<deadline:
   for msg in inp.iter_pending():
    if msg.type!='sysex': continue
    b=bytes(msg.data)
    if b.startswith(b'\x7dBEETS\x01'):
     obj=json.loads(b[7:]);
     if obj.get('type')=='state': last=obj
    elif b.startswith(b'\x7dBP\x01'):
     values=[sum(b[i+j]<<(7*j) for j in range(5)) for i in range(4,len(b),5)]
     reports.append(values)
   time.sleep(.002)
  return reports
 try:
  receive(1.2); initial=last
  for playing in (0,1):
   transport(playing)
   for label,mode in [('normal',0),('disabled',2),('blocked',3)]:
    send(mode); receive(a.seconds);send(1); reports=receive(.3)
    if not reports: raise RuntimeError('No BP profile reply; install BEETS_PROFILE=1 firmware')
    v=reports[-1]; elapsed=v[0]/24e6
    row={'transport':'playing' if playing else 'stopped','mode':label,'seconds':elapsed,'scopes':{}}
    for i,name in enumerate(['app_loop','snapshot','serialize','send']):
     count,total,maximum=v[1+i*3:4+i*3]
     row['scopes'][name]={'calls':count,'calls_per_second':count/elapsed,'elapsed_share_pct':total/v[0]*100,'mean_us':total/24/count if count else 0,'max_us':maximum/24}
    row.update(rejected=v[13],max_irq_lock_us=v[14],audio_xruns=v[15],audio_render_pct=v[16])
    results.append(row); print(json.dumps(row),flush=True)
 finally:
  send(0);transport(bool(initial and initial['playing']))
Path(a.output).parent.mkdir(parents=True,exist_ok=True)
Path(a.output).write_text(json.dumps(results,indent=2)+'\n')
