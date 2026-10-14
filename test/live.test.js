'use strict';
var test = require('node:test'), assert = require('node:assert/strict');
var vm = require('node:vm'), fs = require('node:fs'), source = require('../src/pkjs/calendar-source');
var script = fs.readFileSync(require.resolve('../src/pkjs/index'), 'utf8');
function read(host) {
  var configure;
  vm.runInNewContext(script, { Pebble: host, Date: Date, require: function(name) {
    if (name === './calendar-source') return source;
    if (name === './bridge') return function(_, fn) { configure = fn; };
    throw new Error('Unexpected dependency');
  } });
  var data = [], errors = [];
  var subscription = configure(function(s) { data.push(s); }, function(e) { errors.push(e); });
  return { data: data, errors: errors, subscription: subscription };
}
test('live build reports missing APIs instead of falling back to synthetic data', function() {
  var result = read({});
  assert.deepEqual(result.errors, ['PLUGIN_UNAVAILABLE']); assert.deepEqual(result.data, []);
});
test('live build refuses a substitute provider', function() {
  var subscribed = false;
  var result = read({ enumeratePlugins: function() { return [{ uuid: 'different' }]; }, subscribeToSource: function() { subscribed = true; } });
  assert.deepEqual(result.errors, ['PLUGIN_UNAVAILABLE']); assert.equal(subscribed, false);
});
test('live source uses the built-in calendar contract and validates every returned provider', function() {
  var configuration;
  var result = read({ enumeratePlugins: function() { return [{ uuid: source.BUILTIN_UUID }]; }, subscribeToSource: function(config) {
    configuration = config; return { unsubscribe: function() {} };
  } });
  assert.equal(configuration.category, 'calendar'); assert.equal(configuration.item, 'event');
  assert.equal(configuration.plugin, source.BUILTIN_UUID);
  assert.ok(configuration.properties.includes('calendar'));
  assert.equal(configuration.properties.includes('attendees'), false);
  configuration.onData({ pluginUuid: 'fallback', instances: [] });
  assert.deepEqual(result.errors, ['INVALID_DATA']); assert.equal(result.data.length, 0);
  configuration.onData({ pluginUuid: source.BUILTIN_UUID, instances: [] });
  assert.equal(result.data.length, 1); assert.deepEqual(result.data[0].events, []);
  configuration.onError({ code: 'PERMISSION_DENIED', message: 'Do not copy this private detail' });
  assert.deepEqual(result.errors, ['INVALID_DATA', 'PERMISSION_DENIED']);
});

test('minimal live source does not request or retain unused attendee names even when advertised', function() {
  var configuration;
  var declarations = [
    { category: 'other', items: ['event'], properties: { attendees: ['longText'] } },
    { category: 'calendar', items: ['event'], properties: { attendees: ['longText'] } }
  ];
  var result = read({ enumeratePlugins: function() { return [{ uuid: source.BUILTIN_UUID, sources: declarations }]; },
    subscribeToSource: function(config) { configuration = config; return {}; } });
  assert.equal(configuration.properties.includes('attendees'), false);
  configuration.onData({ pluginUuid: source.BUILTIN_UUID, instances: [{ properties: {
    name: { longText: { text: 'Synthetic review' } }, starts_at: { timestamp: { value: 1900000000 } },
    ends_at: { timestamp: { value: 1900003600 } }, all_day: { boolean: { value: false } },
    location: { longText: { text: 'ABC | MB2-3rd-3.07 Example Room (8)' } },
    attendees: { longText: { text: 'Alex Sample\nABC | MB2-3rd-3.07 Example Room (8) [Team Room, Video]' } }
  } }] });
  assert.deepEqual(result.errors, []);
  assert.equal(result.data[0].events[0].attendees, '');
  assert.equal(result.data[0].events[0].room, null);
  assert.equal(result.data[0].events[0].location, 'ABC | MB2-3rd-3.07 Example Room (8)');
});
test('wrong source or attendee shape cannot enable an unadvertised extension', function() {
  [
    { category: 'other', items: ['event'], properties: { attendees: ['longText'] } },
    { category: 'calendar', items: ['other'], properties: { attendees: ['longText'] } },
    { category: 'calendar', items: ['event'], properties: { attendees: ['shortText'] } }
  ].forEach(function(declaration) {
    var configuration;
    read({ enumeratePlugins: function() { return [{ uuid: source.BUILTIN_UUID, sources: [declaration] }]; },
      subscribeToSource: function(config) { configuration = config; return {}; } });
    assert.equal(configuration.properties.includes('attendees'), false);
  });
});
