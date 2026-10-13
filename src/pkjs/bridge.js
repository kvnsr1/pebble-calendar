'use strict';
var source = require('./calendar-source');

module.exports = function startBridge(Pebble, getSource) {
  var generation = 0, token = 0, queue = [], sending = false, pending = null;
  var stopped = false, subscription = null, retries = 0, watchdog = null, received = false;
  var session = 0;
  var codes = ['PLUGIN_UNAVAILABLE', 'PERMISSION_DENIED', 'INVALID_DATA', 'UNKNOWN', 'TIMEOUT'];

  function error(code) {
    var safe = codes.indexOf(code) >= 0 ? code : 'UNKNOWN';
    pending = null; queue = [];
    // Send only a fixed code; never send/log host exception messages or calendar contents.
    queue.push({ TYPE: source.TYPES.ERROR, ERROR: safe, TOKEN: token });
    pump();
  }
  function pump() {
    if (sending || stopped) return;
    if (!queue.length && pending) {
      queue = source.messages(pending, ++generation, token); pending = null; retries = 0;
    }
    if (!queue.length) return;
    sending = true;
    var message = queue[0];
    Pebble.sendAppMessage(message, function() {
      sending = false;
      if (queue[0] === message) queue.shift();
      retries = 0; pump();
    }, function() {
      sending = false;
      if (queue[0] !== message) { pump(); return; }
      if (++retries <= 2) setTimeout(pump, 500 * retries);
      else { queue = []; pending = null; retries = 0; }
    });
  }
  function request(newToken) {
    token = newToken;
    var active = ++session;
    // A refresh cancels older envelopes logically using the watch-provided token.
    queue = []; pending = null; received = false;
    if (subscription) subscription.unsubscribe();
    if (watchdog) clearTimeout(watchdog);
    watchdog = setTimeout(function() { if (active === session && !received) error('TIMEOUT'); }, 15000);
    try {
      subscription = getSource(function(snapshot) {
        if (stopped || active !== session) return;
        received = true; clearTimeout(watchdog);
        pending = snapshot; pump();
      }, function(code) { if (active === session) { received = true; clearTimeout(watchdog); error(code); } });
    } catch (_) { error('UNKNOWN'); }
  }
  Pebble.addEventListener('appmessage', function(event) {
    var p = event.payload;
    if (p.TYPE === source.TYPES.CLEAR) {
      token = p.TOKEN; ++session; pending = null; queue = [];
      if (subscription) subscription.unsubscribe(); subscription = null;
      if (watchdog) clearTimeout(watchdog);
    } else if (p.TYPE === source.TYPES.REQUEST) request(p.TOKEN);
  });
  // The watch requests again after ready/reconnect; source errors don't destroy valid offline data.
  Pebble.addEventListener('ready', function() { queue.push({ TYPE: source.TYPES.READY }); pump(); });
  return { stop: function() {
    stopped = true; ++session; queue = []; pending = null;
    if (subscription) subscription.unsubscribe();
    if (watchdog) clearTimeout(watchdog);
  } };
};
