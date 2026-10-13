'use strict';
var source = require('./calendar-source');
require('./bridge')(Pebble, function(onData, onError) {
  if (typeof Pebble.subscribeToSource !== 'function' || typeof Pebble.enumeratePlugins !== 'function') {
    onError('PLUGIN_UNAVAILABLE'); return null;
  }
  var plugins = Pebble.enumeratePlugins();
  if (!plugins.some(function(plugin) { return plugin.uuid === source.BUILTIN_UUID; })) {
    onError('PLUGIN_UNAVAILABLE'); return null;
  }
  return Pebble.subscribeToSource({
    plugin: source.BUILTIN_UUID, category: 'calendar', item: 'event',
    properties: ['name', 'location', 'starts_at', 'ends_at', 'all_day', 'calendar'],
    onData: function(envelope) {
      try { onData(source.normalize(envelope, Date.now())); }
      catch (_) { onError('INVALID_DATA'); }
    },
    onError: function(error) { onError(error && error.code || 'UNKNOWN'); }
  });
});
