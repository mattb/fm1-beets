#!/usr/bin/env python3
"""Capture actual FM1 USB audio and check split/mono routing at 44.1 and 48 kHz.
Temporarily operates panel controls; restores routing, blend, mutes and transport.
Leaves the physical MASTER knob in control; set a comfortable audible level.
Never saves settings. Requires mido, python-rtmidi and sounddevice.
"""
import argparse, array, json, math, time, wave
from pathlib import Path
import mido
import sounddevice as sd
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', default='X0X FM-1')
p.add_argument('--device', default='BEETS FM-1')
p.add_argument('--output', default='build/tests/usb-audio')
a = p.parse_args()
base = Path(a.output); base.mkdir(parents=True, exist_ok=True)
state = initial = None
results = []
with mido.open_input(a.port) as inp, mido.open_output(a.port) as out:
    def collect(seconds):
        global state
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            for msg in inp.iter_pending():
                if msg.type == 'sysex' and bytes(msg.data).startswith(b'\x7dBEETS\x01'):
                    obj = json.loads(bytes(msg.data)[7:])
                    if obj['type'] == 'state': state = obj
            time.sleep(.002)
    def remote(op, *values):
        out.send(mido.Message('sysex', data=[0x7d,0x4c,0x43,op,*values])); collect(.18)
    def buttons(mask): remote(1,mask & 127,mask >> 7)
    def tap(bit): buttons(1 << bit); buttons(0)
    def playing(on):
        if bool(state['playing']) != bool(on): tap(10)
    def mute(lane,on):
        if bool(state['lanes'][lane]['muted']) != bool(on): tap(5 if lane else 1)
    def blend(value):
        for _ in range(10):
            delta = max(-32,min(32,value-state['blend']))
            if not delta: return
            remote(3,2,64+delta)
        raise RuntimeError('blend restore failed')
    def split(on):
        playing(False); buttons(1 << 4); remote(3,2,65 if on else 63); buttons(0)
        assert state['output_split'] == int(on)
    def capture(label,rate):
        # Open before playback so the host establishes its USB audio stream first.
        with sd.RawInputStream(device=a.device,samplerate=rate,channels=2,dtype='int16',blocksize=0) as stream:
            stream.read(rate//2)
            raw,overflow = stream.read(rate*2)
        assert not overflow, 'host input overflow'
        data = bytes(raw); pcm = array.array('h',data)
        peaks = [max(abs(v) for v in pcm[c::2]) for c in (0,1)]
        rms = [math.sqrt(sum(v*v for v in pcm[c::2])/ (len(pcm)//2)) for c in (0,1)]
        with wave.open(str(base / f'{label}-{rate}.wav'),'wb') as wav:
            wav.setparams((2,2,rate,0,'NONE','not compressed')); wav.writeframes(data)
        row = {'mode':label,'rate':rate,'peak_pcm16':peaks,'rms_pcm16':rms,'host_overflow':overflow}
        results.append(row)
        if label == 'split-a': assert peaks[0] > 100 and peaks[1] <= 1, row
        elif label == 'split-b': assert peaks[1] > 100 and peaks[0] <= 1, row
        # Allow two PCM16 counts in the host capture; WASM checks exact mono equality.
        else: assert peaks[0] > 100 and all(abs(pcm[i]-pcm[i+1]) <= 2 for i in range(0,len(pcm),2)), row
        print(json.dumps(row),flush=True)
        collect(.2)
    try:
        collect(1.5)
        assert state and 'output_split' in state, 'requires Beets 0.3.1'
        assert state['clock_source'] == 0, 'disconnect external clocks'
        initial = state.copy()
        playing(False); blend(50)
        split(True)
        for rate in (44100,48000):
            mute(0,False); mute(1,True); playing(True); collect(.5); capture('split-a',rate)
            mute(0,True); mute(1,False); collect(.5); capture('split-b',rate)
        split(False); mute(0,False); mute(1,False); playing(True); collect(.5)
        capture('mono',44100)
    finally:
        if initial:
            playing(False); split(initial['output_split']); blend(initial['blend'])
            mute(0,initial['lanes'][0]['muted']); mute(1,initial['lanes'][1]['muted'])
            playing(initial['playing'])
report = {'result':'PASS','device':a.device,'checks':results,'limits':['No physical analog capture','No physical power-cycle test; persistence tested in native and WASM']}
(base / 'summary.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS USB stereo channel isolation and dual mono')
