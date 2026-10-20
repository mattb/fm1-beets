#!/usr/bin/env python3
"""Verify source provenance, packed assets, duration and actual quantization quality."""
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'app/build/gen'
manifest = json.loads((GEN / 'sample_manifest.json').read_text())
header = (GEN / 'beets_samples.h').read_text()
assert len(manifest['samples']) == 10
results = []
for i, sample in enumerate(manifest['samples']):
    source = ROOT / sample['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == sample['source_sha256']
    with wave.open(str(source)) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == 48000
        duration = w.getnframes() / w.getframerate()
    match = re.search(rf'beets_audio_{i}\[\d+\]\s*=\s*\{{([^}}]+)\}}', header)
    assert match, f'missing packed sample {i}'
    packed = bytes(int(v) for v in re.findall(r'\d+', match.group(1)))
    assert len(packed) == sample['frames']
    assert hashlib.sha256(packed).hexdigest() == sample['packed_sha256']
    assert abs(len(packed)/sample['sample_rate'] - duration) <= 1/sample['sample_rate']
    pcm = subprocess.check_output(['ffmpeg','-v','error','-i',str(source),'-ac','1','-ar','22050',
        '-af','aresample=filter_size=64:phase_shift=10:cutoff=0.95','-f','s16le','-'])
    ref = [v[0] for v in struct.iter_unpack('<h', pcm)]
    assert len(ref) == len(packed)
    # Independent G.711 expansion, then compare to resampled linear PCM.
    decoded=[]
    for code in packed:
        u=code ^ 0xff
        magnitude=((u & 0x0f)*8+132)*(1<<((u>>4)&7))-132
        decoded.append(-magnitude if u&0x80 else magnitude)
    power=sum(v*v for v in ref)
    noise=sum((a-b)**2 for a,b in zip(ref,decoded))
    snr=10*math.log10(power/max(1,noise))
    assert power>0 and snr>30, (sample['name'],snr)
    results.append({'name':sample['name'],'frames':len(packed),'quantization_snr_db':round(snr,2)})
assert sum(v['frames'] for v in results)==manifest['packed_bytes']
out=ROOT/'app/build/tests';out.mkdir(parents=True,exist_ok=True)
(out/'sample-pack.json').write_text(json.dumps(results,indent=2)+'\n')
print(f"PASS 10 original loops: hashes, packed integrity, duration, mu-law SNR >= {min(v['quantization_snr_db'] for v in results):.2f} dB")
