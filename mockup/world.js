// VMX World mockup: the Xbox disc's TV screens (docs/DESIGN.md "The World disc"). 640 x 480, driven by the pad or
// the keyboard. The games are the VMX simulator's adapters and cores (vmx/, copied by tools/sync.sh); downloads,
// the VMX in the controller and the national tables are made up. The score call is real: real records, real modem
// sound (modem.js), decoded again through a fake phone line before the check code is shown.
import { GAMES as SIM } from "./vmx/games.js";
import * as M from "./modem.js";

"use strict";
// ---------------------------------------------------------------- 1-bit surfaces + the VMX's 3x5 font (from vmx.js)
const FONT = {
  A:"010101111101101",B:"110101110101110",C:"011100100100011",D:"110101101101110",E:"111100110100111",F:"111100110100100",
  G:"011100101101011",H:"101101111101101",I:"111010010010111",J:"001001001101010",K:"101101110101101",L:"100100100100111",
  M:"101111111101101",N:"110101101101101",O:"010101101101010",P:"110101110100100",Q:"010101101110011",R:"110101110101101",
  S:"011100010001110",T:"111010010010010",U:"101101101101111",V:"101101101101010",W:"101101111111101",X:"101101010101101",
  Y:"101101010010010",Z:"111001010100111","0":"111101101101111","1":"010110010010111","2":"110001010100111",
  "3":"110001010001110","4":"101101111001001","5":"111100110001110","6":"011100111101111","7":"111001010010010",
  "8":"111101111101111","9":"111101111001110"," ":"000000000000000",".":"000000000000010","-":"000000111000000",
  "?":"110001010000010","!":"010010010000010",":":"000010000010000","/":"001001010100100","%":"101001010100101",
  "(":"010100100100010",")":"010001001001010","+":"000010111010000",">":"100010001010100","<":"001010100010001",
  "*":"000101010101000"
};
class Surf {
  constructor(w, h) { this.w = w; this.h = h; this.d = new Uint8Array(w * h); }
  clear(c = 0) { this.d.fill(c); }
  px(x, y, c = 1) { x = Math.floor(x); y = Math.floor(y); if (x < 0 || y < 0 || x >= this.w || y >= this.h) return; this.d[y * this.w + x] = c; }
  fill(x, y, w, h, c = 1) { for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) this.px(x + i, y + j, c); }
  rect(x, y, w, h, c = 1) { for (let i = 0; i < w; i++) { this.px(x + i, y, c); this.px(x + i, y + h - 1, c); }
    for (let j = 0; j < h; j++) { this.px(x, y + j, c); this.px(x + w - 1, y + j, c); } }
  rrect(x, y, w, h, c = 1) { for (let i = 1; i < w - 1; i++) { this.px(x + i, y, c); this.px(x + i, y + h - 1, c); }
    for (let j = 1; j < h - 1; j++) { this.px(x, y + j, c); this.px(x + w - 1, y + j, c); } }
  line(x0, y0, x1, y1, c = 1) { const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1);
    for (let i = 0; i <= n; i++) this.px(Math.round(x0 + (x1 - x0) * i / n), Math.round(y0 + (y1 - y0) * i / n), c); }
  disc(cx, cy, r, c = 1) { for (let y = -r - 1; y <= r + 1; y++) for (let x = -r - 1; x <= r + 1; x++)
    if (x * x + y * y <= r * r + r * 0.6) this.px(cx + x, cy + y, c); }
  ring(cx, cy, r, c = 1) { for (let y = -r - 1; y <= r + 1; y++) for (let x = -r - 1; x <= r + 1; x++) {
    const d = x * x + y * y; if (d <= r * r + r * 0.6 && d > (r - 1) * (r - 1) + (r - 1) * 0.6) this.px(cx + x, cy + y, c); } }
  text(x, y, s, c = 1) { s = String(s).toUpperCase(); for (const ch of s) { const g = FONT[ch] || FONT["?"];
    for (let r = 0; r < 5; r++) for (let q = 0; q < 3; q++) if (g[r * 3 + q] === "1") this.px(x + q, y + r, c); x += 4; } }
  centre(y, s, c = 1) { this.text(32 - Math.floor((String(s).length * 4 - 1) / 2), y, s, c); }
}
const ON = "#7dff3a", OFF = "#061006";
// a Surf as a canvas data URL, each VMX pixel k x k
function surfURL(s, k, on = ON, off = "transparent") {
  const c = document.createElement("canvas"); c.width = s.w * k; c.height = s.h * k;
  const x = c.getContext("2d"); if (off !== "transparent") { x.fillStyle = off; x.fillRect(0, 0, c.width, c.height); }
  x.fillStyle = on;
  for (let j = 0; j < s.h; j++) for (let i = 0; i < s.w; i++) if (s.d[j * s.w + i]) x.fillRect(i * k, j * k, k, k);
  return c.toDataURL();
}

