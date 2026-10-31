#!/usr/bin/env python3
"""Set only documented muted diagnostic modes, on an exact product MIDI port."""
import argparse,mido,json,subprocess,sys,pathlib
p=argparse.ArgumentParser();p.add_argument('--port',help='Exact MIDI port when macOS caches an older name');p.add_argument('--mode',type=int,choices=[0,1,2],required=True);p.add_argument('--mask',type=int,choices=range(64),default=63);a=p.parse_args()
probe=subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('usb-descriptors.py')),'--product','FM1 USB Audio Test'],capture_output=True,text=True,check=True)
assert json.loads(probe.stdout)['product']=='FM1 USB Audio Test'
ports=[s for s in mido.get_output_names() if (s==a.port if a.port else 'FM1 USB Audio Test' in s)]
if len(ports)!=1:raise SystemExit(f'Expected one diagnostic port; found {ports}')
with mido.open_output(ports[0]) as port:
 port.send(mido.Message('control_change',channel=0,control=20,value=a.mode))
 port.send(mido.Message('control_change',channel=0,control=21,value=a.mask))
print(f'{ports[0]}: mode={a.mode} mask={a.mask}')
