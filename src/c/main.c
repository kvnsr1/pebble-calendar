#include <pebble.h>
#include "calendar.h"

enum { MSG_REQUEST = 1, MSG_BEGIN, MSG_EVENT, MSG_COMMIT, MSG_ERROR, MSG_CLEAR, MSG_READY };
static Window *s_window, *s_details_window, *s_actions_window;
static Layer *s_canvas;
static ScrollLayer *s_scroll;
static TextLayer *s_detail_title, *s_detail_body;
static MenuLayer *s_menu;
static CalendarSnapshot s_snapshot, s_staging;
static bool s_has_snapshot, s_cached = true, s_loading, s_receiving, s_confirm_clear, s_auto_refresh = true;
static bool s_cache_saved = true;
static bool s_cache_cleared;
#ifdef CALENDAR_DEMO_BUILD
static const bool s_demo_build = true;
#else
static const bool s_demo_build = false;
#endif
static uint32_t s_token, s_generation;
static uint16_t s_received;
static int s_selected, s_day_count, s_indices[EVENT_LIMIT], s_request_attempts;
static time_t s_day;
static AppTimer *s_timeout, *s_retry;
static char s_status[48] = "Connecting to phone...";
static char s_detail_text[700], s_detail_heading[256];

static void request_refresh(void);
static void refresh_view(void);

static void draw_text(GContext *ctx, const char *text, GRect rect, const char *font, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font), rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void format_time(int32_t epoch, char *result, size_t size) {
  time_t instant = epoch;
  struct tm value = *localtime(&instant);
  strftime(result, size, clock_is_24h_style() ? "%H:%M" : "%I:%M%p", &value);
  if (!clock_is_24h_style() && result[0] == '0') memmove(result, result + 1, strlen(result));
}

static void event_time(const CalendarEvent *event, char *result, size_t size) {
  if (event->flags & FLAG_ALL_DAY) { snprintf(result, size, "All day"); return; }
  char start[14], end[14];
  format_time(event->start, start, sizeof(start)); format_time(event->end, end, sizeof(end));
  snprintf(result, size, "%s - %s%s", start, end,
    calendar_day_code(event->start, false) != calendar_day_code(event->end, false) ? " +day" : "");
}

