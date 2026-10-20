// SPDX-License-Identifier: MIT
// Execute the actual browser artifact in a fresh WASM instance, with panel inputs.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
const root=path.resolve(import.meta.dirname,'..');
const wasm=fs.readFileSync(path.join(root,'app/build/emu/app.wasm'));
const compiled=await WebAssembly.compile(wasm);
async function boot(saved={}) {
  let ex;
  const wasi=new Proxy({
    clock_time_get: (_id,_precision,out)=>{new DataView(ex.memory.buffer).setBigUint64(out,0n,true);return 0;},
    fd_write: (_fd,_iov,_n,out)=>{new DataView(ex.memory.buffer).setUint32(out,0,true);return 0;},
    proc_exit: code=>{throw Error(`WASM exited ${code}`);}
  },{get:(o,k)=>o[k]??(()=>0)});
  ex=(await WebAssembly.instantiate(compiled,{wasi_snapshot_preview1:wasi,env:new Proxy({},{get:()=>()=>0})})).exports;
  ex._initialize?.();
  for(const [obj,bytes] of Object.entries(saved)) { new Uint8Array(ex.memory.buffer,ex.web_store(+obj),bytes.length).set(bytes);ex.web_store_set_len(+obj,bytes.length); }
  ex.web_master(3200); ex.web_boot(); return ex;
}
function render(ex,frames,chunk=128) {
  const out=new Float32Array(frames*2);
  for(let pos=0;pos<frames;) {
    const n=Math.min(chunk,frames-pos); ex.web_render(n);
    const l=new Float32Array(ex.memory.buffer,ex.web_out_l(),n),r=new Float32Array(ex.memory.buffer,ex.web_out_r(),n);
    for(let i=0;i<n;i++){assert(Number.isFinite(l[i])&&Number.isFinite(r[i]));out[(pos+i)*2]=l[i];out[(pos+i)*2+1]=r[i];}
    pos+=n;
  }
  return out;
}
function press(ex,bit){ex.web_buttons(1<<bit);render(ex,2205);ex.web_buttons(0);render(ex,2205);}
function perform(ex,chunk) {
  render(ex,44100); // splash + UI initialization
  press(ex,10); // PLAY
  const first=render(ex,44100*2,chunk);
  press(ex,5); // deck B mute/unmute, a stable physical mapping
  ex.web_enc(3,25);ex.web_enc(4,30);ex.web_enc(5,50);ex.web_enc(6,55);
  render(ex,4410);
  const mutation=render(ex,44100*2,chunk);
  ex.web_keys((1<<0)|(1<<7));render(ex,4410);ex.web_keys(0); // A range 1..5
  const range=render(ex,44100,chunk);
  const blits=ex.web_blits();assert(blits>0);
  const fb=new Uint16Array(ex.memory.buffer,ex.web_fb(),240*240);assert(fb.some(v=>v!==0));
  press(ex,10);render(ex,4410);
  const silence=render(ex,4410,chunk);assert(silence.every(v=>v===0),'stop must silence both decks');
  const all=new Float32Array(first.length+mutation.length+range.length);all.set(first);all.set(mutation,first.length);all.set(range,first.length+mutation.length);
  const peak=all.reduce((p,v)=>Math.max(p,Math.abs(v)),0);assert(peak>0.01&&peak<=1,'actual samples must be audible and bounded');
  assert(first.some(v=>v!==0));assert(mutation.some(v=>v!==0));
  return {pcm:all,peak,blits,ms:ex.web_now()};
}
const black=[1,3,5,8,10,13,15,17,20,22];
const samplePeaks=[];
for(let sample=0;sample<10;sample++) {
  const ex=await boot();render(ex,44100);
  ex.web_keys(1<<black[sample]);render(ex,2205);ex.web_keys(0);render(ex,2205);
  press(ex,10);const pcm=render(ex,44100);
  const peak=pcm.reduce((p,v)=>Math.max(p,Math.abs(v)),0);
  assert(peak>0.01&&peak<=1,`sample ${sample+1} must sound`);samplePeaks.push(peak);
}
const saveEx=await boot();render(saveEx,44100);
saveEx.web_enc(0,17);saveEx.web_keys(1<<black[7]);render(saveEx,2205);saveEx.web_keys(0);render(saveEx,2205);
press(saveEx,7); // SAVE
const saved={};for(let obj=0;obj<saveEx.web_nobj();obj++){const n=saveEx.web_store_len(obj);if(n>0)saved[obj]=new Uint8Array(saveEx.memory.buffer,saveEx.web_store(obj),n).slice();}
assert(Object.keys(saved).length>0,'SAVE must produce a persistent settings object');
const reference=await boot();reference.web_enc(0,17);reference.web_keys(1<<black[7]);render(reference,2205);reference.web_keys(0);render(reference,41895);press(reference,10);const beforeRestore=render(reference,44100);
const restored=await boot(saved);render(restored,44100);press(restored,10);const afterRestore=render(restored,44100);
assert(Buffer.from(beforeRestore.buffer).equals(Buffer.from(afterRestore.buffer)),'saved tempo and sample must restore to identical audio at the same control/audio timeline');
const a=perform(await boot(),128),b=perform(await boot(),256);
assert(Buffer.from(a.pcm.buffer).equals(Buffer.from(b.pcm.buffer)),'fresh render must be reproducible across audio quantum sizes');
const outdir=path.join(root,'app/build/tests');fs.mkdirSync(outdir,{recursive:true});
const wav=Buffer.alloc(44+a.pcm.length*2);
wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(2,22);wav.writeUInt32LE(44100,24);wav.writeUInt32LE(44100*4,28);wav.writeUInt16LE(4,32);wav.writeUInt16LE(16,34);wav.write('data',36);wav.writeUInt32LE(a.pcm.length*2,40);
for(let i=0;i<a.pcm.length;i++)wav.writeInt16LE(Math.max(-32768,Math.min(32767,Math.round(a.pcm[i]*32767))),44+i*2);
fs.writeFileSync(path.join(outdir,'wasm-performance.wav'),wav);
const result={result:'PASS',wasmBytes:wasm.length,peak:a.peak,screenBlits:a.blits,simulatedMs:a.ms,renderedSeconds:a.pcm.length/2/44100,blockInvariant:true,allTenSamples:samplePeaks,persistence:true};
fs.writeFileSync(path.join(outdir,'wasm-smoke.json'),JSON.stringify(result,null,2)+'\n');console.log(result);
