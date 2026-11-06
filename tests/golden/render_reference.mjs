// Renders golden reference outputs from the JS prototype kernel.
//
// The TShaperKernel class is extracted verbatim from the prototype HTML at run
// time, so the golden files can never drift from the reference implementation.
//
// usage: node render_reference.mjs <prototype_v3.html> <outDir>
// writes <outDir>/manifest.txt plus planar float32 LE files (L block, R block).
import fs from 'node:fs';
import path from 'node:path';

const [, , htmlPath, outDir] = process.argv;
if (!htmlPath || !outDir) { console.error('usage: render_reference.mjs <html> <outDir>'); process.exit(2); }
fs.mkdirSync(outDir, { recursive: true });

// ---------------------------------------------------------------- kernel extraction
const html = fs.readFileSync(htmlPath, 'utf8');
const a = html.indexOf('class TShaperKernel');
const b = html.indexOf('/* ================= built-in drum loop');
if (a < 0 || b < 0) throw new Error('could not locate TShaperKernel in ' + htmlPath);
const TShaperKernel = new Function(html.slice(a, b) + '\nreturn TShaperKernel;')();

// ---------------------------------------------------------------- deterministic signals
function rng(seed) {   // mulberry32
  let s = seed >>> 0;
  return () => {
    s = (s + 0x6D2B79F5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// Same construction as the prototype's makeDrumLoop, with a seeded RNG.
function drums(sr, seconds, gain = 1) {
  const R = rng(1234);
  const rnd = () => R() * 2 - 1;
  const bpm = 112, spb = 60 / bpm;
  const len = Math.round(sr * seconds);
  const L = new Float32Array(len), Rt = new Float32Array(len);
  const add = (beat, dur, fn, pan) => {
    const n0 = Math.round(beat * spb * sr);
    if (n0 >= len) return;
    const N = Math.min(len - n0, Math.round(dur * sr));
    const gl = Math.min(1, 1 - pan), gr = Math.min(1, 1 + pan);
    for (let i = 0; i < N; i++) { const s = fn(i / sr); L[n0 + i] += s * gl; Rt[n0 + i] += s * gr; }
  };
  const kick = (g) => { let ph = 0; return (t) => {
    const f = 150 * Math.exp(-t * 35) + 46; ph += 2 * Math.PI * f / sr;
    let s = Math.sin(ph) * Math.exp(-t * 7.5);
    if (t < 0.003) s += rnd() * 0.6 * Math.exp(-t * 1500);
    return s * 0.95 * g; }; };
  const snare = (g) => { let lp = 0; return (t) => {
    const tone = Math.sin(2 * Math.PI * 196 * t) * Math.exp(-t * 24) * 0.35;
    const n = rnd(); lp += 0.25 * (n - lp); const bp = n - lp;
    return (tone + bp * Math.exp(-t * 13) * 0.8) * 0.8 * g; }; };
  const hat = (open, g) => { let pn = 0; return (t) => {
    const n = rnd(); const hp = n - pn; pn = n;
    return hp * Math.exp(-t * (open ? 16 : 65)) * 0.55 * g; }; };
  [0, 2, 3.25].forEach(bb => add(bb, 0.45, kick(1), 0));
  [1, 3].forEach(bb => add(bb, 0.35, snare(1), 0.06));
  add(2.75, 0.25, snare(0.35), -0.10);
  for (let bb = 0; bb < 4; bb += 0.5) add(bb, 0.30, hat(false, bb % 1 === 0 ? 0.85 : 0.5), 0.25);
  add(3.5, 0.6, hat(true, 0.9), 0.25);
  let pk = 0; for (let i = 0; i < len; i++) pk = Math.max(pk, Math.abs(L[i]), Math.abs(Rt[i]));
  const g = 0.85 * gain / pk;
  for (let i = 0; i < len; i++) { L[i] *= g; Rt[i] *= g; }
  return [L, Rt];
}

// Noise bursts at three levels — exercises level-independent detection.
function bursts(sr, seconds) {
  const R = rng(99);
  const len = Math.round(sr * seconds);
  const L = new Float32Array(len), Rt = new Float32Array(len);
  const levels = [0.01, 0.1, 0.5, 0.05, 0.9];
  levels.forEach((lv, j) => {
    const n0 = Math.round((0.02 + j * seconds / levels.length) * sr);
    for (let i = 0; n0 + i < len && i < sr * 0.15; i++) {
      const env = Math.exp(-i / (sr * 0.03));
      L[n0 + i] = (R() * 2 - 1) * lv * env;
      Rt[n0 + i] = (R() * 2 - 1) * lv * env * 0.7;
    }
  });
  return [L, Rt];
}

// Clicks on a low sine bed, different per channel — exercises link / M-S.
function clicks(sr, seconds) {
  const len = Math.round(sr * seconds);
  const L = new Float32Array(len), Rt = new Float32Array(len);
  for (let i = 0; i < len; i++) {
    const t = i / sr;
    L[i] = 0.2 * Math.sin(2 * Math.PI * 110 * t);
    Rt[i] = 0.15 * Math.sin(2 * Math.PI * 165 * t + 0.4);
  }
  for (let c = 0; c * 0.1 < seconds; c++) {
    const n0 = Math.round((0.013 + c * 0.1) * sr);
    const amp = 0.3 + 0.6 * ((c * 37) % 10) / 10;
    for (let i = 0; i < sr * 0.004 && n0 + i < len; i++) {
      const e = Math.exp(-i / (sr * 0.0008));
      if (c % 2 === 0) L[n0 + i] += amp * e; else Rt[n0 + i] += amp * e;
    }
  }
  return [L, Rt];
}

// ---------------------------------------------------------------- cases
const SIGNALS = {
  drums: (sr) => drums(sr, 2.0),
  hot: (sr) => drums(sr, 1.0, 4.0),       // +12 dB into the clipper
  bursts: (sr) => bursts(sr, 1.0),
  clicks: (sr) => clicks(sr, 1.0),
};

// JS kernel param keys. Defaults match the prototype UI defaults.
const BASE = { attack: 6, sustain: 0, speed: 1, schpf: 60, tilt: 0, pivot: 750, lookMs: 3,
  mix: 1, out: 1, clip: 1, link: 1, delta: 0, bypass: 0, msMode: 0, autoGain: 0, audition: 0 };

const SETS = {
  default:   {},
  punch:     { attack: 18, sustain: -6 },
  soften:    { attack: -15, sustain: 9, speed: 0.6 },
  tilt:      { attack: 9, tilt: 10, pivot: 1400 },
  tiltdark:  { tilt: -12, pivot: 250, speed: 1.8 },
  mid:       { msMode: 1, attack: 12, sustain: -4 },
  side:      { msMode: 2, attack: -10, sustain: 6 },
  nolink:    { link: 0, attack: 14 },
  delta:     { delta: 1, attack: 12 },
  autogain:  { autoGain: 1, attack: 20, sustain: -10 },
  noclip:    { clip: 0, attack: 10, out: Math.pow(10, -6 / 20) },
  zerolook:  { lookMs: 0, attack: 12 },
  maxlook:   { lookMs: 10, attack: 12 },
  hpf:       { schpf: 400, attack: 10 },
  halfmix:   { mix: 0.5, attack: 18, out: Math.pow(10, 3.5 / 20) },
  audition:  { audition: 1, schpf: 200 },
  bypass:    { bypass: 1, attack: 24 },
};

const MATRIX = [];
const RATES = [44100, 48000, 88200, 96000, 192000];
for (const sr of RATES) {
  const full = sr === 48000;
  for (const [sig, sets] of Object.entries({
    drums: full ? Object.keys(SETS) : ['default', 'punch', 'mid', 'tilt', 'autogain'],
    hot: full ? ['default', 'noclip', 'punch'] : ['punch'],
    bursts: full ? ['default', 'punch', 'soften', 'hpf'] : ['punch'],
    clicks: full ? ['default', 'nolink', 'mid', 'side', 'maxlook'] : ['nolink'],
  })) for (const set of sets) MATRIX.push({ sr, sig, set });
}

function writePlanar(file, L, R) {
  const buf = Buffer.alloc((L.length + R.length) * 4);
  for (let i = 0; i < L.length; i++) buf.writeFloatLE(L[i], i * 4);
  for (let i = 0; i < R.length; i++) buf.writeFloatLE(R[i], (L.length + i) * 4);
  fs.writeFileSync(file, buf);
}

const lines = [];
const inputCache = new Map();
for (const { sr, sig, set } of MATRIX) {
  const inKey = `${sig}_${sr}`;
  let input = inputCache.get(inKey);
  if (!input) {
    input = SIGNALS[sig](sr);
    writePlanar(path.join(outDir, `in_${inKey}.f32`), input[0], input[1]);
    inputCache.set(inKey, input);
  }
  const [L, R] = input;
  const k = new TShaperKernel(sr);
  const params = { ...BASE, ...SETS[set] };
  k.set(params);
  const n = L.length;
  const oL = new Float32Array(n), oR = new Float32Array(n);
  const BS = 1024;
  for (let i = 0; i < n; i += BS) {
    const m = Math.min(BS, n - i);
    k.process(L.subarray(i, i + m), R.subarray(i, i + m), oL.subarray(i, i + m), oR.subarray(i, i + m), m);
    k.mQ.length = 0;
  }
  const name = `${sig}_${set}_${sr}`;
  writePlanar(path.join(outDir, `ref_${name}.f32`), oL, oR);
  const kv = Object.entries(params).map(([kk, v]) => `${kk}=${v}`).join(' ');
  lines.push(`${name} ${sr} ${n} in_${inKey}.f32 ref_${name}.f32 ${kv}`);
}
fs.writeFileSync(path.join(outDir, 'manifest.txt'), lines.join('\n') + '\n');
console.log(`rendered ${lines.length} golden cases to ${outDir}`);