static void refresh_view(void) {
  s_day_count = s_has_snapshot ? calendar_day_events(&s_snapshot, s_day, s_indices) : 0;
  if (s_selected >= s_day_count) s_selected = s_day_count ? s_day_count - 1 : 0;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void draw_agenda(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int width = bounds.size.w, height = bounds.size.h;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(0, 0, width, 48), 0, GCornerNone);
  char date[40], heading[32];
  struct tm date_tm = *localtime(&s_day);
  strftime(date, sizeof(date), "%a, %b %e", &date_tm);
  bool today = calendar_day_code(s_day, false) == calendar_day_code(time(NULL), false);
  snprintf(heading, sizeof(heading), "%s%s", s_demo_build ? "DEMO / " : "", today ? "TODAY" : "AGENDA");
  draw_text(ctx, heading, GRect(10, 2, width - 20, 18), FONT_KEY_GOTHIC_14_BOLD, GColorLightGray);
  draw_text(ctx, date, GRect(10, 17, width - 20, 29), FONT_KEY_GOTHIC_24_BOLD, GColorWhite);

  int bottom = height - 36;
  if (!s_day_count) {
    const char *title = s_has_snapshot ? "No upcoming events" : s_cache_cleared ? "No saved events" : "Calendar unavailable";
    const char *body = s_has_snapshot ? "None in the received list.\nThis is not a full-day agenda." : s_status;
    if (s_loading && !s_has_snapshot) { title = "Getting your agenda"; body = "Keep the phone connected.\nCached events appear here."; }
    draw_text(ctx, title, GRect(10, 65, width - 20, 55), FONT_KEY_GOTHIC_24_BOLD, GColorBlack);
    draw_text(ctx, body, GRect(10, 123, width - 20, bottom - 123), FONT_KEY_GOTHIC_18, GColorDarkGray);
  } else {
    int next_event = -1;
    for (int i = 0; i < s_day_count; ++i) {
      CalendarEvent *event = &s_snapshot.events[s_indices[i]];
      if (!(event->flags & FLAG_ALL_DAY) && event->start > time(NULL)) { next_event = i; break; }
    }
    int rows = 3, row_height = (bottom - 52) / rows;
    int first = (s_selected / rows) * rows;
    for (int row = 0; row < rows && first + row < s_day_count; ++row) {
      int index = first + row, y = 52 + row * row_height;
      CalendarEvent *event = &s_snapshot.events[s_indices[index]];
      bool selected = index == s_selected;
      time_t now = time(NULL);
      bool ongoing = !(event->flags & FLAG_ALL_DAY) && event->start <= now && now < event->end;
      if (selected) {
        graphics_context_set_fill_color(ctx, GColorCobaltBlue);
        graphics_fill_rect(ctx, GRect(4, y, width - 8, row_height - 2), 4, GCornersAll);
      }
      char times[48]; event_time(event, times, sizeof(times));
      if (ongoing) {
        graphics_context_set_fill_color(ctx, selected ? GColorWhite : GColorCobaltBlue);
        graphics_fill_circle(ctx, GPoint(width - 14, y + 11), 3);
      } else if (index == next_event) {
        graphics_context_set_stroke_color(ctx, selected ? GColorWhite : GColorCobaltBlue);
        graphics_draw_circle(ctx, GPoint(width - 14, y + 11), 3);
      }
      draw_text(ctx, times, GRect(10, y + 1, width - 29, 18), FONT_KEY_GOTHIC_14_BOLD, selected ? GColorWhite : GColorDarkGray);
      draw_text(ctx, event->title, GRect(10, y + 16, width - 20, row_height - 18), FONT_KEY_GOTHIC_18_BOLD, selected ? GColorWhite : GColorBlack);
      if (!selected) {
        graphics_context_set_stroke_color(ctx, GColorLightGray);
        graphics_draw_line(ctx, GPoint(10, y + row_height - 2), GPoint(width - 10, y + row_height - 2));
      }
    }
  }
  char footer[64];
  if (s_loading) snprintf(footer, sizeof(footer), "Refreshing...");
  else if (!s_has_snapshot) snprintf(footer, sizeof(footer), "Double Select: options");
  else if (strcmp(s_status, "") != 0) snprintf(footer, sizeof(footer), "%s", s_status);
  else {
    int age = (int)(time(NULL) - s_snapshot.updated) / 60;
    if (age < 0) age = 0;
    snprintf(footer, sizeof(footer), "%s%d min ago%s", s_cached ? "Cached / " : "Updated ", age, s_cache_saved ? "" : " / RAM only");
  }
  draw_text(ctx, footer, GRect(8, height - 36, width - 16, 18), FONT_KEY_GOTHIC_14, GColorDarkGray);
  snprintf(footer, sizeof(footer), "Upcoming list%s%s", s_has_snapshot && s_snapshot.count == EVENT_LIMIT ? " / 10-event limit" : " / 7 days", s_demo_build ? " / DEMO" : "");
  draw_text(ctx, footer, GRect(8, height - 19, width - 16, 17), FONT_KEY_GOTHIC_14_BOLD, GColorCobaltBlue);
}

static void set_failure(const char *status) {
  s_loading = false; s_receiving = false; s_cached = true;
  memset(&s_staging, 0, sizeof(s_staging));
  snprintf(s_status, sizeof(s_status), "%s", status);
  refresh_view();
}

static void timeout_callback(void *context) {
  s_timeout = NULL;
  set_failure(s_has_snapshot ? "Offline / showing cached list" : "No response. Double Select to retry.");
}

static void reset_timeout(void) {
  if (s_timeout) app_timer_cancel(s_timeout);
  s_timeout = app_timer_register(20000, timeout_callback, NULL);
}

static void retry_callback(void *context) { s_retry = NULL; request_refresh(); }

