'use strict';
var source = require('./calendar-source');
require('./bridge')(Pebble, function(onData) {
  var now = new Date(), day = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  function at(offset, hour, minute) {
    return Math.floor(new Date(day.getFullYear(), day.getMonth(), day.getDate() + offset, hour, minute || 0).getTime() / 1000);
  }
  var seconds = Math.floor(now.getTime() / 1000);
  var events = [
    { title: 'Synthetic retrospective', start: at(-1, 12), end: at(-1, 13), flags: 0, calendar: 'Uber', location: 'Example studio' },
    { title: 'Focus day', start: Date.UTC(day.getFullYear(), day.getMonth(), day.getDate()) / 1000,
      end: Date.UTC(day.getFullYear(), day.getMonth(), day.getDate() + 1) / 1000, flags: source.FLAGS.ALL_DAY, calendar: 'Gmail', location: '' },
    { title: 'Design review', start: seconds - 600, end: seconds + 1800, flags: 0, calendar: 'Uber', location: 'Studio A' },
    { title: 'Project catch-up and release planning discussion', start: seconds + 600, end: seconds + 2700, flags: 0, calendar: 'Uber', location: 'Conference room' },
    { title: 'An evening walk with friends', start: at(1, 17, 30), end: at(1, 18, 30), flags: 0, calendar: 'Gmail', location: 'Riverside park' },
    { title: 'Overnight maintenance window', start: at(2, 23, 30), end: at(3, 1), flags: 0, calendar: 'Uber', location: '' }
  ].map(function(event) {
    return source.enrich(event, [], false, false);
  }).sort(function(a, b) { return a.start - b.start; });
  onData({ events: events, updated: seconds, demo: true });
  return { unsubscribe: function() {} };
});
