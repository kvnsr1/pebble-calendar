'use strict';
var metadata = require('./event-metadata');

var BUILTIN_UUID = '00000000-0000-0000-0001-000000000004';
var MAX_EVENTS = 10;
var TYPES = { REQUEST: 1, BEGIN: 2, EVENT: 3, COMMIT: 4, ERROR: 5, CLEAR: 6, READY: 7 };
var FLAGS = { ALL_DAY: 1, TRUNCATED: 2, ATTENDEES_AVAILABLE: 4 };

function timestamp(property) {
  var value = property && property.timestamp && property.timestamp.value;
  if (typeof value !== 'number' || !isFinite(value) || Math.floor(value) !== value || value <= 0 || value > 2147483647) {
    throw new Error('INVALID_DATA');
  }
  return value;
}

// Bound UTF-8 bytes without splitting a Unicode character (C buffers include a NUL).
function boundedText(value, maxBytes, keepLines) {
  var controls = keepLines ? /[\u0000-\u0009\u000b-\u001f\u007f]/g : /[\u0000-\u001f\u007f]/g;
  var text = typeof value === 'string' ? value.replace(controls, ' ') : '';
  var result = '', bytes = 0;
  for (var i = 0; i < text.length; i++) {
    var code = text.charCodeAt(i), piece = text.charAt(i), size;
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < text.length && text.charCodeAt(i + 1) >= 0xdc00 && text.charCodeAt(i + 1) <= 0xdfff) {
      piece += text.charAt(++i); size = 4;
    } else if (code >= 0xd800 && code <= 0xdfff) {
      piece = '\ufffd'; size = 3;
    } else { size = code < 0x80 ? 1 : code < 0x800 ? 2 : 3; }
    if (bytes + size > maxBytes) return { text: result, truncated: true };
    result += piece; bytes += size;
  }
  return { text: result, truncated: false };
}

function text(property) {
  return property && ((property.longText && property.longText.text) || (property.shortText && property.shortText.text)) || '';
}

function enrich(event, attendees, attendeesAvailable, allowLocationFallback) {
  var extracted = metadata.extract(attendees, event.location, allowLocationFallback);
  var truncated = false;
  function clip(value, limit, keepLines) {
    var bounded = boundedText(value, limit, keepLines);
    truncated = truncated || bounded.truncated;
    return bounded.text;
  }
  var result = { title: clip(event.title, 255), location: clip(event.location, 127),
    calendar: clip(event.calendar, 63), start: event.start, end: event.end, flags: event.flags || 0,
    attendees: '', room: null };
  // Preserve separators only in the already-sanitized list of people, not arbitrary input.
  var people = extracted.people.map(function(name) { return clip(name, 383); });
  result.attendees = clip(people.join('\n'), 383, true);
  if (extracted.room) {
    var room = extracted.room;
    result.room = { name: clip(room.name, 63), office: clip(room.office, 15), building: clip(room.building, 15),
      floor: room.floor, number: clip(room.number, 15), capacity: room.capacity,
      facilities: clip(room.facilities, 95), source: room.source };
  }
  if (attendeesAvailable) result.flags |= FLAGS.ATTENDEES_AVAILABLE;
  if (truncated) result.flags |= FLAGS.TRUNCATED;
  return result;
}

function normalize(envelope, now, attendeesSupported) {
  if (!envelope || envelope.pluginUuid !== BUILTIN_UUID || !Array.isArray(envelope.instances) || envelope.instances.length > MAX_EVENTS) {
    throw new Error('INVALID_DATA');
  }
  var events = envelope.instances.map(function(instance) {
    var p = instance && instance.properties;
    if (!p || !p.all_day || !p.all_day.boolean || typeof p.all_day.boolean.value !== 'boolean') throw new Error('INVALID_DATA');
    var start = timestamp(p.starts_at), end = timestamp(p.ends_at);
    if (end <= start) throw new Error('INVALID_DATA');
    var names = [], available = false;
    if (attendeesSupported === true && Object.prototype.hasOwnProperty.call(p, 'attendees')) {
      if (!p.attendees || !p.attendees.longText || typeof p.attendees.longText.text !== 'string') {
        throw new Error('INVALID_DATA');
      }
      // The Core enhancement declares longText with one display name per line. Never split
      // on commas: room resources contain commas in their facilities list.
      names = p.attendees.longText.text.split('\n');
      available = true;
    }
    return enrich({ title: text(p.name).trim() || 'Untitled event', location: text(p.location),
      calendar: text(p.calendar), start: start, end: end, flags: p.all_day.boolean.value ? FLAGS.ALL_DAY : 0 }, names, available, true);
  }).sort(function(a, b) { return a.start - b.start || a.title.localeCompare(b.title); });
  // A complete source envelope is the unit of replacement. Never merge positional IDs.
  return { events: events, updated: Math.floor(now / 1000), demo: false };
}

function messages(snapshot, generation, token) {
  var result = [{ TYPE: TYPES.BEGIN, GENERATION: generation, TOKEN: token, VERSION: 2,
    COUNT: snapshot.events.length, UPDATED: snapshot.updated, DEMO: snapshot.demo ? 1 : 0 }];
  snapshot.events.forEach(function(event, index) {
    var room = event.room || {};
    result.push({ TYPE: TYPES.EVENT, GENERATION: generation, TOKEN: token, INDEX: index,
      TITLE: event.title, LOCATION: event.location, CALENDAR: event.calendar,
      START: event.start, END: event.end, FLAGS: event.flags, ATTENDEES: event.attendees || '',
      ROOM_NAME: room.name || '', OFFICE: room.office || '', BUILDING: room.building || '',
      FLOOR: room.floor || 0, ROOM_NUMBER: room.number || '', CAPACITY: room.capacity || 0,
      FACILITIES: room.facilities || '', ROOM_SOURCE: room.source || 0 });
  });
  result.push({ TYPE: TYPES.COMMIT, GENERATION: generation, TOKEN: token });
  return result;
}

module.exports = { BUILTIN_UUID: BUILTIN_UUID, MAX_EVENTS: MAX_EVENTS, TYPES: TYPES, FLAGS: FLAGS,
  boundedText: boundedText, normalize: normalize, messages: messages, enrich: enrich };
