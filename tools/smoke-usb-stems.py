#!/usr/bin/env python3
"""Muted Beets six-channel hardware smoke test; never saves settings.
Requires mido/python-rtmidi and the separately compiled CoreAudio validation
helpers. Restores blend, mutes and split, leaves transport stopped, then returns
MASTER to the physical pot only after telemetry confirms that stop.
"""
import argparse
import array
import copy
import json
import subprocess
import sys
import time
import wave
from pathlib import Path


def main():
    import mido
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='X0X FM-1')
    parser.add_argument('--device', default='BEETS FM-1')
    parser.add_argument('--validation', type=Path, default=Path(__file__).resolve().parents[1] / 'tools/usb-audio-validation',
                        help='directory with capture.py, coreaudio-inspect, usb-descriptors.py')
    parser.add_argument('--output', type=Path, default=Path('build/tests/usb-stems'))
    args = parser.parse_args()
    for name in ('capture.py', 'coreaudio-inspect', 'usb-descriptors.py'):
        if not (args.validation / name).is_file(): parser.error(f'missing validation helper: {name}')
    args.output.mkdir(parents=True, exist_ok=True)
    state, initial, results = None, None, []
    state_count = 0
    with mido.open_input(args.port) as inp, mido.open_output(args.port) as out:
        def collect(seconds):
            nonlocal state, state_count
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                for message in inp.iter_pending():
                    if message.type == 'sysex' and bytes(message.data).startswith(b'\x7dBEETS\x01'):
                        obj = json.loads(bytes(message.data)[7:])
                        if obj['type'] == 'state':
                            state = obj
                            state_count += 1
                time.sleep(.002)

        def expect(predicate, description):
            before = state_count
            until = time.monotonic() + 3
            while time.monotonic() < until:
                collect(.05)
                if state_count > before and state and predicate(state): return
            raise RuntimeError(f'telemetry did not confirm {description}')

        def remote(op, *values):
            out.send(mido.Message('sysex', data=[0x7d, 0x4c, 0x43, op, *values]))
            collect(.18)

        def buttons(mask): remote(1, mask & 127, mask >> 7)
        def tap(bit): buttons(1 << bit); buttons(0)

        def stop():
            if state and state['playing']: tap(10)
            expect(lambda s: not s['playing'], 'stopped transport')

        def play():
            assert state['ui']['master'] == 0, 'MASTER must be muted before playback'
            if not state['playing']: tap(10)
            expect(lambda s: s['playing'] and s['ui']['master'] == 0, 'muted playback')

        def mute(lane, value):
            if bool(state['lanes'][lane]['muted']) != value: tap(5 if lane else 1)
            expect(lambda s: bool(s['lanes'][lane]['muted']) == value, 'lane mute')

        def blend(value):
            for _ in range(10):
                old = state['blend']
                delta = max(-32, min(32, value - old))
                if not delta: return
                remote(3, 2, 64 + delta)
                expect(lambda s: s['blend'] == old + delta, 'blend change')
            raise RuntimeError('blend change failed')

        def split(value):
            stop(); buttons(1 << 4)
            try: remote(3, 2, 65 if value else 63)
            finally: buttons(0)
            expect(lambda s: bool(s['output_split']) == bool(value), 'output split')

        def capture(label, rate, active):
            assert state['ui']['master'] == 0
            prefix = args.output / f'{label}-{rate}'
            command = [sys.executable, str(args.validation / 'capture.py'), '--device', args.device,
                       '--usb-product', args.device, '--rate', str(rate), '--seconds', '3',
                       '--channels', '6', '--expected-names',
                       'Master L,Master R,Deck A L,Deck A R,Deck B L,Deck B R', '--output', str(prefix)]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode: raise RuntimeError(result.stderr or result.stdout)
            evidence = json.loads(prefix.with_suffix('.json').read_text())
            assert evidence['status'] == 'PASS', evidence
            with wave.open(str(prefix.with_suffix('.wav')), 'rb') as wav:
                assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (6, 2, rate)
                pcm = array.array('h', wav.readframes(wav.getnframes()))
            if sys.byteorder != 'little': pcm.byteswap()
            # Exclude stream priming and existing DSP tails from routing assertions.
            pcm = pcm[rate * 6:]
            peaks = [max(map(abs, pcm[c::6])) for c in range(6)]
            pair_error = [max(abs(x-y) for x,y in zip(pcm[c::6], pcm[c+1::6])) for c in (0,2,4)]
            row = {'mode': label, 'rate': rate, 'peak_pcm16': peaks, 'pair_error_pcm16': pair_error,
                   'physical_format_evidence': str(prefix.with_suffix('.json'))}
            results.append(row)
            assert max(peaks[:2]) <= 2, row
            assert max(pair_error) <= 2, row
            for lane, on in enumerate(active):
                peak = max(peaks[2+lane*2:4+lane*2])
                assert (peak > 100 if on else peak <= 2), row
            collect(.3)
            assert state['ui']['master'] == 0, 'MASTER changed during capture'
            print(json.dumps(row), flush=True)

        try:
            # First mutation is mute; no command ever sets a nonzero remote gain.
            remote(4, 0, 0)
            expect(lambda s: s['ui']['master'] == 0, 'MASTER zero')
            initial = copy.deepcopy(state)
            stop()
            assert state['clock_source'] == 0, 'disconnect external clock before test'
            for rate in (44100,):
                stop()
                for mode in (False, True):
                    split(mode); blend(50)
                    for label, active, mix in (
                        ('a-only', (True, False), 50), ('b-only', (False, True), 50),
                        ('muted', (False, False), 50), ('blend-a', (True, True), 0),
                        ('blend-b', (True, True), 100)):
                        mute(0, not active[0]); mute(1, not active[1]); blend(mix)
                        play(); collect(.6)
                        capture(('split-' if mode else 'mono-') + label, rate, active)
        finally:
            # On a lost connection/failed stop, leave remote MASTER zero.
            remote(4, 0, 0)
            buttons(0)
            stop()
            if initial:
                split(initial['output_split']); blend(initial['blend'])
                mute(0, bool(initial['lanes'][0]['muted'])); mute(1, bool(initial['lanes'][1]['muted']))
            stop()
            expect(lambda s: not s['playing'] and s['ui']['master'] == 0,
                   'fresh stopped and muted state before releasing MASTER')
            remote(4, 1, 32)  # 4097 = release override to physical pot, only while stopped
    report = {'status': 'PASS', 'checks': results, 'transport_left_stopped': True,
              'saved_settings_written': False, 'limitations': ['No physical analog recording']}
    (args.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS: six-channel stems, muted master, mutes, blend and split independence')


if __name__ == '__main__':
    main()
