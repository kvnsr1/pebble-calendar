'use strict';

var BUILTIN_UUID = '00000000-0000-0000-0001-000000000004';
var MAX_EVENTS = 10;
var TYPES = { REQUEST: 1, BEGIN: 2, EVENT: 3, COMMIT: 4, ERROR: 5, CLEAR: 6, READY: 7 };
var FLAGS = { ALL_DAY: 1, TRUNCATED: 2 };

function timestamp(property) {
  var value = property && property.timestamp && property.timestamp.value;
  if (typeof value !== 'number' || !isFinite(value) || Math.floor(value) !== value || value <= 0 || value > 2147483647) {
    throw new Error('INVALID_DATA');
  }
  return value;
}

// Bound UTF-8 bytes without splitting a Unicode character (C buffers include a NUL).
function boundedText(value, maxBytes) {
  var text = typeof value === 'string' ? value.replace(/[\u0000-\u001f\u007f]/g, ' ') : '';
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

function normalize(envelope, now) {
  if (!envelope || envelope.pluginUuid !== BUILTIN_UUID || !Array.isArray(envelope.instances) || envelope.instances.length > MAX_EVENTS) {
    throw new Error('INVALID_DATA');
  }
  var events = envelope.instances.map(function(instance) {
    var p = instance && instance.properties;
    if (!p || !p.all_day || !p.all_day.boolean || typeof p.all_day.boolean.value !== 'boolean') throw new Error('INVALID_DATA');
    var start = timestamp(p.starts_at), end = timestamp(p.ends_at);
    if (end <= start) throw new Error('INVALID_DATA');
    var title = boundedText(text(p.name).trim() || 'Untitled event', 255);
    var location = boundedText(text(p.location), 127), calendar = boundedText(text(p.calendar), 63);
    return { title: title.text, location: location.text, calendar: calendar.text, start: start, end: end,
      flags: (p.all_day.boolean.value ? FLAGS.ALL_DAY : 0) | (title.truncated || location.truncated || calendar.truncated ? FLAGS.TRUNCATED : 0) };
  }).sort(function(a, b) { return a.start - b.start || a.title.localeCompare(b.title); });
  // A complete source envelope is the unit of replacement. Never merge positional IDs.
  return { events: events, updated: Math.floor(now / 1000), demo: false };
}

function messages(snapshot, generation, token) {
  var result = [{ TYPE: TYPES.BEGIN, GENERATION: generation, TOKEN: token, VERSION: 1,
    COUNT: snapshot.events.length, UPDATED: snapshot.updated, DEMO: snapshot.demo ? 1 : 0 }];
  snapshot.events.forEach(function(event, index) {
    result.push({ TYPE: TYPES.EVENT, GENERATION: generation, TOKEN: token, INDEX: index,
      TITLE: event.title, LOCATION: event.location, CALENDAR: event.calendar,
      START: event.start, END: event.end, FLAGS: event.flags });
  });
  result.push({ TYPE: TYPES.COMMIT, GENERATION: generation, TOKEN: token });
  return result;
}

module.exports = { BUILTIN_UUID: BUILTIN_UUID, MAX_EVENTS: MAX_EVENTS, TYPES: TYPES, FLAGS: FLAGS,
  boundedText: boundedText, normalize: normalize, messages: messages };
