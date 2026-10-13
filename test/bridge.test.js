'use strict';
var test = require('node:test'), assert = require('node:assert/strict');
var start = require('../src/pkjs/bridge'), types = require('../src/pkjs/calendar-source').TYPES;
function harness(hold) {
  var listeners = {}, sent = [], callbacks = [], subscriptions = [];
  var bridge = start({
    addEventListener: function(name, fn) { listeners[name] = fn; },
    sendAppMessage: function(message, success, failure) {
      sent.push(message);
      if (hold) callbacks.push({ success: success, failure: failure }); else success();
    }
  }, function(onData, onError) {
    var sub = { data: onData, error: onError, unsubscribed: false };
    subscriptions.push(sub);
    return { unsubscribe: function() { sub.unsubscribed = true; } };
  });
  return { sent: sent, callbacks: callbacks, subscriptions: subscriptions, stop: bridge.stop,
    request: function(token) { listeners.appmessage({ payload: { TYPE: types.REQUEST, TOKEN: token } }); },
    clear: function(token) { listeners.appmessage({ payload: { TYPE: types.CLEAR, TOKEN: token } }); } };
}
function snapshot(title) {
  return { updated: 1900000000, demo: false, events: title ? [{ title: title, location: '', calendar: 'Test', start: 1900000000, end: 1900003600, flags: 0 }] : [] };
}
test('commit waits for event acknowledgments', function() {
  var h = harness(true);
  try {
    h.request(10); h.subscriptions[0].data(snapshot('Synthetic event'));
    assert.deepEqual(h.sent.map(function(m) { return m.TYPE; }), [types.BEGIN]);
    h.callbacks.shift().success();
    assert.equal(h.sent[1].TYPE, types.EVENT);
    h.callbacks.shift().success();
    assert.equal(h.sent[2].TYPE, types.COMMIT);
    assert.equal(h.sent[2].TOKEN, 10);
  } finally { h.stop(); }
});
test('updates replace the whole list including event deletions and empty calendars', function() {
  var h = harness();
  try {
    h.request(1); h.subscriptions[0].data(snapshot('First'));
    h.subscriptions[0].data(snapshot());
    assert.deepEqual(h.sent.filter(function(m) { return m.TYPE === types.BEGIN; }).map(function(m) { return m.COUNT; }), [1, 0]);
    assert.equal(h.sent.filter(function(m) { return m.TYPE === types.COMMIT; }).length, 2);
  } finally { h.stop(); }
});
test('coalesces multiple changes during an active transfer', function() {
  var h = harness(true);
  try {
    h.request(1); h.subscriptions[0].data(snapshot('First'));
    h.subscriptions[0].data(snapshot('Second')); h.subscriptions[0].data(snapshot('Latest'));
    while (h.callbacks.length) h.callbacks.shift().success();
    assert.deepEqual(h.sent.filter(function(m) { return m.TYPE === types.EVENT; }).map(function(m) { return m.TITLE; }), ['First', 'Latest']);
  } finally { h.stop(); }
});
test('old subscriptions cannot repopulate data after refresh or clear', function() {
  var h = harness();
  try {
    h.request(1); var old = h.subscriptions[0]; h.request(2);
    old.data(snapshot('Obsolete')); assert.equal(h.sent.length, 0); assert.ok(old.unsubscribed);
    var current = h.subscriptions[1]; h.clear(3);
    current.data(snapshot('Cleared')); current.error('UNKNOWN');
    assert.equal(h.sent.length, 0); assert.ok(current.unsubscribed);
  } finally { h.stop(); }
});
test('provider errors transmit fixed codes without private exception content', function() {
  var h = harness();
  try {
    h.request(8); h.subscriptions[0].error('A private event name');
    assert.equal(h.sent[0].ERROR, 'UNKNOWN');
    assert.ok(!JSON.stringify(h.sent).includes('private event'));
    h.subscriptions[0].error('PERMISSION_DENIED');
    assert.equal(h.sent[1].ERROR, 'PERMISSION_DENIED');
  } finally { h.stop(); }
});