// ---------------------------------------------------------------- the catalogue
const best = (rec, v) => { if (v > (rec.best || 0)) { rec.best = v; return true; } return false; };
const FOLD = {
  id: "fold", name: ["FOLD", "ROOMS"], title: "FOLD ROOMS", usesB: true,
  load: () => import("./vmx/cores/foldrooms_core.js"),
  start: (m, seed) => m.newGame(seed, 0),
  tick(g, { held: h, demo }) { this.m.step(g, demo ? this.m.demo(g) : h); },   // it takes held buttons
  record: (g, rec) => best(rec, g.level + 1),
  draw(g) { return this.m.draw(g); },
  art(s) { s.rect(3, 3, 12, 12); s.line(3, 3, 9, 0); s.line(15, 3, 21, 0); s.line(15, 15, 21, 12); s.line(9, 0, 21, 0);
    s.line(21, 0, 21, 12); s.fill(7, 7, 4, 4); s.rect(12, 17, 5, 5); s.px(7, 19); s.px(8, 18); s.px(8, 20); s.line(0, 23, 23, 23); },
};
const ADAPT = Object.fromEntries([...SIM, FOLD].map(a => [a.id, a]));
// what the World service sells; tid = the game's ID (DESIGN.md "Title IDs"), f = labels of fields 1-3
const CAT = [
  { id: "blox", tid: 0x56581001, pub: "VMX Studio", blocks: 2, shelf: "new", blurb: "Tiles tumble down the belt. Catch them on your paddle and drop them into the bin in rows of three." },
  { id: "blobs", tid: 0x56581002, pub: "VMX Studio", blocks: 2, shelf: "new", blurb: "Pairs of blobs fall. Join four of a colour to pop them, and set up chains for big scores." },
  { id: "fold", tid: 0x56581010, pub: "VMX Studio", blocks: 4, shelf: "new", applet: "FOLD BUILDER", f: ["LEVEL"],
    blurb: "Push the boxes onto the targets. Every room is one face of a shape you can't see, until you clear it. Comes with FOLD BUILDER for the Xbox: make rooms in 3D and send them to your VMX." },
  { id: "intercept", tid: 0x56581003, pub: "VMX Studio", blocks: 1, shelf: "arcade", blurb: "Missiles rain on your cities. Steer the sight and burst them in the sky." },
  { id: "invasion", tid: 0x56581004, pub: "VMX Studio", blocks: 1, shelf: "arcade", blurb: "Rows of invaders march down. Hold the line from behind your bunkers." },
  { id: "blackjack", tid: 0x56581005, pub: "VMX Studio", blocks: 1, shelf: "cards", title: "BLACKJACK AND HOOKERS", blurb: "Blackjack on the pier. Bet your hooks, beat the dealer, don't go bust." },
  { id: "checkers", tid: 0x56581006, pub: "VMX Studio", blocks: 1, shelf: "cards", f: ["WON", "LOST", "DRAWN"], blurb: "American checkers against a VMX that thinks ahead." },
  { id: "reversi", tid: 0x56581007, pub: "VMX Studio", blocks: 1, shelf: "cards", f: ["WON", "LOST", "DRAWN"], blurb: "Flip the board your colour. Easy to learn, hard to master." },
  { id: "tricky", tid: 0x56588001, pub: "Homebrew", blocks: 2, shelf: "homebrew", blurb: "Light every wick before the candles run out. A homebrew puzzle game." },
  { id: "zork", tid: 0x56588002, pub: "Homebrew (MIT licence)", blocks: 8, shelf: "homebrew", scoreless: true, blurb: "The great underground empire, on your VMX. A word picker takes the place of a keyboard." },
];
for (const c of CAT) { const a = ADAPT[c.id]; c.a = a; c.title ||= a.title; const s = new Surf(24, 24); a.art(s);
  c.card = s; c.icon = surfURL(s, 3); c.iconBig = surfURL(s, 6); }
const byId = id => CAT.find(c => c.id === id);
const hex = n => n.toString(16).toUpperCase().padStart(8, "0");

// ---------------------------------------------------------------- saved state (this browser only)
const load = (k, d) => { try { const v = JSON.parse(localStorage.getItem(k)); return v && typeof v === "object" ? v : d; } catch { return d; } };
const store = (k, v) => { try { localStorage.setItem(k, JSON.stringify(v)); } catch {} };
const ST = Object.assign({ hdd: ["blox", "blobs"], vmx: ["blox"], vmxIn: true, recs: { blox: { best: 1240, day: 370 } }, sent: {}, owner: "PLAYER1", serial: 0 },
  load("vmx-world", {}));
if (!ST.serial) ST.serial = (Math.random() * 0xFFFFFF) >>> 0;
const save = () => store("vmx-world", ST);
const VMX_BLOCKS = 256;
const vmxUsed = () => ST.vmx.reduce((n, id) => n + byId(id).blocks, 0) + 3;          // 3 = the VMX's own settings
// one score record per game, from the simulator's best (fields per the game's labels)
function recordOf(c) {
  const r = ST.recs[c.id]; if (!r || c.scoreless) return null;
  const won = c.f?.[0] === "WON";
  const score = won ? (r.w || 0) : (r.best || 0);
  if (!score && !(r.l || r.d)) return null;
  return { tid: c.tid, board: 0, flags: 4, date: r.day ?? M.day(), score, f: won ? [r.w || 0, r.l || 0, r.d || 0] : [0, 0, 0] };
}

