// VMX score records and the phone call that sends them (docs/DESIGN.md "High scores" and "Calling in").
// Plain functions, no DOM: the mockup plays the audio, tools/modem_test.mjs checks it through a fake phone line.

// ---------------------------------------------------------------- records
export const EPOCH = Date.UTC(2001, 10, 15);                 // 15 Nov 2001, Xbox launch day: record dates count from it
export const day = (ms = Date.now()) => Math.max(0, Math.floor((ms - EPOCH) / 86400000));
export const dayDate = d => new Date(EPOCH + d * 86400000);

export function crc16(b, from = 0, to = b.length) {          // CRC-16/CCITT-FALSE
  let c = 0xFFFF;
  for (let i = from; i < to; i++) {
    c ^= b[i] << 8;
    for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
  }
  return c;
}

export const REC = 20;
// r: { tid, board, flags, date, score, f: [f1, f2, f3] } -> 20 bytes
export function packRecord(r, out = new Uint8Array(REC), o = 0) {
  const v = new DataView(out.buffer, out.byteOffset + o, REC);
  v.setUint32(0, r.tid >>> 0, true); v.setUint8(4, r.board || 0); v.setUint8(5, r.flags || 0);
  v.setUint16(6, r.date ?? day(), true); v.setInt32(8, r.score | 0, true);
  for (let i = 0; i < 3; i++) v.setUint16(12 + i * 2, (r.f?.[i] || 0) & 0xFFFF, true);
  v.setUint16(18, crc16(out, o, o + 18), true);
  return out;
}
export function unpackRecord(b, o = 0) {
  const v = new DataView(b.buffer, b.byteOffset + o, REC);
  return { tid: v.getUint32(0, true), board: v.getUint8(4), flags: v.getUint8(5), date: v.getUint16(6, true),
    score: v.getInt32(8, true), f: [0, 1, 2].map(i => v.getUint16(12 + i * 2, true)),
    ok: v.getUint16(18, true) === crc16(b, o, o + 18) };
}

// the message on the line: "VX", version, count, serial, name[8], records, CRC-16
export function packMessage({ serial, name, records }) {
  const n = records.length, b = new Uint8Array(16 + n * REC + 2), v = new DataView(b.buffer);
  b[0] = 0x56; b[1] = 0x58; b[2] = 1; b[3] = n; v.setUint32(4, serial >>> 0, true);
  const nm = String(name).toUpperCase().slice(0, 8);
  for (let i = 0; i < 8; i++) b[8 + i] = i < nm.length ? nm.charCodeAt(i) & 0x7F : 0;
  records.forEach((r, i) => packRecord(r, b, 16 + i * REC));
  v.setUint16(16 + n * REC, crc16(b, 0, 16 + n * REC), true);
  return b;
}
export function unpackMessage(b) {
  if (b.length < 18 || b[0] !== 0x56 || b[1] !== 0x58) return null;
  const n = b[3], len = 16 + n * REC + 2;
  if (b.length < len) return null;
  const v = new DataView(b.buffer, b.byteOffset);
  if (v.getUint16(len - 2, true) !== crc16(b, 0, len - 2)) return null;
  let name = ""; for (let i = 0; i < 8 && b[8 + i]; i++) name += String.fromCharCode(b[8 + i]);
  return { version: b[2], serial: v.getUint32(4, true), name,
    records: Array.from({ length: n }, (_, i) => unpackRecord(b, 16 + i * REC)) };
}
// the 4-digit code the voice reads back, and the VMX shows
export const checkCode = msg => String(crc16(msg) % 10000).padStart(4, "0");

