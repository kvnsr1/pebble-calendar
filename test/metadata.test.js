'use strict';
var test = require('node:test'), assert = require('node:assert/strict');
var metadata = require('../src/pkjs/event-metadata'), source = require('../src/pkjs/calendar-source');
var resource = 'ABC | 1000 MB2-3rd-3.07 Example Room (8) [Team Room, Video]';

test('resource name yields office, building, floor, room number, room name and capacity', function() {
  assert.deepEqual(metadata.parseRoom(resource), { office: 'ABC', building: 'MB2', floor: 3, number: '7',
    name: 'Example Room', capacity: 8, facilities: 'Team Room, Video' });
});
test('ordinal variants, spacing, facilities omission and alphanumeric rooms are supported', function() {
  var result = metadata.parseRoom(' xyz | 200 MB10 - 21st - 21.04A Sample Studio (12) ');
  assert.equal(result.floor, 21); assert.equal(result.number, '04A'); assert.equal(result.facilities, '');
  assert.equal(metadata.parseRoom('ABC | MB3-2nd-2.02 Meeting Space (4)').number, '2');
});
test('people, ordinary locations and contradictory floor metadata are not rooms', function() {
  ['Alex Sample', 'Conference Room', 'ABC | MB2-3rd-4.07 Room (8)', 'ABC | MB2-3rd-3.07 Room (0)',
    'ABC | MB2-3rd-3.07 Room (eight)'].forEach(function(value) { assert.equal(metadata.parseRoom(value), null); });
});
test('room attendees are separated from people and take priority over a Location fallback', function() {
  var result = metadata.extract(['Alex Sample', { name: resource }, 'Alex Sample', { name: 'Jordan Sample' }],
    'XYZ | MB4-1st-1.02 Other Room (6)', true);
  assert.deepEqual(result.people, ['Alex Sample', 'Jordan Sample']);
  assert.equal(result.room.name, 'Example Room'); assert.equal(result.room.source, 1);
});
test('Location fallback is explicit and never generates an attendee list', function() {
  assert.equal(metadata.extract([], resource, false).room, null);
  var result = metadata.extract([], resource, true);
  assert.equal(result.room.source, 2); assert.deepEqual(result.people, []);
});
test('metadata transport preserves people lines and bounded Unicode strings', function() {
  var event = source.enrich({ title: 'Synthetic meeting', location: '', calendar: 'Work', start: 1900000000,
    end: 1900003600 }, ['Alex Sample', resource, 'Jordan Sample'], true, false);
  assert.equal(event.attendees, 'Alex Sample\nJordan Sample');
  assert.ok(event.flags & source.FLAGS.ATTENDEES_AVAILABLE);
  var message = source.messages({ events: [event], updated: 1900000000 }, 1, 2)[1];
  assert.equal(message.FLOOR, 3); assert.equal(message.ROOM_NUMBER, '7'); assert.equal(message.CAPACITY, 8);
  assert.equal(message.ROOM_SOURCE, 1);
  var long = source.enrich({ title: 'Test', location: '', calendar: '', start: 1900000000, end: 1900003600 },
    Array.from({length: 20}, function(_, index) { return index + ' ' + 'é'.repeat(100); }), true, false);
  assert.ok(Buffer.byteLength(long.attendees, 'utf8') <= 383);
  assert.ok(long.flags & source.FLAGS.TRUNCATED);
});
test('the live contract can parse a Location room without claiming attendees were supplied', function() {
  var envelope = { pluginUuid: source.BUILTIN_UUID, instances: [{ properties: {
    name: { longText: { text: 'Synthetic review' } }, location: { longText: { text: resource } },
    starts_at: { timestamp: { value: 1900000000 } }, ends_at: { timestamp: { value: 1900003600 } },
    all_day: { boolean: { value: false } }, calendar: { shortText: { text: 'Work' } }
  } }] };
  var event = source.normalize(envelope, 1900000000000).events[0];
  assert.equal(event.room.source, 2); assert.equal(event.room.number, '7');
  assert.equal(event.attendees, ''); assert.equal(event.flags & source.FLAGS.ATTENDEES_AVAILABLE, 0);
});