// ---------------------------------------------------------------- sound (the World's own UI blips)
let ac = null;
const audio = () => { if (!ac) try { ac = new (window.AudioContext || window.webkitAudioContext)(); } catch { ac = null; }
  if (ac?.state === "suspended") ac.resume(); return ac; };
function tone(f, t0, dur, vol = 0.05, type = "sine") {
  const a = audio(); if (!a) return;
  const o = a.createOscillator(), g = a.createGain(), t = a.currentTime + t0;
  o.type = type; o.frequency.value = f; g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.0005, t + dur);
  o.connect(g).connect(a.destination); o.start(t); o.stop(t + dur + 0.02);
}
const sMove = () => tone(880, 0, 0.05, 0.03);
const sOk = () => { tone(660, 0, 0.08); tone(990, 0.06, 0.12); };
const sBack = () => { tone(740, 0, 0.07); tone(494, 0.06, 0.1); };
const sErr = () => tone(160, 0, 0.18, 0.06, "square");
let callSrc = null;
function playSamples(x, sr) {
  const a = audio(); if (!a) return null;
  const b = a.createBuffer(1, x.length, sr); b.copyToChannel(x, 0);
  const s = a.createBufferSource(), g = a.createGain(); g.gain.value = 0.25;
  s.buffer = b; s.connect(g).connect(a.destination); s.start(); return s;
}

// ---------------------------------------------------------------- the TV
const TABS = ["NEWS", "GET GAMES", "MY VMX", "PLAY", "SCORES", "APPLETS"];
const S = { tab: 0, zone: "tabs", sel: {}, col: 0, menu: null, dl: null, play: null, call: null, applet: null, toast: null };
const sel = (k, n) => { const v = S.sel[k] || 0; return Math.max(0, Math.min(n - 1, v)); };
const el = s => document.querySelector(s);
const esc = s => String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
function toast(t) { S.toast = { t, at: performance.now() }; draw(); }

const NEWS = [
  { d: "21 NOV 2002", h: "FOLD ROOMS is out", b: "Push boxes round rooms that wrap a shape you can't see. FOLD ROOMS brings the first World applet, FOLD BUILDER: build levels in 3D on your TV, then send them to your VMX. 4 blocks, free this month." },
  { d: "15 NOV 2002", h: "Score lines are open", b: "Pick CALL on your VMX's main menu, hold it to your phone and it sends the scores from every game you played. The tables update every night. The call is free from US phones." },
  { d: "15 NOV 2002", h: "Welcome to VMX World", b: "Get new VMX games, put them on your VMX, and play them on your TV. Games you get here live on your hard disk as VMX World saves: copy them to a friend's memory unit in the Xbox dashboard and they can install them too." },
  { d: "01 NOV 2002", h: "Homebrew shelf", b: "Make your own VMX games with the free VMX kit and send them to us. The best ones go on the Homebrew shelf, next to ours." },
  { d: "28 OCT 2002", h: "Retail games with VMX features", b: "Look for the VMX badge on the box. Those games carry VMX games on the disc and a World Kit to install them. World adds an applet for each one, so you can manage its VMX extras without the disc." },
];

const APPLETS = [
  { id: "foldb", name: "FOLD BUILDER", from: "fold", what: "Build FOLD ROOMS levels in 3D: fold the working plane round the shape, place walls, boxes and targets, wire up buttons and gates. Test on the VMX window, then send the level to your VMX.", img: "img/fold-builder.png" },
  { id: "kit", name: "RETAIL APPLET (EXAMPLE)", from: null, what: "Each retail game with VMX features gets an applet here: its VMX games, the unlocks traded with the disc game, and its score tables, without the disc in the tray. None yet in this mockup." },
];