static void send_control(int type) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_TYPE, type);
  dict_write_uint32(out, MESSAGE_KEY_TOKEN, s_token);
  app_message_outbox_send();
}

static void request_refresh(void) {
  s_auto_refresh = true; s_cache_cleared = false; s_loading = true; s_receiving = false; ++s_token;
  s_status[0] = 0;
  send_control(MSG_REQUEST); reset_timeout(); refresh_view();
}

static bool number(DictionaryIterator *iterator, uint32_t key, int32_t *result) {
  Tuple *tuple = dict_find(iterator, key);
  if (!tuple || (tuple->type != TUPLE_INT && tuple->type != TUPLE_UINT) || tuple->length != 4) return false;
  *result = tuple->value->int32; return true;
}

static bool string_value(DictionaryIterator *iterator, uint32_t key, char *result, size_t capacity) {
  Tuple *tuple = dict_find(iterator, key);
  if (!tuple || tuple->type != TUPLE_CSTRING || !tuple->length || tuple->length > capacity) return false;
  const char *bytes = (const char *)tuple->value;
  if (bytes[tuple->length - 1] != 0) return false;
  memcpy(result, bytes, tuple->length); return true;
}

static void inbox(DictionaryIterator *iterator, void *context) {
  int32_t type, token, generation;
  if (!number(iterator, MESSAGE_KEY_TYPE, &type)) return;
  if (type == MSG_READY) { if (s_auto_refresh) request_refresh(); return; }
  if (!number(iterator, MESSAGE_KEY_TOKEN, &token) || (uint32_t)token != s_token) return;
  if (type == MSG_ERROR) {
    char code[32];
    if (!string_value(iterator, MESSAGE_KEY_ERROR, code, sizeof(code))) return;
    if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; }
    if (strcmp(code, "PERMISSION_DENIED") == 0) {
      calendar_store_clear(); memset(&s_snapshot, 0, sizeof(s_snapshot)); s_has_snapshot = false;
      if (s_details_window && window_stack_contains_window(s_details_window)) window_stack_remove(s_details_window, false);
      set_failure("Allow Calendar access in Core.");
    } else if (strcmp(code, "PLUGIN_UNAVAILABLE") == 0) set_failure("Enable plugins in Pebble Core.");
    else if (strcmp(code, "INVALID_DATA") == 0) set_failure("Calendar data could not be read.");
    else set_failure(s_has_snapshot ? "Offline / showing cached list" : "Sync failed. Double Select to retry.");
    return;
  }
  if (!number(iterator, MESSAGE_KEY_GENERATION, &generation) || generation <= 0) return;
  if (type == MSG_BEGIN) {
    int32_t count, updated, version, demo;
    if (!number(iterator, MESSAGE_KEY_COUNT, &count) || count < 0 || count > EVENT_LIMIT ||
        !number(iterator, MESSAGE_KEY_UPDATED, &updated) || updated <= 0 ||
        !number(iterator, MESSAGE_KEY_VERSION, &version) || version != 1 ||
        !number(iterator, MESSAGE_KEY_DEMO, &demo) || (demo != 0 && demo != 1) || (bool)demo != s_demo_build) { set_failure("Sync data invalid. Retry."); return; }
    memset(&s_staging, 0, sizeof(s_staging));
    s_staging.version = version; s_staging.count = count; s_staging.updated = updated; s_staging.demo = demo;
    s_generation = generation; s_received = 0; s_receiving = true; s_loading = true; reset_timeout();
  } else if (s_receiving && (uint32_t)generation == s_generation && type == MSG_EVENT) {
    int32_t index, flags;
    if (!number(iterator, MESSAGE_KEY_INDEX, &index) || index < 0 || index >= s_staging.count ||
        !number(iterator, MESSAGE_KEY_FLAGS, &flags) || flags < 0 || flags > 3) { set_failure("Sync data invalid. Retry."); return; }
    CalendarEvent *event = &s_staging.events[index];
    if (!number(iterator, MESSAGE_KEY_START, &event->start) || !number(iterator, MESSAGE_KEY_END, &event->end) ||
        !string_value(iterator, MESSAGE_KEY_TITLE, event->title, sizeof(event->title)) ||
        !string_value(iterator, MESSAGE_KEY_LOCATION, event->location, sizeof(event->location)) ||
        !string_value(iterator, MESSAGE_KEY_CALENDAR, event->calendar, sizeof(event->calendar))) { set_failure("Sync data invalid. Retry."); return; }
    event->flags = flags;
    if (!calendar_event_valid(event)) { set_failure("Sync data invalid. Retry."); return; }
    s_received |= 1 << index; reset_timeout();
  } else if (s_receiving && (uint32_t)generation == s_generation && type == MSG_COMMIT) {
    if (s_received != (uint16_t)((1 << s_staging.count) - 1)) { set_failure("Sync interrupted. Retry."); return; }
    if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; }
    if (s_details_window && window_stack_contains_window(s_details_window)) window_stack_remove(s_details_window, false);
    s_snapshot = s_staging; memset(&s_staging, 0, sizeof(s_staging));
    s_has_snapshot = true; s_cache_cleared = s_loading = s_receiving = s_cached = false;
    s_cache_saved = calendar_store_save(&s_snapshot); s_request_attempts = 0;
    APP_LOG(APP_LOG_LEVEL_INFO, "Calendar snapshot: %u events; saved=%u", s_snapshot.count, s_cache_saved);
    s_status[0] = 0; refresh_view();
  }
}

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  if (s_auto_refresh && ++s_request_attempts <= 2 && !s_retry) s_retry = app_timer_register(1500, retry_callback, NULL);
}

