#!/usr/bin/env python3
"""Validate native app integration; this does not emulate physical USB timing."""
import json
from pathlib import Path
import sys
raw = Path(sys.argv[1]).read_bytes()
frames = []
lengths = []
while raw:
    assert raw[:8] == b'\xf0\x7dBEETS\x01', 'unexpected or split SysEx prefix'
    end = raw.index(b'\xf7')
    frame, raw = raw[:end + 1], raw[end + 1:]
    assert len(frame) <= 3072 and all(b < 128 for b in frame[1:-1])
    lengths.append(len(frame))
    frames.append(json.loads(frame[8:-1]))
assert frames and frames[0]['type'] == 'state'
for previous, current in zip(frames, frames[1:]):
    assert current['seq'] == previous['seq'] + 1 or (current['seq'] == 0 and current['type'] == 'state'), 'missing/duplicate accepted frame'
# The native scenario explicitly reboots once; a boot restarts the sequence.
states = [f for f in frames if f['type'] == 'state']
inputs = [f for f in frames if f['type'] == 'input']
assert any(f['playing'] for f in states) and any(not f['playing'] for f in states)
assert any(f['keys_down'] for f in inputs) and any(f['keys_up'] for f in inputs)
assert any(f['buttons_down'] for f in inputs) and any(f['buttons_up'] for f in inputs)
assert any(any(f['enc']) for f in inputs)
assert all(f['ui']['input_dropped'] == 0 for f in states)
assert any(p['active'] and p['position_ppm'] > 0 for f in states for p in f['playheads'])
print(f'Native telemetry integration: {len(states)} state/{len(inputs)} input frames, max {max(lengths)} bytes; complete ASCII SysEx, strict JSON, contiguous sequence, controls and moving playheads.')
