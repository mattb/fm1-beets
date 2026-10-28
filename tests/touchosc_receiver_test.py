#!/usr/bin/env python3
"""Replay hardware JSON through the actual Lua callback with mocked TouchOSC objects.
Native TouchOSC JSON parsing and USB reception are verified separately in-app.
"""
import json, subprocess, tempfile, zlib
from pathlib import Path
import xml.etree.ElementTree as E
root=Path(__file__).resolve().parents[1]
def lua(v):
    if isinstance(v,dict): return '{'+','.join('['+lua(k)+']='+lua(x) for k,x in v.items())+'}'
    if isinstance(v,list): return '{'+','.join(map(lua,v))+'}'
    if isinstance(v,str): return json.dumps(v,ensure_ascii=False)
    if v is None: return 'nil'
    if isinstance(v,bool): return 'true' if v else 'false'
    return str(v)
x=E.fromstring(zlib.decompress((root/'touchosc/Beets.tosc').read_bytes()))
nodes={}
for n in x.findall('./node/children/node'):
    p={p.findtext('key'):p.find('value') for p in n.findall('./properties/property')}
    nodes[p['name'].text]={'values':{'text':''},'frame':{k:float(p['frame'].findtext(k)) for k in 'xywh'}}
source=(root/'touchosc/beets.lua').read_text()
assert x.find("./node/properties/property[key='script']/value").text==source
frames=[json.loads(s) for s in (root/'build/tests/usb-telemetry.jsonl').read_text().splitlines()]
script='controls='+lua(nodes)+'''
now=0
function Color(...) return {...} end
function getMillis() return now end
self={findByName=function(_,name) assert(controls[name],name); return controls[name] end}
json={toTable=function(_) return fixture end}
pcall=nil
'''+source+'\n'
for f in frames:
    body=json.dumps(f,ensure_ascii=True,separators=(',',':'))
    wire=list(b'\xf0\x7dBEETS\x01'+body.encode()+b'\xf7')
    script+='fixture='+lua(f)+'; now='+str(f['ms'])+'; assert(onReceiveMIDI('+lua(wire)+',{})==true); update()\n'
    if f['type']=='state':
        script+='assert(controls.tempo.values.text=='+lua(str(f['tempo'])+' BPM')+')\n'
        script+='assert(controls.aloop.values.text=='+lua(f['lanes'][0]['loop_name'])+')\n'
script+='''
assert(controls.connection.values.text=='USB MIDI / LIVE')
assert(string.find(controls.diagnostics.values.text,'GAPS 0',1,true))
assert(onReceiveMIDI({240,125,70,77,1,123,125,247},{})==nil)
fixture=nil
assert(onReceiveMIDI({240,125,66,69,69,84,83,1,123,247},{})==true)
now=now+4000; update()
assert(controls.connection.values.text=='STALE / CHECK USB')
print('PASS: hardware frame replay, UI mapping, header filtering, invalid decode, stale timeout; no pcall')
'''
with tempfile.NamedTemporaryFile(mode='w',suffix='.lua') as f:
    f.write(script); f.flush(); subprocess.run(['lua',f.name],check=True)
