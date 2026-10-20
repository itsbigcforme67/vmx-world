// Checks the score call end to end: records -> message -> call audio (square-wave beeper) -> fake phone line ->
// decoder -> the same records. Run: node tools/modem_test.mjs
import * as M from "../mockup/modem.js";

let fail = 0;
const check = (ok, what) => { console.log((ok ? "ok   " : "FAIL ") + what); if (!ok) fail++; };

const records = Array.from({ length: 30 }, (_, i) => ({ tid: 0x56581001 + (i % 9), board: i % 3, flags: 0,
  date: M.day(Date.UTC(2002, 10, 20)) + i, score: (i * 7919) % 100000 - (i === 5 ? 42 : 0), f: [i, i * 3, 65535 - i] }));
const msg = M.packMessage({ serial: 0x00C0FFEE, name: "JAMES", records });
check(msg.length === 16 + 30 * 20 + 2, `message is ${msg.length} bytes`);
const back = M.unpackMessage(msg);
check(back && back.records.every((r, i) => r.ok && r.score === records[i].score && r.f[2] === records[i].f[2]),
  "pack/unpack round trip");
const bad = msg.slice(); bad[40] ^= 1;
check(M.unpackMessage(bad) === null, "a flipped bit is caught by the CRC");
check(M.keypad("1-800-VMX-WRLD") === "18008699753", "keypad letters");

for (const [label, opts] of [["clean line", { noise: 0 }], ["normal line", { noise: 0.02 }],
  ["noisy line", { noise: 0.08 }], ["quiet handset", { gain: 0.15, noise: 0.02 }]]) {
  for (const square of [true, false]) {
    const audio = M.callAudio(msg, { sr: 48000, square });
    const line = M.phoneLine(audio, 48000, opts);
    const got = M.decode(line, 8000);
    const good = got.filter(g => g.msg.serial === 0x00C0FFEE && g.msg.records.length === 30);
    check(good.length >= 1, `${label}, ${square ? "beeper" : "sine"}: ${good.length}/3 copies decoded`);
  }
}
const { len } = M.callPlan(msg);
console.log(`call length ${len.toFixed(1)} s, check code ${M.checkCode(msg)}`);
process.exit(fail ? 1 : 0);
