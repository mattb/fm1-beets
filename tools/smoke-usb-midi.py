#!/usr/bin/env python3
"""Exercise a connected Beets FM1 and validate the actual USB MIDI telemetry.
Temporarily changes transport/tempo and holds remote controls; never saves settings.
Requires mido/python-rtmidi. Records received MIDI bytes and decoded JSONL.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import time
import mido

spec = importlib.util.spec_from_file_location('monitor', Path(__file__).with_name('monitor-midi.py'))
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='X0X FM-1')
parser.add_argument('--output', default='build/tests/usb-telemetry')
args = parser.parse_args()
base = Path(args.output)
base.parent.mkdir(parents=True, exist_ok=True)
records, sizes, other = [], [], []
initial = None
with mido.open_input(args.port) as inp, mido.open_output(args.port) as out, \
        base.with_suffix('.syx').open('wb') as raw, base.with_suffix('.jsonl').open('w') as decoded:
    def collect(seconds):
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            for msg in inp.iter_pending():
                if msg.type != 'sysex':
                    continue
                raw.write(bytes(msg.bytes()))
                obj = monitor.parse_payload(msg.data)
                if obj is None:
                    other.append(list(msg.data))
                    continue
                if records:
                    assert obj['seq'] == (records[-1]['seq'] + 1) & 0xffffffff, 'sequence gap/reboot'
                records.append(obj)
                sizes.append(len(msg.data) + 2)
                decoded.write(json.dumps(obj, ensure_ascii=True) + '\n')
            time.sleep(.002)
    def remote(op, *data):
        out.send(mido.Message('sysex', data=[0x7d, 0x4c, 0x43, op, *data]))
        collect(.15)
    def states():
        return [x for x in records if x['type'] == 'state']
    try:
        collect(2.2)
        assert states(), 'no telemetry received'
        initial = states()[-1]
        out.send(mido.Message('stop'))
        collect(1.2)
        assert states()[-1]['playing'] == 0, 'MIDI Stop not reflected'
        # Real MIDI Start, then tempo detent through the existing panel remote protocol.
        out.send(mido.Message('start'))
        collect(5)
        running = [s for s in states() if s['playing']]
        assert len(running) >= 15, 'insufficient tempo-aligned snapshots'
        assert running[-1]['ticks'] > running[0]['ticks'], 'clock stalled'
        assert len({s['playheads'][0]['position_ppm'] for s in running}) > 4, 'playhead stalled'
        delta = -1 if initial['tempo'] >= 220 else 1
        remote(3, 0, 64 + delta)
        assert states()[-1]['tempo'] == initial['tempo'] + delta, 'encoder state missing'
        remote(3, 0, 64 - delta)
        # Held reverse and a white key: exercise buttons, keys, release edges and coexistence ACKs.
        remote(1, 1 << 2, 0)
        assert states()[-1]['lanes'][initial['ui']['focus']]['force_reverse'] == 1
        remote(1, 0, 0)
        remote(2, 1, 0, 0, 0)
        remote(2, 0, 0, 0, 0)
        # Ordinary note traffic in both directions of the two-deck mapping.
        for channel in (0, 1):
            for note in range(36, 44):
                out.send(mido.Message('note_on', channel=channel, note=note, velocity=80))
                collect(.06)
                out.send(mido.Message('note_off', channel=channel, note=note))
        collect(5)
        out.send(mido.Message('stop'))
        collect(2.2)
        assert states()[-1]['playing'] == 0
        inputs = [x for x in records if x['type'] == 'input']
        for field in ('keys_down', 'keys_up', 'buttons_down', 'buttons_up'):
            assert any(x[field] for x in inputs), f'missing {field}'
        assert sum(x['enc'][0] for x in inputs) == 0, 'encoder delta lost'
        assert all(s['ui']['input_dropped'] == initial['ui']['input_dropped'] for s in states())
        assert any(x[:3] == [0x7d, 0x4c, 0x43] for x in other), 'remote ACK coexistence missing'
    finally:
        remote(1, 0, 0)
        remote(2, 0, 0, 0, 0)
        if initial and states():
            diff = initial['tempo'] - states()[-1]['tempo']
            if diff:
                remote(3, 0, 64 + diff)
        out.send(mido.Message('start' if initial and initial['playing'] else 'stop'))
        collect(.3)
summary = {'result': 'PASS', 'port': args.port, 'state_frames': len(states()),
           'input_frames': len(records)-len(states()), 'max_frame_bytes': max(sizes),
           'sequence_gaps': 0, 'other_sysex_frames': len(other),
           'checks': ['ASCII JSON and framing', 'contiguous sequence', 'MIDI Start/Stop',
                      'advancing clock/playheads', 'encoder change and restore',
                      'button/key press and release', 'remote ACK coexistence', 'no new input drops']}
base.with_suffix('.summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
