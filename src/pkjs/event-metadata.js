'use strict';

// Resource attendees are distinct from people. Parse the room before clipping display text.
function parseRoom(value) {
  if (typeof value !== 'string') return null;
  var match = /^\s*([a-z]{2,5})\s*\|\s*.*?\b([a-z][a-z0-9]*\d[a-z0-9]*)\s*-\s*(\d+)(?:st|nd|rd|th)\s*-\s*(\d+)\.([a-z0-9]+)\s+(.+?)\s*\((\d+)\)\s*(?:\[([^\]]*)\])?\s*$/i.exec(value);
  if (!match) return null;
  var floor = Number(match[3]), roomFloor = Number(match[4]), capacity = Number(match[7]);
  if (floor !== roomFloor || floor > 999 || capacity < 1 || capacity > 10000) return null;
  var number = /^\d+$/.test(match[5]) ? String(Number(match[5])) : match[5].toUpperCase();
  return { office: match[1].toUpperCase(), building: match[2].toUpperCase(), floor: floor,
    number: number, name: match[6].trim(), capacity: capacity, facilities: (match[8] || '').trim() };
}

function attendeeName(value) {
  if (typeof value === 'string') return value.trim();
  return value && typeof value.name === 'string' ? value.name.trim() : '';
}

function extract(attendees, location, allowLocationFallback) {
  var people = [], room = null;
  (Array.isArray(attendees) ? attendees : []).forEach(function(attendee) {
    var name = attendeeName(attendee), resource = parseRoom(name);
    if (resource && !room) { room = resource; room.source = 1; }
    if (!resource && name && people.indexOf(name) < 0) people.push(name);
  });
  if (!room && allowLocationFallback) {
    room = parseRoom(location);
    if (room) room.source = 2;
  }
  return { people: people, room: room };
}

module.exports = { parseRoom: parseRoom, extract: extract };
