'use strict';
var source = require('./calendar-source');
require('./bridge')(Pebble, function(onData, onError) {
  if (typeof Pebble.subscribeToSource !== 'function' || typeof Pebble.enumeratePlugins !== 'function') {
    onError('PLUGIN_UNAVAILABLE'); return null;
  }
  var plugins = Pebble.enumeratePlugins();
  var calendarPlugin = plugins.filter(function(plugin) { return plugin.uuid === source.BUILTIN_UUID; })[0];
  if (!calendarPlugin) {
    onError('PLUGIN_UNAVAILABLE'); return null;
  }
  // The minimal UI needs no attendee data; rooms are entered by the user.
  var properties = ['name', 'location', 'starts_at', 'ends_at', 'all_day', 'calendar'];
  return Pebble.subscribeToSource({
    plugin: source.BUILTIN_UUID, category: 'calendar', item: 'event',
    properties: properties,
    onData: function(envelope) {
      try { onData(source.normalize(envelope, Date.now(), false, false)); }
      catch (_) { onError('INVALID_DATA'); }
    },
    onError: function(error) { onError(error && error.code || 'UNKNOWN'); }
  });
});