// ----- render
function draw() {
  el("#tabs").innerHTML = TABS.map((t, i) => `<div class="tab ${i === S.tab ? "on" : ""} ${i === S.tab && S.zone === "tabs" ? "focus" : ""}" data-tab="${i}"><span>${t}</span></div>`).join("");
  el("#vmxstat").innerHTML = ST.vmxIn ? `<b class="dot"></b>VMX IN CONTROLLER 1 &middot; ${VMX_BLOCKS - vmxUsed()} BLOCKS FREE`
    : `<b class="dot off"></b>NO VMX IN CONTROLLER 1`;
  const body = el("#body");
  body.className = "body t" + S.tab + (S.zone === "content" ? " active" : "");
  const f = [news, shop, mine, play, scores, applets][S.tab];
  const html = f(); if (html !== null) body.innerHTML = html;
  if (S.tab === 3) placeDevice();
  for (const r of body.querySelectorAll(".list .row.sel")) {         // keep the chosen row in view
    const l = r.parentNode; if (r.offsetTop < l.scrollTop) l.scrollTop = r.offsetTop;
    else if (r.offsetTop + r.offsetHeight > l.scrollTop + l.clientHeight) l.scrollTop = r.offsetTop + r.offsetHeight - l.clientHeight; }
  el("#hints").innerHTML = hints();
  const tst = el("#toast");
  if (S.toast && performance.now() - S.toast.at < 2600) { tst.textContent = S.toast.t; tst.className = "toast show"; }
  else tst.className = "toast";
  el("#overlay").innerHTML = overlay();
  const cv = el("#callvmx"); if (cv) cv.appendChild(device);          // the VMX's own CALL screen during a call
}
const hint = (k, t) => `<span class="h"><b class="k k${k}">${k}</b>${t}</span>`;
function hints() {
  if (S.call) return S.call.stage === "done" ? hint("A", "DONE") : hint("B", "HANG UP");
  if (S.applet) return hint("B", "BACK TO WORLD");
  if (S.play) return hint("A", "") + hint("B", "") + `<span class="h">D-PAD / STICK</span><span class="h"><b class="k kBK">BACK</b>VMX SIDE BUTTON: QUIT</span>`;
  if (S.menu) return hint("A", "SELECT") + hint("B", "CANCEL");
  if (S.zone === "tabs") return hint("A", "OPEN") + `<span class="h"><b class="k kLR">&#9664; &#9654;</b>TABS</span>`;
  return hint("A", "SELECT") + hint("B", "BACK") + `<span class="h"><b class="k kLR">LT RT</b>TABS</span>`;
}
function list(key, items, row) {
  const k = sel(key, items.length);
  return `<div class="list">${items.map((it, i) => `<div class="row ${i === k ? "sel" : ""}" data-key="${key}" data-i="${i}">${row(it, i)}</div>`).join("")}</div>`;
}
function news() {
  const k = sel("news", NEWS.length), n = NEWS[k];
  return `<div class="panel left">${list("news", NEWS, it => `<small>${it.d}</small><span>${esc(it.h)}</span>`)}</div>
    <div class="panel right article"><small>${n.d}</small><h2>${esc(n.h)}</h2><p>${esc(n.b)}</p></div>`;
}
const status = c => S.dl?.id === c.id ? `${Math.floor(S.dl.p * 100)}%` : ST.hdd.includes(c.id) ? "&#10003;" : "";
function shop() {
  const k = sel("shop", CAT.length), c = CAT[k];
  const on = ST.hdd.includes(c.id), dl = S.dl?.id === c.id;
  return `<div class="panel left">${list("shop", CAT, it => `<img src="${it.icon}" alt=""><span>${esc(it.title)}</span><small class="st">${status(it)}</small>`)}</div>
    <div class="panel right detail">
      <div class="hero"><img src="${c.iconBig}" alt=""><div><h2>${esc(c.title)}</h2><small>${esc(c.pub)} &middot; ${c.blocks} BLOCK${c.blocks > 1 ? "S" : ""} &middot; ID ${hex(c.tid)}</small>
      ${c.applet ? `<div class="badge">+ XBOX APPLET: ${c.applet}</div>` : ""}</div></div>
      <p>${esc(c.blurb)}</p>
      ${dl ? `<div class="bar"><i style="width:${S.dl.p * 100}%"></i></div><small>DOWNLOADING ${Math.round(c.blocks * 16 * S.dl.p)} OF ${c.blocks * 16} KB</small>`
        : `<div class="act ${S.zone === "content" ? "focus" : ""}">${on ? "&#10003; ON YOUR HARD DISK" : "A &nbsp;DOWNLOAD (FREE)"}</div>`}
    </div>`;
}
function mine() {
  const hdd = ST.hdd.map(byId), vmx = ST.vmx.map(byId);
  const used = vmxUsed();
  const col = (title, items, key, c, extra) => `<div class="panel col ${S.col === c && S.zone === "content" ? "cur" : ""}"><h3>${title}</h3>${extra || ""}
    ${items.length ? list(key, items, it => `<img src="${it.icon}" alt=""><span>${esc(it.title)}</span><small class="st">${it.blocks} BL${(c === 0 && ST.vmx.includes(it.id)) ? " &middot; ON VMX" : ""}</small>`)
      : `<p class="empty">${c ? (ST.vmxIn ? "NO GAMES ON THE VMX" : "PUT A VMX IN CONTROLLER 1") : "NOTHING YET: SEE GET GAMES"}</p>`}</div>`;
  return col("XBOX HARD DISK", hdd, "hdd", 0) + col("VMX &middot; CONTROLLER 1", ST.vmxIn ? vmx : [], "vmx", 1,
    ST.vmxIn ? `<div class="bar small"><i style="width:${used / VMX_BLOCKS * 100}%"></i></div><small class="blk">${used} OF ${VMX_BLOCKS} BLOCKS USED</small>` : "");
}
// PLAY: the list on the left, the VMX device on the right (kept between draws so the game's canvas lives on)
const device = (() => { const d = document.createElement("div"); d.className = "device";
  d.innerHTML = `<canvas id="lcd" width="64" height="64"></canvas>`; return d; })();