static void inbox_dropped(AppMessageResult reason, void *context) { set_failure("Sync interrupted. Double Select."); }

static void connection_changed(bool connected) {
  if (connected && s_auto_refresh) { s_request_attempts = 0; request_refresh(); }
  else if (!connected) set_failure(s_has_snapshot ? "Offline / showing cached list" : "Connect your phone to refresh.");
}

static void detail_back(ClickRecognizerRef recognizer, void *context) { window_stack_pop(true); }
static void detail_up(ClickRecognizerRef recognizer, void *context) {
  GPoint offset = scroll_layer_get_content_offset(s_scroll); offset.y += 42; scroll_layer_set_content_offset(s_scroll, offset, false);
}
static void detail_down(ClickRecognizerRef recognizer, void *context) {
  GPoint offset = scroll_layer_get_content_offset(s_scroll); offset.y -= 42; scroll_layer_set_content_offset(s_scroll, offset, false);
}
static void detail_click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, detail_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, detail_down);
  window_single_click_subscribe(BUTTON_ID_BACK, detail_back);
}

static void detail_load(Window *window) {
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  int width = bounds.size.w - 16;
  s_scroll = scroll_layer_create(bounds);
  s_detail_title = text_layer_create(GRect(8, 4, width, 1000));
  text_layer_set_font(s_detail_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_detail_title, s_detail_heading);
  GSize title_size = text_layer_get_content_size(s_detail_title);
  layer_set_frame(text_layer_get_layer(s_detail_title), GRect(8, 4, width, title_size.h + 8));
  s_detail_body = text_layer_create(GRect(8, title_size.h + 14, width, 2000));
  text_layer_set_font(s_detail_body, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(s_detail_body, s_detail_text);
  GSize body_size = text_layer_get_content_size(s_detail_body);
  layer_set_frame(text_layer_get_layer(s_detail_body), GRect(8, title_size.h + 14, width, body_size.h + 8));
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_detail_title));
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_detail_body));
  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, title_size.h + body_size.h + 35));
  layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(s_scroll));
}
static void detail_unload(Window *window) {
  text_layer_destroy(s_detail_title); text_layer_destroy(s_detail_body); scroll_layer_destroy(s_scroll);
  s_detail_title = s_detail_body = NULL; s_scroll = NULL;
  memset(s_detail_heading, 0, sizeof(s_detail_heading)); memset(s_detail_text, 0, sizeof(s_detail_text));
}
static void open_details(void) {
  if (!s_day_count) return;
  CalendarEvent *event = &s_snapshot.events[s_indices[s_selected]];
  snprintf(s_detail_heading, sizeof(s_detail_heading), "%s", event->title);
  char start[50], end[50], clock[14], date[32];
  time_t instant = event->start;
  struct tm value = *(event->flags & FLAG_ALL_DAY ? gmtime(&instant) : localtime(&instant));
  strftime(date, sizeof(date), "%a, %b %e", &value);
  format_time(event->start, clock, sizeof(clock));
  snprintf(start, sizeof(start), "%s / %s", date, event->flags & FLAG_ALL_DAY ? "All day" : clock);
  instant = event->end - ((event->flags & FLAG_ALL_DAY) ? 1 : 0);
  value = *(event->flags & FLAG_ALL_DAY ? gmtime(&instant) : localtime(&instant));
  strftime(date, sizeof(date), "%a, %b %e", &value);
  format_time(event->end, clock, sizeof(clock));
  if (event->flags & FLAG_ALL_DAY) snprintf(end, sizeof(end), "%s", date);
  else snprintf(end, sizeof(end), "%s / %s", date, clock);
  snprintf(s_detail_text, sizeof(s_detail_text), "%s\nUntil %s\n\n%s\n%s\n\nCalendar\n%s\n\n%s%s",
    start, end, event->location[0] ? "Location" : "Location not provided", event->location,
    event->calendar[0] ? event->calendar : "Not provided", s_cached ? "Cached event\n" : "",
    event->flags & FLAG_TRUNCATED ? "Long metadata was shortened.\n" : "");
  window_stack_push(s_details_window, true);
}

