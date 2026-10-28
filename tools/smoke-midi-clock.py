#!/usr/bin/env python3
"""Check USB MIDI clock on a connected Beets. Operates transport and tempo,
then restores them; never saves settings. Requires an idle instrument."""
import argparse
import json
from pathlib import Path
import statistics
import time
import mido

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', default='X0X FM-1')
p.add_argument('--output', default='build/tests/midi-clock.json')
a = p.parse_args()
state = None
initial = None
received = []
with mido.open_input(a.port) as inp, mido.open_output(a.port) as out:
    def send(kind): out.send(mido.Message(kind))
    def collect(seconds, bpm=None):
        global state
        end = time.monotonic() + seconds
        next_clock = time.monotonic()
        while time.monotonic() < end:
            now = time.monotonic()
            if bpm and now >= next_clock:
                send('clock')
                next_clock += 60 / bpm / 24
            for msg in inp.iter_pending():
                received.append((time.monotonic(), msg.type))
                if msg.type == 'sysex' and bytes(msg.data).startswith(b'\x7dBEETS\x01'):
                    obj = json.loads(bytes(msg.data)[7:])
                    if obj['type'] == 'state': state = obj
            time.sleep(.0005)
    def panel_play():
        out.send(mido.Message('sysex', data=[0x7d, 0x4c, 0x43, 1, 0, 8]))
        collect(.06)
        out.send(mido.Message('sysex', data=[0x7d, 0x4c, 0x43, 1, 0, 0]))
        collect(.1)
    def tempo(value):
        for _ in range(20):
            if state['tempo'] == value: return
            d = max(-32, min(32, value - state['tempo']))
            out.send(mido.Message('sysex', data=[0x7d, 0x4c, 0x43, 3, 0, 64 + d]))
            collect(.25)
        raise RuntimeError('tempo restore did not converge')
    try:
        collect(1.3)
        assert state and state.get('clock_source') == 0, 'requires firmware with clock support and no active external clock'
        initial = state.copy()
        send('stop'); collect(.7)
        received.clear(); panel_play(); collect(5); panel_play()
        clocks = [t for t, kind in received if kind == 'clock']
        assert any(kind == 'start' for _, kind in received) and any(kind == 'stop' for _, kind in received)
        assert len(clocks) > 100 and not state['playing']
        rate = (len(clocks)-1)/(clocks[-1]-clocks[0])
        assert abs(rate - initial['tempo'] * 24 / 60) < 2, (rate, initial['tempo'])
        gaps = sorted((b-a)*1000 for a,b in zip(clocks,clocks[1:]))
        report = {'result':'PASS','port':a.port,'internal_bpm':initial['tempo'],
                  'output_clocks':len(clocks),'output_hz':rate,
                  'host_gap_ms':{'median':statistics.median(gaps),'p95':gaps[int(.95*(len(gaps)-1))],'max':max(gaps)},
                  'external':[]}
        for bpm in (120,180):
            received.clear(); send('start'); collect(2.5,bpm)
            assert state['playing'] and state['clock_source'] == 1
            assert abs(state['tempo']-bpm) <= 2, state['tempo']
            assert not any(k in ('clock','start','continue','stop') for _,k in received), 'USB input echoed'
            ticks = state['ticks']
            send('stop'); collect(.35,bpm)
            assert not state['playing'] and state['ticks'] == ticks
            send('continue'); collect(.5,bpm)
            assert state['playing'] and state['ticks'] > ticks
            report['external'].append({'sent_bpm':bpm,'measured_bpm':state['tempo'],'no_usb_echo':True,'stop_continue':True})
        collect(.85)
        assert not state['playing'] and state['clock_source'] == 0, 'clock loss did not stop/release'
        report['clock_loss_stops'] = True
    finally:
        if initial:
            send('stop'); collect(.7)
            tempo(initial['tempo'])
            if initial['playing']: panel_play()
Path(a.output).parent.mkdir(parents=True,exist_ok=True)
Path(a.output).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