const lcd = device.querySelector("canvas"), lx = lcd.getContext("2d"), img = lx.createImageData(64, 64);
function play() {
  const lib = ST.hdd.map(byId);
  return `<div class="panel left">${lib.length ? list("play", lib, it => `<img src="${it.icon}" alt=""><span>${esc(it.title)}</span>`) : `<p class="empty">NO GAMES ON THE HARD DISK</p>`}</div>
    <div class="right stage" id="stage"></div>`;
}
function placeDevice() { const st = el("#stage"); if (st && device.parentNode !== st) st.appendChild(device); if (!S.play) idleScreen(); }
function scores() {
  const games = CAT.filter(c => ST.hdd.includes(c.id) && !c.scoreless);
  if (!games.length) return `<div class="panel left"><p class="empty">NO GAMES YET</p></div>`;
  const k = sel("scores", games.length), c = games[k], r = recordOf(c), sent = ST.sent[c.id];
  const unsent = CAT.map(x => recordOf(x) && !ST.sent[x.id] ? x : null).filter(Boolean);
  const table = national(c, r && sent ? r.score : null);
  return `<div class="panel left">${list("scores", games, it => `<img src="${it.icon}" alt=""><span>${esc(it.title)}</span><small class="st">${recordOf(it) ? (ST.sent[it.id] ? "SENT" : "NEW") : ""}</small>`)}</div>
    <div class="panel right sc">
      <h2>${esc(c.title)}</h2>
      <div class="rec">${r ? `<div><small>YOUR BEST</small><b>${r.score}</b></div>${(c.f || []).map((l, i) => `<div><small>${l}</small><b>${r.f[i]}</b></div>`).join("")}
        <div><small>STATUS</small><b class="${sent ? "" : "new"}">${sent ? "SENT " + sent : "NOT SENT"}</b></div>` : `<p class="empty">NO SCORE YET: PLAY IT ON THE VMX OR IN PLAY</p>`}</div>
      <h3>NATIONAL TOP 5 <small>(MADE UP)</small></h3>
      <ol class="nat">${table.map(e => `<li class="${e.you ? "you" : ""}"><span>${e.name}</span><b>${e.score}</b></li>`).join("")}</ol>
      <div class="act ${S.zone === "content" ? "focus" : ""}">${unsent.length ? `A &nbsp;CALL IN ${unsent.length} NEW SCORE${unsent.length === 1 ? "" : "S"} FROM THE VMX` : "NO NEW SCORES TO CALL IN"}</div>
      <small class="note">On the VMX this is CALL on its main menu; here World shows and plays the call for it.</small>
    </div>`;
}
function national(c, yours) {
  let r = c.tid; const rnd = () => { r ^= r << 13; r >>>= 0; r ^= r >> 17; r ^= r << 5; r >>>= 0; return r / 4294967296; };
  const names = ["ZEKE", "MOMO", "JAX", "PIXL", "DANA", "R2K", "LULU", "OSKR", "VEX", "TAMI"];
  const top = c.f?.[0] === "WON" ? 60 : c.id === "fold" ? 28 : 40000;
  const t = Array.from({ length: 5 }, (_, i) => ({ name: names.splice((rnd() * names.length) | 0, 1)[0], score: Math.round(top * (1 - i * 0.12) * (0.9 + rnd() * 0.1)) }));
  if (yours != null) t.push({ name: ST.owner, score: yours, you: true });
  return t.sort((a, b) => b.score - a.score).slice(0, 5);
}
function applets() {
  const k = sel("applets", APPLETS.length), a = APPLETS[k], have = !a.from || ST.hdd.includes(a.from);
  return `<div class="panel left">${list("applets", APPLETS, it => `<span>${esc(it.name)}</span><small class="st">${it.from && !ST.hdd.includes(it.from) ? "NEEDS " + byId(it.from).title : ""}</small>`)}</div>
    <div class="panel right detail"><h2>${esc(a.name)}</h2>${a.img ? `<div class="shot" style="background-image:url(${a.img})"></div>` : ""}<p>${esc(a.what)}</p>
    ${a.from ? `<div class="act ${S.zone === "content" ? "focus" : ""}">${have ? "A &nbsp;START APPLET" : "GET " + byId(a.from).title + " FIRST"}</div>` : ""}</div>`;
}
function overlay() {
  if (S.menu) return `<div class="menu"><h3>${esc(S.menu.title)}</h3>${S.menu.items.map((m, i) => `<div class="row ${i === S.menu.k ? "sel" : ""}" data-menu="${i}">${m[0]}</div>`).join("")}</div>`;
  if (S.applet) return `<div class="applet"><div class="shot big" style="background-image:url(${S.applet.img})"></div>
    <p>On the Xbox, World starts <b>${S.applet.name}</b> here (TDATA\\56580001\\APPS\\${hex(byId(S.applet.from).tid)}\\default.xbe), and returns to World when you quit. This is its level view, from the FOLD repo.</p></div>`;
  if (S.call) return callView();
  return "";
}
function callView() {
  const C = S.call, steps = [["HOLD THE VMX TO THE PHONE", "dial"], ["DIALLING 1-800-VMX-WRLD", "dial"], ["SENDING " + C.n + " SCORES (3 TIMES)", "send"], ["THE VOICE READS BACK A CODE", "check"]];
  const at = { dial: 1, send: 2, check: 3, done: 4 }[C.stage];
  return `<div class="call"><div id="callvmx"></div><h3>CALLING IN</h3><canvas id="scope" width="300" height="60"></canvas>
    <ol>${steps.map((s, i) => `<li class="${i < at ? "done" : i === at ? "now" : ""}">${s[0]}</li>`).join("")}</ol>
    ${C.stage === "done" ? `<p class="code">VOICE: <b>"${C.heard ? C.heard.split("").join(" ") : "- - - -"}"</b> &nbsp; VMX: <b>${C.code}</b></p>
      <p>${C.heard === C.code ? `The service decoded ${C.copies} of 3 copies through a fake phone line. The codes match, so the VMX marks these scores sent.` : "No copy got through. Try again."}</p>` : ""}</div>`;
}