static void go_today(void) { s_day = calendar_midnight(time(NULL)); s_selected = 0; refresh_view(); }
static uint16_t menu_count(MenuLayer *menu, uint16_t section, void *context) { return s_confirm_clear ? 2 : 3; }
static void menu_draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  const char *title, *subtitle;
  if (s_confirm_clear) { title = index->row ? "Cancel" : "Clear now"; subtitle = index->row ? "Keep cached events" : "Remove events from this app"; }
  else if (index->row == 0) { title = "Today"; subtitle = "Return to today's agenda"; }
  else if (index->row == 1) { title = "Refresh"; subtitle = "Ask the phone for events"; }
  else { title = "Clear cache"; subtitle = "Remove saved calendar data"; }
  menu_cell_basic_draw(ctx, cell, title, subtitle, NULL);
}
static void menu_select(MenuLayer *menu, MenuIndex *index, void *context) {
  if (s_confirm_clear) {
    if (index->row == 0) {
      s_auto_refresh = false; ++s_token;
      if (s_retry) { app_timer_cancel(s_retry); s_retry = NULL; }
      if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; }
      send_control(MSG_CLEAR); calendar_store_clear();
      memset(&s_snapshot, 0, sizeof(s_snapshot)); memset(&s_staging, 0, sizeof(s_staging));
      s_cache_cleared = true;
      s_has_snapshot = s_loading = s_receiving = false; snprintf(s_status, sizeof(s_status), "Cache cleared. Refresh to reload.");
    }
    window_stack_pop(true); refresh_view();
  } else if (index->row == 0) { window_stack_pop(true); go_today(); }
  else if (index->row == 1) { window_stack_pop(true); s_request_attempts = 0; request_refresh(); }
  else { s_confirm_clear = true; menu_layer_reload_data(menu); MenuIndex first = { 0, 0 }; menu_layer_set_selected_index(menu, first, MenuRowAlignTop, false); }
}
static void actions_load(Window *window) {
  s_menu = menu_layer_create(layer_get_bounds(window_get_root_layer(window)));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){ .get_num_rows = menu_count, .draw_row = menu_draw, .select_click = menu_select });
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(window_get_root_layer(window), menu_layer_get_layer(s_menu));
}
static void actions_unload(Window *window) { menu_layer_destroy(s_menu); s_menu = NULL; }

