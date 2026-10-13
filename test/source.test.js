'use strict';
var test = require('node:test'), assert = require('node:assert/strict');
var source = require('../src/pkjs/calendar-source');
function instance(title, start, end, allDay) {
  return { instanceId: '0', properties: {
    name: { longText: { text: title } }, starts_at: { timestamp: { value: start } },
    ends_at: { timestamp: { value: end } }, all_day: { boolean: { value: !!allDay } },
    location: { longText: { text: 'Synthetic room' } }, calendar: { shortText: { text: 'Work' } }
  } };
}
function envelope(instances) { return { pluginUuid: source.BUILTIN_UUID, instances: instances }; }

test('combines and orders overlapping events, retaining raw timestamps and all-day dates', function() {
  var events = [instance('Later', 1900100000, 1900103600), instance('All day', 1900022400, 1900108800, true), instance('Overlap', 1900100100, 1900103700)];
  var result = source.normalize(envelope(events), 1900000000000);
  assert.deepEqual(result.events.map(function(e) { return e.title; }), ['All day', 'Later', 'Overlap']);
  assert.equal(result.events[0].flags, source.FLAGS.ALL_DAY);
  assert.equal(result.events[0].start, 1900022400);
  assert.equal(result.updated, 1900000000);
});
test('empty successful envelope produces an explicit empty replacement', function() {
  var result = source.normalize(envelope([]), 1900000000000);
  assert.deepEqual(source.messages(result, 4, 9).map(function(m) { return m.TYPE; }), [source.TYPES.BEGIN, source.TYPES.COMMIT]);
  assert.equal(source.messages(result, 4, 9)[0].COUNT, 0);
});
test('rejects unexpected providers, malformed metadata and oversized source lists', function() {
  assert.throws(function() { source.normalize({ pluginUuid: 'other', instances: [] }, Date.now()); });
  assert.throws(function() { source.normalize(envelope(Array(11).fill(instance('Meeting', 1900000000, 1900003600))), Date.now()); });
  [NaN, '1900000000', -1, 1900000000.5, 2147483648].forEach(function(start) {
    assert.throws(function() { source.normalize(envelope([instance('Meeting', start, 1900003600)]), Date.now()); });
  });
  assert.throws(function() { source.normalize(envelope([instance('Meeting', 1900000000, 1900000000)]), Date.now()); });
});
test('partial malformed envelope fails as a whole rather than deleting valid cached records', function() {
  assert.throws(function() { source.normalize(envelope([instance('Good', 1900000000, 1900003600), { properties: {} }]), Date.now()); });
});
test('bounded UTF-8 preserves Unicode boundaries and marks shortened metadata', function() {
  var clipped = source.boundedText('A😀é', 5);
  assert.equal(clipped.text, 'A😀'); assert.equal(clipped.truncated, true);
  var event = source.normalize(envelope([instance('é'.repeat(200), 1900000000, 1900003600)]), Date.now()).events[0];
  assert.ok(Buffer.byteLength(event.title, 'utf8') <= 255);
  assert.ok(event.flags & source.FLAGS.TRUNCATED);
  assert.equal(source.boundedText('Room\u0000\nA', 30).text, 'Room  A');
});
test('missing optional strings are allowed and notes never enter the watch payload', function() {
  var item = instance('', 1900000000, 1900003600);
  delete item.properties.location; delete item.properties.calendar;
  item.properties.description = { longText: { text: 'Not to be copied' } };
  var result = source.normalize(envelope([item]), Date.now());
  assert.equal(result.events[0].title, 'Untitled event');
  assert.equal(result.events[0].location, '');
  assert.ok(!JSON.stringify(source.messages(result, 1, 1)).includes('Not to be copied'));
});

module.exports = { instance: instance, envelope: envelope };