// ---------------------------------------------------------------- the VMX screen (simulator)
function blit(buf) {
  const d = img.data;
  for (let i = 0; i < 4096; i++) { const v = buf[i]; d[i * 4] = v ? 125 : 6; d[i * 4 + 1] = v ? 255 : 16; d[i * 4 + 2] = v ? 58 : 6; d[i * 4 + 3] = 255; }
  lx.putImageData(img, 0, 0);
}
const scr = new Surf(64, 64);
function idleScreen() {
  scr.clear();
  const lib = ST.hdd.map(byId), c = lib[sel("play", lib.length)];
  if (S.call) return callScreen();
  if (!c) { scr.centre(28, "NO GAMES"); return blit(scr.d); }
  for (let j = 0; j < 24; j++) for (let i = 0; i < 24; i++) if (c.card.d[j * 24 + i]) scr.px(20 + i, 8 + j);
  scr.rrect(18, 6, 28, 28);
  c.a.name.forEach((l, i) => scr.centre(38 + i * 7, l));
  scr.centre(56, "A: PLAY");
  blit(scr.d);
}
function callScreen() {                                   // the VMX's own CALL screen during the call
  const C = S.call; scr.clear();
  scr.centre(3, "CALL");
  scr.line(0, 10, 63, 10);
  if (C.stage === "dial") { scr.centre(20, "DIALLING"); scr.centre(30, C.digits || ""); }
  else if (C.stage === "send") { scr.centre(18, "SENDING"); scr.centre(26, C.n + " SCORES");
    const p = Math.min(1, C.p || 0); scr.rect(8, 40, 48, 6); scr.fill(9, 41, Math.round(46 * p), 4); }
  else if (C.stage === "check" || C.stage === "done") { scr.centre(18, "CODE"); scr.centre(28, C.code); scr.centre(44, "A: IT MATCHES"); scr.centre(52, "B: TRY AGAIN"); }
  blit(scr.d);
}

// game loop pieces (as vmx.js tickGame)
const BTN = ["up", "down", "left", "right", "a", "b"];
const held = {}, latch = {};
async function startGame(c) {
  const a = c.a;
  try { a.m ||= await a.load.call(a); } catch { sErr(); toast("COULDN'T LOAD " + c.title); return; }
  const g = a.start(a.m, Date.now() % 100000, false);
  S.play = { c, a, g, prev: {} };
  for (const b of BTN) held[b] = latch[b] = false;
  sOk(); draw();
}
function stopGame() { S.play = null; sBack(); draw(); }
function tickGame() {
  const P = S.play, h = {}, e = {};
  for (const b of BTN) { h[b] = held[b] || latch[b]; e[b] = h[b] && !P.prev[b]; latch[b] = false; }
  P.prev = h;
  if (e.b && !P.a.usesB) return stopGame();
  P.a.tick(P.g, { held: h, edges: e, tap: null, demo: false });
  const rec = (ST.recs[P.c.id] ||= {});
  if (P.a.record(P.g, rec)) { rec.day = M.day(); delete ST.sent[P.c.id]; save(); }
}

