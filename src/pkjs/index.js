// ********** Timeline ********** //
// "Timer abgelaufen" pin; id, time, subtitle and launchCode are set per timer
var timerPIN = {
  "id": "",
  "time": 0,
  "layout": {
    "type": "weatherPin",
    "title": "Timer abgelaufen",
    "subtitle": "50:00",
    "tinyIcon": "system://images/ALARM_CLOCK",
    "largeIcon": "system://images/ALARM_CLOCK",
    "locationName": " ",
    // Muss zu ZM_COLOR_ACCENT in src/c/theme.h passen (handgepflegte Kopie)
    "backgroundColor": "#00AA55",
    "foregroundColor": "#000000"
  },
  "actions": [
    {
      "title": "Timer oeffnen",
      "type": "openWatchApp",
      "launchCode": 10
    }
  ]
};

// Timeline-Endpunkt. Der alte Host timeline-api.getpebble.com ist tot - er
// loest auf 0.0.0.0 auf und antwortet nicht mehr; timeline-api.rebble.io lebt.
// Auf Android faellt das bisher nicht auf, weil die Telefon-App von Core
// Devices BEIDE Hosts unter /v1/user/pins selbst abfaengt und den Pin lokal
// anlegt (RemoteTimelineEmulator, standardmaessig an). Ohne diesen Abfang -
// auf dem iPhone, mit der klassischen App, oder wenn die Einstellung
// "Emulate Timeline Webservice" aus ist - ging der Aufruf bisher ins Leere.
var TIMELINE_API = 'https://timeline-api.rebble.io/v1/user/pins/';

// Send pin to the Pebble timeline API; type is 'PUT' (insert) or 'DELETE'.
function timelineRequest(pin, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    console.log('timeline: response received: ' + this.responseText);
    callback(this.responseText);
  };
  xhr.open(type, TIMELINE_API + pin.id);

  Pebble.getTimelineToken(function (token) {
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', '' + token);
    xhr.send(JSON.stringify(pin));
    console.log('timeline: request sent.');
  }, function (error) { console.log('timeline: error getting timeline token: ' + error); });
}

// ********** AppMessage ********** //
// Watch sends KEY_UNIQUEID / KEY_DURATION / KEY_TOTAL_TIME (src/c/phone.c);
// KEY_DURATION > 0 inserts the pin, 0 deletes it.
Pebble.addEventListener('appmessage', function(e) {
  if (!e.payload.hasOwnProperty('KEY_DURATION')) return;
  // timeline needs SDK 3.0+
  if (typeof Pebble.getTimelineToken != 'function') return;

  timerPIN.id = e.payload.KEY_UNIQUEID.toString();
  // subtitle = total time as HH:MM
  var tot = e.payload.KEY_TOTAL_TIME / 60;
  var hr = Math.floor(tot / 60);
  var min = Math.floor(tot % 60);
  if (hr < 10) hr = "0" + hr;
  if (min < 10) min = "0" + min;
  timerPIN.layout.subtitle = hr + ":" + min;
  // launch code = timer id * 100 + action (10 = open)
  timerPIN.actions[0].launchCode = timerPIN.id * 100 + 10;

  if (e.payload.KEY_DURATION > 0) {
    var tDate = new Date();
    tDate.setSeconds(tDate.getSeconds() + e.payload.KEY_DURATION);
    timerPIN.time = tDate.toISOString();
    timelineRequest(timerPIN, 'PUT', function (responseText) {
      console.log('Pin Sent Result (' + timerPIN.id + '): ' + responseText);
    });
  } else {
    timelineRequest(timerPIN, 'DELETE', function (responseText) {
      console.log('Pin Deleted Result (' + timerPIN.id + '): ' + responseText);
    });
  }
});

Pebble.addEventListener('ready', function(e) {
  console.log("JS ready!");
});