// ---------------------------------------------------------------- sound
export const BAUD = 1200, MARK = 1200, SPACE = 2200;          // Bell 202
const DTMF_ROW = [697, 770, 852, 941], DTMF_COL = [1209, 1336, 1477];
const DTMF_KEY = "123456789*0#";
// letters on the keypad (1-800-VMX-WRLD)
const LETTER = { ABC: 2, DEF: 3, GHI: 4, JKL: 5, MNO: 6, PQRS: 7, TUV: 8, WXYZ: 9 };
export const keypad = s => [...String(s).toUpperCase()].map(c => {
  if (/[0-9*#]/.test(c)) return c;
  for (const k in LETTER) if (k.includes(c)) return String(LETTER[k]);
  return "";
}).join("");

// The VMX's beeper is a magnetic buzzer driven by a square wave: square = true makes that sound.
function tones(out, at, sr, freqs, dur, amp, square) {
  const n = Math.round(dur * sr);
  for (let i = 0; i < n && at + i < out.length; i++) {
    let s = 0;
    for (const f of freqs) { const x = Math.sin(2 * Math.PI * f * i / sr); s += square ? Math.sign(x) : x; }
    out[at + i] += amp * s / freqs.length;
  }
  return at + n;
}

// bits for one byte: start (0), 8 data bits lowest first, stop (1)
const frame = b => [0, ...Array.from({ length: 8 }, (_, k) => (b >> k) & 1), 1];

// Plan of the whole call: [{t, kind, ...}] in seconds, and the total length.
export function callPlan(msg, number = "18008699753", copies = 3) {
  const plan = []; let t = 0.2;
  for (const d of keypad(number)) { plan.push({ t, kind: "dtmf", d, dur: 0.09 }); t += 0.16; }
  t += 1.6;                                                   // the voice answers ("Send now"), the player presses A
  for (let c = 0; c < copies; c++) {
    plan.push({ t, kind: "data", copy: c, dur: 0.3 + (2 + msg.length) * 10 / BAUD + 0.05 });
    t += plan[plan.length - 1].dur + 0.25;
  }
  return { plan, len: t + 0.2 };
}

// The call as samples: DTMF digits, a pause, then the message `copies` times (sync tone, 7E 7E, bytes).
export function callAudio(msg, { sr = 48000, number, copies = 3, square = true, amp = 0.5 } = {}) {
  const { plan, len } = callPlan(msg, number, copies);
  const out = new Float32Array(Math.ceil(len * sr));
  for (const p of plan) {
    const at = Math.round(p.t * sr);
    if (p.kind === "dtmf") {
      const k = DTMF_KEY.indexOf(p.d);
      tones(out, at, sr, [DTMF_ROW[k / 3 | 0], DTMF_COL[k % 3]], p.dur, amp, square);
    } else {
      // phase-continuous FSK: 0.3 s of marks, then the framed bytes
      const bits = [...Array(Math.round(0.3 * BAUD)).fill(1)];
      for (const b of [0x7E, 0x7E, ...msg]) bits.push(...frame(b));
      bits.push(1, 1, 1, 1);
      let ph = 0, i = at;
      for (let k = 0; k < bits.length; k++) {
        const end = at + Math.round((k + 1) * sr / BAUD), f = bits[k] ? MARK : SPACE;
        for (; i < end && i < out.length; i++) {
          ph += 2 * Math.PI * f / sr;
          const x = Math.sin(ph); out[i] += amp * (square ? Math.sign(x) : x);
        }
      }
    }
  }
  return out;
}

// ---------------------------------------------------------------- a fake phone line (for tests and the mockup)
function biquad(x, sr, type, f0, q = 0.7071) {
  const w = 2 * Math.PI * f0 / sr, al = Math.sin(w) / (2 * q), c = Math.cos(w);
  let b0, b1, b2;
  if (type === "lp") { b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; } else { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; }
  const a0 = 1 + al, a1 = -2 * c, a2 = 1 - al, y = new Float32Array(x.length);
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i++) {
    const v = (b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
    x2 = x1; x1 = x[i]; y2 = y1; y1 = v; y[i] = v;
  }
  return y;
}
const mulaw = v => {                                          // 8-bit mu-law round trip
  const s = Math.sign(v), m = Math.log1p(255 * Math.min(1, Math.abs(v))) / Math.log1p(255);
  const q = Math.round(m * 127) / 127;
  return s * (Math.pow(256, q) - 1) / 255;
};
// band-pass 300-3400 Hz, down to 8 kHz, mu-law, hiss; seed makes the noise repeatable
export function phoneLine(x, sr, { noise = 0.02, gain = 0.6, seed = 1 } = {}) {
  let y = biquad(biquad(biquad(x, sr, "hp", 300), sr, "lp", 3400), sr, "lp", 3400);
  const step = sr / 8000, out = new Float32Array(Math.floor(y.length / step));
  let r = seed >>> 0 || 1;
  const rnd = () => { r ^= r << 13; r >>>= 0; r ^= r >> 17; r ^= r << 5; r >>>= 0; return r / 4294967296 - 0.5; };
  for (let i = 0; i < out.length; i++) out[i] = mulaw(gain * y[Math.floor(i * step)] + noise * rnd() * 2);
  return out;
}

// ---------------------------------------------------------------- the decoder (the service's side)
// Non-coherent FSK: energy at MARK vs SPACE over one bit, then a UART that looks for 7E 7E and a valid message.
export function decode(x, sr = 8000) {
  const N = Math.round(sr / BAUD), d = new Float32Array(x.length);
  const sum = f => { const c = new Float32Array(x.length), s = new Float32Array(x.length);
    for (let i = 0; i < x.length; i++) { c[i] = x[i] * Math.cos(2 * Math.PI * f * i / sr); s[i] = x[i] * Math.sin(2 * Math.PI * f * i / sr); }
    return [c, s]; };
  const [mc, ms] = sum(MARK), [sc, ss] = sum(SPACE);
  let a = 0, b = 0, p = 0, q = 0, lev = 0;
  for (let i = 0; i < x.length; i++) {
    a += mc[i]; b += ms[i]; p += sc[i]; q += ss[i];
    if (i >= N) { a -= mc[i - N]; b -= ms[i - N]; p -= sc[i - N]; q -= ss[i - N]; }
    const m = a * a + b * b, s = p * p + q * q;
    d[i] = m - s; lev = Math.max(lev * 0.999, m + s);
    if (m + s < lev * 0.05) d[i] = 1e-9;                       // quiet: treat as idle (mark)
  }
  const spb = sr / BAUD, bit = t => d[Math.min(d.length - 1, Math.round(t))] > 0 ? 1 : 0;
  // read UART bytes from sample i on; returns [bytes, next i]
  const found = [];
  let i = 0;
  while (i < d.length - 20 * spb) {
    // a start bit: the window ending here is more space than mark, after a mark
    if (!(d[i] < 0 && d[i - 1] >= 0)) { i++; continue; }
    const bytes = [];
    let t0 = i - N / 2, ok = true;
    while (ok && bytes.length < 16 + 255 * REC + 4) {
      // check the start bit's centre, then the data and the stop bit
      if (bit(t0 + spb - 1) !== 0) { ok = false; break; }
      let v = 0; for (let k = 0; k < 8; k++) v |= bit(t0 + (k + 2) * spb - 1) << k;
      if (bit(t0 + 10 * spb - 1) !== 1) { ok = false; break; }
      bytes.push(v);
      // the next start bit: the first space within a few bits of the stop
      let j = Math.round(t0 + 10 * spb), lim = j + 4 * spb;
      while (j < lim && j < d.length && !(d[j] < 0)) j++;
      if (j >= lim || j >= d.length) break;
      t0 = j - N / 2;
    }
    const k = bytes.indexOf(0x56);
    if (bytes[0] === 0x7E && k > 0) {
      const m = unpackMessage(Uint8Array.from(bytes.slice(k)));
      if (m) { found.push({ at: i / sr, msg: m, raw: Uint8Array.from(bytes.slice(k, k + 18 + m.records.length * REC)) }); }
    }
    i = Math.max(i + 1, Math.round(t0 + 10 * spb));
  }
  return found;
}