// ---------------------------------------------------------------- actions
function download(c) {
  if (ST.hdd.includes(c.id) || S.dl) return;
  S.dl = { id: c.id, p: 0, t0: performance.now(), dur: 1200 + c.blocks * 500 }; sOk();
}
function openMenu(title, items) { S.menu = { title, items, k: 0 }; sOk(); }
function mineAction() {
  if (S.col === 0) {
    const c = byId(ST.hdd[sel("hdd", ST.hdd.length)]); if (!c) return;
    const onV = ST.vmx.includes(c.id);
    openMenu(c.title, [
      [onV ? "ALREADY ON THE VMX" : "INSTALL TO VMX", () => {
        if (onV) return;
        if (!ST.vmxIn) { sErr(); return toast("PUT A VMX IN CONTROLLER 1"); }
        if (vmxUsed() + c.blocks > VMX_BLOCKS) { sErr(); return toast("NOT ENOUGH BLOCKS ON THE VMX"); }
        ST.vmx.push(c.id); save(); toast(c.title + " IS ON YOUR VMX"); }],
      ["PLAY ON TV", () => { S.tab = 3; S.sel.play = ST.hdd.indexOf(c.id); S.zone = "content"; startGame(c); }],
      ["DELETE FROM HARD DISK", () => { ST.hdd = ST.hdd.filter(x => x !== c.id); save(); toast(c.title + " DELETED"); }],
    ]);
  } else {
    const c = byId(ST.vmx[sel("vmx", ST.vmx.length)]); if (!c) return;
    openMenu(c.title, [
      ["REMOVE FROM VMX", () => { ST.vmx = ST.vmx.filter(x => x !== c.id); save(); toast(c.title + " REMOVED FROM THE VMX"); }],
      [ST.hdd.includes(c.id) ? "SAVES: COPY NEWEST BOTH WAYS" : "COPY TO HARD DISK", () => {
        if (!ST.hdd.includes(c.id)) ST.hdd.push(c.id); save(); toast("COPIED"); }],
    ]);
  }
}
async function call() {
  const games = CAT.filter(c => recordOf(c) && !ST.sent[c.id]);
  if (!ST.vmxIn) { sErr(); return toast("PUT A VMX IN CONTROLLER 1"); }
  if (!games.length) { sErr(); return toast("NO NEW SCORES: PLAY SOMETHING FIRST"); }
  const recs = games.map(recordOf);
  const msg = M.packMessage({ serial: ST.serial, name: ST.owner, records: recs });
  const sr = audio()?.sampleRate || 48000;
  const x = M.callAudio(msg, { sr, square: true });
  // the service's side: through a phone line, decoded, before the voice reads the code back
  const got = M.decode(M.phoneLine(x, sr, { noise: 0.02, seed: ST.serial }), 8000)
    .filter(g => g.msg.serial === ST.serial && g.msg.records.length === recs.length);
  const { plan, len } = M.callPlan(msg);
  S.call = { n: recs.length, code: M.checkCode(msg), heard: got.length ? M.checkCode(got[0].raw) : null, copies: got.length,
    games: games.map(g => g.id), stage: "dial", t0: performance.now(), plan, len, x, sr };
  try { callSrc?.stop(); } catch {}
  callSrc = playSamples(x, sr);
  sOk(); draw();
}
function tickCall(now) {
  const C = S.call; if (C.stage === "done") return;
  const t = (now - C.t0) / 1000;
  const digits = C.plan.filter(p => p.kind === "dtmf" && p.t <= t).map(p => p.d).join("");
  const data = C.plan.filter(p => p.kind === "data"), first = data[0], last = data[data.length - 1];
  const stage = t < first.t ? "dial" : t < last.t + last.dur ? "send" : t < C.len + 1.2 ? "check" : "done";
  C.digits = digits.length > 1 ? "1-" + digits.slice(1, 4) + (digits.length > 4 ? "-" + digits.slice(4) : "") : digits;
  C.p = (t - first.t) / (last.t + last.dur - first.t);
  if (stage !== C.stage) { C.stage = stage; if (stage === "done") { if (C.heard === C.code) sOk(); else sErr(); } draw(); }
  // the scope: the last 20 ms of what the beeper is playing
  const sc = el("#scope"); if (sc) {
    const g = sc.getContext("2d"), n = Math.round(0.02 * C.sr), at = Math.max(0, Math.min(C.x.length - n, Math.round(t * C.sr)));
    g.fillStyle = "#020602"; g.fillRect(0, 0, sc.width, sc.height); g.strokeStyle = ON; g.lineWidth = 1.5; g.beginPath();
    for (let i = 0; i < sc.width; i++) { const v = C.x[at + Math.floor(i * n / sc.width)] || 0; g.lineTo(i, sc.height / 2 - v * sc.height * 0.7); }
    g.stroke();
  }
  callScreen();
}
function endCall(accept) {
  const C = S.call; S.call = null; if (S.tab !== 3) device.remove(); try { callSrc?.stop(); } catch {} callSrc = null;
  if (accept && C.stage === "done" && C.heard === C.code) {
    const when = new Date().toISOString().slice(5, 10).replace("-", "/");
    for (const id of C.games) ST.sent[id] = when;
    save(); toast(C.n + " SCORES SENT");
  } else sBack();
  draw();
}

// ---------------------------------------------------------------- input
function press(b) {                                       // b: up down left right a b lt rt back
  audio();
  if (S.play) {
    if (b === "back") return stopGame();
    if (BTN.includes(b)) { held[b] = true; latch[b] = true; }
    return;
  }
  if (S.call) { if (b === "a" && S.call.stage === "done") endCall(true); else if (b === "b") endCall(false); return; }
  if (S.applet) { if (b === "b" || b === "back") { S.applet = null; sBack(); draw(); } return; }
  if (S.menu) {
    const m = S.menu;
    if (b === "up" || b === "down") { m.k = (m.k + (b === "up" ? -1 : 1) + m.items.length) % m.items.length; sMove(); }
    else if (b === "a") { S.menu = null; m.items[m.k][1](); }
    else if (b === "b") { S.menu = null; sBack(); }
    return draw();
  }
  if (b === "lt" || b === "rt" || (S.zone === "tabs" && (b === "left" || b === "right"))) {
    const d = b === "lt" || b === "left" ? -1 : 1; S.tab = (S.tab + d + TABS.length) % TABS.length; sMove(); return draw();
  }
  if (S.zone === "tabs") {
    if (b === "a" || b === "down") { S.zone = "content"; sOk(); }
    return draw();
  }
  if (b === "b" || b === "back") { S.zone = "tabs"; sBack(); return draw(); }
  const key = ["news", "shop", S.col ? "vmx" : "hdd", "play", "scores", "applets"][S.tab];
  const n = [NEWS.length, CAT.length, (S.col ? ST.vmx : ST.hdd).length, ST.hdd.length,
    CAT.filter(c => ST.hdd.includes(c.id) && !c.scoreless).length, APPLETS.length][S.tab];
  if (b === "up" || b === "down") {
    const k = sel(key, n), nk = Math.max(0, Math.min(n - 1, k + (b === "up" ? -1 : 1)));
    if (b === "up" && k === 0) { S.zone = "tabs"; sBack(); return draw(); }
    if (nk !== k) sMove(); S.sel[key] = nk; return draw();
  }
  if (S.tab === 2 && (b === "left" || b === "right")) { const c = b === "left" ? 0 : 1; if (c !== S.col) { S.col = c; sMove(); } return draw(); }
  if (b !== "a") return;
  if (S.tab === 1) download(CAT[sel("shop", CAT.length)]);
  else if (S.tab === 2) mineAction();
  else if (S.tab === 3) { const c = byId(ST.hdd[sel("play", ST.hdd.length)]); if (c) startGame(c); }
  else if (S.tab === 4) call();
  else if (S.tab === 5) { const a = APPLETS[sel("applets", APPLETS.length)];
    if (!a.from) sErr(); else if (!ST.hdd.includes(a.from)) { sErr(); toast("GET " + byId(a.from).title + " FIRST"); } else { S.applet = a; sOk(); } }
  draw();
}
function release(b) { if (BTN.includes(b)) held[b] = false; }

