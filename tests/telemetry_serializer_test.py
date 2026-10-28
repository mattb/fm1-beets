#!/usr/bin/env python3
"""Strict JSON, Unicode, integer, SysEx and bounds checks, with ASan/UBSan."""
import json
from pathlib import Path
import struct
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='beets-json-test-') as tmp:
    exe = Path(tmp) / 'serializer-test'
    subprocess.run(['cc', '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I'+str(ROOT/'app'),
        str(ROOT/'tests/telemetry_serializer_test.c'), str(ROOT/'app/telemetry.c'),
        str(ROOT/'app/vendor/jems/jems.c'), '-o', str(exe)], check=True)
    raw = subprocess.check_output([str(exe)])
frames=[]
while raw:
    size, = struct.unpack('<I',raw[:4]); frame=raw[4:4+size];raw=raw[4+size:]
    assert len(frame)==size and frame[:8]==b'\xf0\x7dBEETS\x01' and frame[-1]==0xf7
    body=frame[8:-1];assert all(32<=b<=126 for b in body)
    frames.append(json.loads(body))
s, bounded, malformed, edges=frames
assert s['ui']['message']=='café ♫ 🥁 "\\\n\t'
assert s['playheads'][0]['position_ppm']==500000 and s['playheads'][0]['direction']==-1
assert s['playheads'][0]['sounding_loop_name']=='AMEN 2' and s['playheads'][0]['active']==1
assert s['seq']==s['ms']==s['frames']==s['ticks']==s['rng']==2**32-1
assert s['lanes'][0]['loop_name']=='AMEN 1' and s['lanes'][1]['loop_name']=='AMEN 2'
assert s['lanes'][0]['queued_loop']==-1 and s['lanes'][0]['queued_loop_name']==''
assert s['lanes'][1]['probability']==dict(repeat=0,skip=25,reverse=50,stutter=75,switch=100)
assert bounded['ui']['message']=='\x01'*48 and bounded['tempo']==-2**31 and bounded['blend']==2**31-1
assert malformed['ui']['message']=='\ufffd'*11
assert edges['type']=='input' and edges['enc']==[-2**31,0,0,0,0,0,2**31-1]
assert edges['keys_down']==edges['buttons_up']==edges['ms']==2**32-1
print('telemetry serializer: PASS (ASCII SysEx, JSON, Unicode/surrogates, invalid UTF-8, uint32/int32 bounds, overflow/canaries, ASan/UBSan)')
