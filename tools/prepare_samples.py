#!/usr/bin/env python3
"""Pack the original Beets demo loops into shared, flash-resident mu-law audio.
Requires ffmpeg; generated C is identical for device, host and WASM.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/source/demo-loops'
RATE = 22050


def encode(sample):
    sign = 0x80 if sample < 0 else 0
    sample = min(abs(sample), 32635) + 132
    exponent = max(0, min(7, sample.bit_length() - 8))
    return (~(sign | (exponent << 4) | ((sample >> (exponent + 3)) & 15))) & 255


def decode(value):
    value = (~value) & 255
    sample = (((value & 15) << 3) + 132) << ((value >> 4) & 7)
    return 132 - sample if value & 128 else sample - 132


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args()
    files = sorted((SOURCE / 'demo-beets').glob('*.wav')) + sorted((SOURCE / 'demo-amen').glob('*.wav'))
    if len(files) != 10:
        raise SystemExit('Expected the ten original Beets demo WAV files')
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        raise SystemExit('Install ffmpeg (brew install ffmpeg) to prepare the original samples')
    version = subprocess.check_output([ffmpeg, '-version'], text=True).splitlines()[0]
    sig = hashlib.sha256(Path(__file__).read_bytes() + version.encode() + b''.join(p.read_bytes() for p in files)).hexdigest()
    signature = f'/* Beets source fingerprint: {sig} */'
    if args.out.exists() and args.out.read_text().splitlines()[0] == signature:
        print('samples: original Beets pack unchanged')
        return
    args.out.parent.mkdir(parents=True, exist_ok=True)
    lines = [signature, '#pragma once', '#include <stdint.h>',
             'typedef struct { const uint8_t *data; uint32_t frames; const char *name; } BeetsSample;',
             'static const int16_t beets_mulaw[256] = {', ','.join(str(decode(i)) for i in range(256)), '};']
    meta = []
    for i, path in enumerate(files):
        raw = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(path), '-ac', '1', '-ar', str(RATE),
                                       '-af', 'aresample=filter_size=64:phase_shift=10:cutoff=0.95', '-f', 's16le', '-'])
        packed = bytes(encode(s[0]) for s in struct.iter_unpack('<h', raw))
        lines.append(f'static const uint8_t beets_audio_{i}[{len(packed)}] = {{')
        lines.extend(','.join(str(v) for v in packed[n:n+32]) + ',' for n in range(0, len(packed), 32))
        lines.append('};')
        with wave.open(str(path)) as w:
            seconds = w.getnframes() / w.getframerate()
        name = f'BEETS {i+1}' if i < 8 else f'AMEN {i-7}'
        meta.append({'name': name, 'source': str(path.relative_to(ROOT)), 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                     'source_duration_seconds': seconds, 'original_bpm': 240 / seconds,
                     'frames': len(packed), 'sample_rate': RATE, 'encoding': 'G.711 mu-law',
                     'packed_sha256': hashlib.sha256(packed).hexdigest()})
    lines.append('static const BeetsSample beets_samples[10] = {')
    lines.extend(f'{{beets_audio_{i}, {m["frames"]}u, "{m["name"]}"}},' for i, m in enumerate(meta))
    lines.append('};')
    args.out.write_text('\n'.join(lines) + '\n')
    args.out.with_name('sample_manifest.json').write_text(json.dumps({'source_commit': '226c2b892658bf7418555677a6a5aeab6fb3cfc6',
          'ffmpeg': version, 'fingerprint': sig, 'packed_bytes': sum(m['frames'] for m in meta), 'samples': meta}, indent=2) + '\n')
    print(f'samples: {len(meta)} original loops, {sum(m["frames"] for m in meta)} bytes at {RATE} Hz mu-law')


if __name__ == '__main__':
    main()