static void up(ClickRecognizerRef recognizer, void *context) { if (s_selected > 0) --s_selected; refresh_view(); }
static void down(ClickRecognizerRef recognizer, void *context) { if (s_selected + 1 < s_day_count) ++s_selected; refresh_view(); }
static void select_event(ClickRecognizerRef recognizer, void *context) { open_details(); }
static void prev_day(ClickRecognizerRef recognizer, void *context) {
  if (calendar_day_code(s_day, false) > calendar_day_code(calendar_midnight(time(NULL)), false)) s_day = calendar_shift_day(s_day, -1);
  s_selected = 0; refresh_view();
}
static void next_day(ClickRecognizerRef recognizer, void *context) {
  if (calendar_day_code(s_day, false) < calendar_day_code(calendar_shift_day(calendar_midnight(time(NULL)), 7), false)) s_day = calendar_shift_day(s_day, 1);
  s_selected = 0; refresh_view();
}
static void today(ClickRecognizerRef recognizer, void *context) { go_today(); }
static void options(ClickRecognizerRef recognizer, void *context) { s_confirm_clear = false; window_stack_push(s_actions_window, true); }
static void agenda_back(ClickRecognizerRef recognizer, void *context) { window_stack_pop(true); }
static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up);
  window_single_click_subscribe(BUTTON_ID_DOWN, down);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_event);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 2, 300, true, options);
  window_single_click_subscribe(BUTTON_ID_BACK, agenda_back);
  window_long_click_subscribe(BUTTON_ID_UP, 650, prev_day, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 650, next_day, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 650, today, NULL);
}
static void tick(struct tm *time_value, TimeUnits units) {
  if (s_has_snapshot && calendar_expired(&s_snapshot, time(NULL))) {
    calendar_store_clear(); memset(&s_snapshot, 0, sizeof(s_snapshot)); s_has_snapshot = false;
    if (window_stack_contains_window(s_details_window)) window_stack_remove(s_details_window, false);
    snprintf(s_status, sizeof(s_status), "Saved list expired. Refresh.");
  }
  refresh_view();
}
static void load(Window *window) {
  s_canvas = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_canvas, draw_agenda); layer_add_child(window_get_root_layer(window), s_canvas);
}
static void unload(Window *window) { layer_destroy(s_canvas); s_canvas = NULL; }

static void init(void) {
  s_day = calendar_midnight(time(NULL));
  s_has_snapshot = calendar_store_load(&s_snapshot, time(NULL));
  if (s_has_snapshot && s_snapshot.demo != s_demo_build) { calendar_store_clear(); memset(&s_snapshot, 0, sizeof(s_snapshot)); s_has_snapshot = false; }
  if (s_has_snapshot) s_status[0] = 0;
  s_window = window_create(); s_details_window = window_create(); s_actions_window = window_create();
  window_set_background_color(s_window, GColorWhite); window_set_background_color(s_details_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = load, .unload = unload });
  window_set_window_handlers(s_details_window, (WindowHandlers){ .load = detail_load, .unload = detail_unload });
  window_set_window_handlers(s_actions_window, (WindowHandlers){ .load = actions_load, .unload = actions_unload });
  window_set_click_config_provider(s_window, click_config); window_set_click_config_provider(s_details_window, detail_click_config);
  window_stack_push(s_window, false);
  app_message_register_inbox_received(inbox); app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed); app_message_open(2048, 128);
  connection_service_subscribe((ConnectionHandlers){ .pebble_app_connection_handler = connection_changed });
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  request_refresh();
}

static void deinit(void) {
  if (s_timeout) app_timer_cancel(s_timeout);
  if (s_retry) app_timer_cancel(s_retry);
  tick_timer_service_unsubscribe(); connection_service_unsubscribe(); app_message_deregister_callbacks();
  window_destroy(s_actions_window); window_destroy(s_details_window); window_destroy(s_window);
  memset(&s_snapshot, 0, sizeof(s_snapshot)); memset(&s_staging, 0, sizeof(s_staging));
}

int main(void) { init(); app_event_loop(); deinit(); }
