// Regressionstest fuer src/pkjs/index.js.
//
//   node tools/pkjs_race_test.js src/pkjs/index.js [alte-fassung.js]
//
// Erzwingt das Rennen, das bis 1.2.1 in der Datei steckte: mehrere
// AppMessages werden hintereinander verarbeitet, WAEHREND der
// getTimelineToken-Callback noch aussteht. Gesendet wird erst danach. Ein
// gemeinsames, pro Nachricht umgeschriebenes Pin-Objekt schickt dann in jeder
// Anfrage den Inhalt der letzten Nachricht - die URL traegt aber noch die
// richtige ID. Der Test prueft je Anfrage, dass URL-ID, body.id, Untertitel,
// Launch-Code und das Zeitfeld zueinander passen.
//
// KEY_DURATION und KEY_TOTAL_TIME sind Sekunden (src/c/phone.c).
// Exitcode 0 = alles stimmig.
'use strict';
const fs = require('fs'), vm = require('vm');

function run(file, msgs) {
  const sent = [], logs = [], pending = [];
  let handler = null;
  function XHR() { this.responseText = '{"ok":true}'; }
  XHR.prototype.open = function (m, u) { this._m = m; this._u = u; };
  XHR.prototype.setRequestHeader = function () {};
  XHR.prototype.send = function (b) {
    sent.push({ method: this._m, url: this._u, body: b, xhr: this });
  };
  const sandbox = {
    XMLHttpRequest: XHR, console: { log: (s) => logs.push(String(s)) },
    Date, Math, JSON, parseInt, parseFloat, isNaN, setTimeout: (f) => f(),
    Pebble: {
      addEventListener: (ev, fn) => { if (ev === 'appmessage') handler = fn; },
      getTimelineToken: (ok) => { pending.push(ok); },
    },
  };
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(file, 'utf8'), sandbox, { filename: file });
  if (!handler) throw new Error(file + ': kein appmessage-Handler registriert');
  msgs.forEach((p) => handler({ payload: p }));
  pending.forEach((ok) => ok('emulated-dummy-token'));
  sent.forEach((r) => r.xhr.onload && r.xhr.onload.call(r.xhr));
  return { sent, logs };
}

// Timer 11 (25 min) und 22 (90 min) anlegen, danach 11 loeschen
const MSGS = [
  { KEY_UNIQUEID: 11, KEY_TOTAL_TIME: 1500, KEY_DURATION: 1500 },
  { KEY_UNIQUEID: 22, KEY_TOTAL_TIME: 5400, KEY_DURATION: 5400 },
  { KEY_UNIQUEID: 11, KEY_TOTAL_TIME: 0, KEY_DURATION: 0 },
];
const WANT = {
  11: [{ method: 'PUT', subtitle: '00:25', launchCode: 1110, timeSet: true },
       { method: 'DELETE', subtitle: '00:00', launchCode: 1110, timeSet: false }],
  22: [{ method: 'PUT', subtitle: '01:30', launchCode: 2210, timeSet: true }],
};

function check(label, file, strict) {
  console.log('===== ' + label + ' =====');
  const { sent, logs } = run(file, MSGS);
  const seen = {};
  let bad = 0;
  for (const r of sent) {
    const id = r.url.replace(/.*\//, '');
    const b = JSON.parse(r.body);
    seen[id] = seen[id] || 0;
    const want = (WANT[id] || [])[seen[id]++];
    const timeSet = b.time !== 0 && b.time !== '0';
    const ok = !!want && id === String(b.id) && r.method === want.method &&
               b.layout.subtitle === want.subtitle &&
               b.actions[0].launchCode === want.launchCode &&
               timeSet === want.timeSet;
    if (!ok) bad++;
    console.log('  ' + r.method.padEnd(6) + ' /' + id +
                '  body.id=' + String(b.id).padEnd(3) +
                ' subtitle=' + b.layout.subtitle +
                ' launchCode=' + String(b.actions[0].launchCode).padEnd(5) +
                ' time=' + (timeSet ? 'gesetzt' : '0') +
                '  -> ' + (ok ? 'stimmig' : 'FALSCH'));
  }
  console.log('  Protokoll: ' +
    JSON.stringify(logs.filter((l) => /^Pin (Sent|Deleted) Result/.test(l))));
  return strict ? bad : 0;
}

const current = process.argv[2] || 'src/pkjs/index.js';
let bad = check('aktuell: ' + current, current, true);
if (process.argv[3]) check('Vergleich: ' + process.argv[3], process.argv[3], false);
console.log('\nFehlerhafte Anfragen im aktuellen Stand: ' + bad);
process.exit(bad === 0 ? 0 : 1);