const KEYS = { ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right", z: "a", Z: "a", " ": "a", Enter: "a",
  x: "b", X: "b", Escape: "b", q: "lt", Q: "lt", e: "rt", E: "rt", Backspace: "back" };
addEventListener("keydown", ev => {
  if (ev.key === "v" || ev.key === "V") { ST.vmxIn = !ST.vmxIn; save(); toast(ST.vmxIn ? "VMX PLUGGED IN" : "VMX TAKEN OUT"); return; }
  const b = KEYS[ev.key]; if (!b) return; ev.preventDefault();
  if (ev.repeat && !["up", "down", "left", "right"].includes(b)) return;
  if (ev.repeat && S.play) return;
  press(b);
});
addEventListener("keyup", ev => { const b = KEYS[ev.key]; if (b) release(b); });
// the pad: standard mapping (A 0, B 1, LB 4, RB 5, LT 6, RT 7, BACK 8, D-pad 12-15, left stick)
const padPrev = {}, padRep = {};
function pollPads() {
  const now = {};
  for (const p of navigator.getGamepads ? navigator.getGamepads() : []) {
    if (!p) continue;
    const bt = i => !!p.buttons[i]?.pressed;
    now.up ||= bt(12) || p.axes[1] < -0.5; now.down ||= bt(13) || p.axes[1] > 0.5;
    now.left ||= bt(14) || p.axes[0] < -0.5; now.right ||= bt(15) || p.axes[0] > 0.5;
    now.a ||= bt(0); now.b ||= bt(1); now.lt ||= bt(4) || bt(6); now.rt ||= bt(5) || bt(7); now.back ||= bt(8);
  }
  for (const b of ["up", "down", "left", "right", "a", "b", "lt", "rt", "back"]) {
    if (now[b] && !padPrev[b]) { press(b); padRep[b] = 0; }
    else if (now[b] && !S.play && ["up", "down", "left", "right"].includes(b) && ++padRep[b] > 18 && padRep[b] % 5 === 0) press(b);
    else if (!now[b] && padPrev[b]) release(b);
    padPrev[b] = now[b];
  }
}
// mouse: tabs and rows are clickable too
el("#tv").addEventListener("click", ev => {
  audio();
  const t = ev.target.closest("[data-tab]"), r = ev.target.closest("[data-key]"), m = ev.target.closest("[data-menu]");
  if (m && S.menu) { S.menu.k = +m.dataset.menu; return press("a"); }
  if (t) { S.tab = +t.dataset.tab; S.zone = "tabs"; sMove(); return draw(); }
  if (r) { const k = r.dataset.key, i = +r.dataset.i;
    if (k === "vmx" || k === "hdd") S.col = k === "vmx" ? 1 : 0;
    const again = S.zone === "content" && S.sel[k] === i; S.sel[k] = i; S.zone = "content";
    if (again) press("a"); else { sMove(); draw(); } }
});

// ---------------------------------------------------------------- the frame loop
let acc = 0, last = performance.now();
function frame(now) {
  pollPads();
  const dt = Math.min(0.25, (now - last) / 1000); last = now;
  if (S.dl) { S.dl.p = Math.min(1, (now - S.dl.t0) / S.dl.dur);
    if (S.dl.p >= 1) { const c = byId(S.dl.id); ST.hdd.push(c.id); save(); S.dl = null; sOk(); toast(c.title + " DOWNLOADED"); }
    if (S.tab === 1) draw(); }
  if (S.play) {
    acc += dt;
    while (acc >= 1 / 30 && S.play) { acc -= 1 / 30; tickGame(); }
    if (S.play) blit(S.play.a.draw(S.play.g));
  } else acc = 0;
  if (S.call) tickCall(now);
  if (S.toast && now - S.toast.at > 2600) { S.toast = null; draw(); }
  requestAnimationFrame(frame);
}
// scale the 640 x 480 TV to the window
function fit() { const k = Math.min(innerWidth / 640, (innerHeight - 8) / 480); el("#tv").style.setProperty("--k", k); }
addEventListener("resize", fit); fit();
draw(); requestAnimationFrame(frame);
if (new URLSearchParams(location.search).has("debug")) window.WORLD = { S, ST, CAT, press, release, draw, M };
