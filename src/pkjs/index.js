// ********** Timeline ********** //
// "Timer abgelaufen"-Pin. Fuer JEDEN Aufruf wird ein eigenes Objekt gebaut.
//
// Vorher stand hier ein einziges Modul-Objekt, das der AppMessage-Handler pro
// Nachricht umgeschrieben hat. Gesendet wird aber erst im asynchronen
// getTimelineToken-Callback: startet man zwei Timer schnell hintereinander,
// trugen beide Rumpfdaten den Inhalt des ZWEITEN, waehrend die URL des ersten
// noch dessen eigene ID trug. Pin 1 bekam also Titel, Restzeit und Launch-Code
// von Pin 2. Dasselbe galt fuer die Protokollzeile in der Antwort, die die ID
// ebenfalls erst spaeter aus dem gemeinsamen Objekt las.

// Die Texte des Pins. Welche Spalte gilt, sagt die Uhr per KEY_LANG - das
// Telefon kann die Uhrsprache nicht von sich aus erfahren. Index 0 ist
// Englisch und zugleich der Rueckfall, genau wie in src/c/strings_table.h.
var PIN_TEXT = [
  { title: 'Timer expired', open: 'Open timer' },
  { title: 'Timer abgelaufen', open: 'Timer öffnen' }
];

function makeTimerPin(id, totalTimeSec, durationSec, lang) {
  var t = PIN_TEXT[lang] || PIN_TEXT[0];
  // Untertitel = Gesamtdauer als HH:MM
  var tot = totalTimeSec / 60;
  var hr = Math.floor(tot / 60);
  var min = Math.floor(tot % 60);
  if (hr < 10) hr = "0" + hr;
  if (min < 10) min = "0" + min;

  // Nur ein anzulegender Pin braucht eine Zeit; beim Loeschen bleibt sie 0.
  var time = 0;
  if (durationSec > 0) {
    var tDate = new Date();
    tDate.setSeconds(tDate.getSeconds() + durationSec);
    time = tDate.toISOString();
  }

  return {
    "id": id,
    "time": time,
    "layout": {
      "type": "weatherPin",
      "title": t.title,
      "subtitle": hr + ":" + min,
      "tinyIcon": "system://images/ALARM_CLOCK",
      "largeIcon": "system://images/ALARM_CLOCK",
      "locationName": " ",
      // Muss zu ZM_COLOR_ACCENT in src/c/theme.h passen (handgepflegte Kopie)
      "backgroundColor": "#00AA55",
      "foregroundColor": "#000000"
    },
    "actions": [
      {
        "title": t.open,
        "type": "openWatchApp",
        // launch code = timer id * 100 + Aktion (10 = oeffnen)
        "launchCode": id * 100 + 10
      }
    ]
  };
}

// Timeline-Endpunkt. Der alte Host timeline-api.getpebble.com ist tot - er
// loest auf 0.0.0.0 auf und antwortet nicht mehr; timeline-api.rebble.io lebt.
// Auf Android faellt das bisher nicht auf, weil die Telefon-App von Core
// Devices BEIDE Hosts unter /v1/user/pins selbst abfaengt und den Pin lokal
// anlegt (RemoteTimelineEmulator, standardmaessig an). Ohne diesen Abfang -
// auf dem iPhone, mit der klassischen App, oder wenn die Einstellung
// "Emulate Timeline Webservice" aus ist - ging der Aufruf bisher ins Leere.
var TIMELINE_API = 'https://timeline-api.rebble.io/v1/user/pins/';

// Send pin to the Pebble timeline API; type is 'PUT' (insert) or 'DELETE'.
// ID und Rumpf werden SOFORT festgehalten, nicht erst im Token-Callback - so
// kann kein spaeterer Aufruf den Inhalt dieses Aufrufs mehr veraendern.
function timelineRequest(pin, type, callback) {
  var id = pin.id;
  var body = JSON.stringify(pin);
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    console.log('timeline: response received: ' + this.responseText);
    callback(id, this.responseText);
  };
  xhr.open(type, TIMELINE_API + id);

  Pebble.getTimelineToken(function (token) {
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', '' + token);
    xhr.send(body);
    console.log('timeline: request sent (' + id + ').');
  }, function (error) { console.log('timeline: error getting timeline token: ' + error); });
}

// ********** AppMessage ********** //
// Watch sends KEY_UNIQUEID / KEY_DURATION / KEY_TOTAL_TIME / KEY_LANG
// (src/c/phone.c); KEY_DURATION > 0 inserts the pin, 0 deletes it.
// KEY_LANG ist die Sprache der Uhr (0 = Englisch, 1 = Deutsch); fehlt sie,
// weil eine aeltere Uhrseite laeuft, faellt der Pin auf Englisch zurueck.
Pebble.addEventListener('appmessage', function(e) {
  if (!e.payload.hasOwnProperty('KEY_DURATION')) return;
  // timeline needs SDK 3.0+
  if (typeof Pebble.getTimelineToken != 'function') return;

  var duration = e.payload.KEY_DURATION;
  var pin = makeTimerPin(e.payload.KEY_UNIQUEID.toString(),
                         e.payload.KEY_TOTAL_TIME, duration, e.payload.KEY_LANG);

  if (duration > 0) {
    timelineRequest(pin, 'PUT', function (id, responseText) {
      console.log('Pin Sent Result (' + id + '): ' + responseText);
    });
  } else {
    timelineRequest(pin, 'DELETE', function (id, responseText) {
      console.log('Pin Deleted Result (' + id + '): ' + responseText);
    });
  }
});

Pebble.addEventListener('ready', function(e) {
  console.log("JS ready!");
});
