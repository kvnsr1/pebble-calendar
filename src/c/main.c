#include <pebble.h>
#include "calendar.h"
#include "event_detail.h"
#include "ui_theme.h"

enum { MSG_REQUEST = 1, MSG_BEGIN, MSG_EVENT, MSG_COMMIT, MSG_ERROR, MSG_CLEAR, MSG_READY };
static Window *s_window, *s_agenda_window, *s_calendar_window, *s_clear_window;
static StatusBarLayer *s_status_bar, *s_agenda_status_bar, *s_calendar_status_bar;
static Layer *s_agenda_canvas, *s_calendar_canvas, *s_clear_canvas;
static ActionBarLayer *s_clear_bar;
static MenuLayer *s_agenda_menu;
static ActionMenu *s_actions;
static ActionMenuLevel *s_actions_root;
static int s_pending_action;
static Layer *s_canvas;
static CalendarSnapshot s_snapshot, s_staging;
static bool s_has_snapshot, s_cached = true, s_loading, s_receiving, s_auto_refresh = true;
static bool s_cache_saved = true;
static bool s_cache_cleared;
static bool s_month_view;
#ifdef CALENDAR_DEMO_BUILD
static const bool s_demo_build = true;
#else
static const bool s_demo_build = false;
#endif
static uint32_t s_token, s_generation;
static uint16_t s_received;
static int s_selected, s_day_count, s_indices[EVENT_LIMIT], s_request_attempts;
static time_t s_day, s_calendar_day;
static AppTimer *s_timeout, *s_retry;
static char s_status[48] = "Connecting to phone...";

static void request_refresh(void);
static void refresh_view(void);
static void open_options(void);
static void layout_views(void);

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

static int header_height(void) { return ui_large_text() ? 68 : 60; }
static int footer_height(void) { return ui_large_text() ? 60 : 48; }

static void refresh_view(void) {
  s_day_count = s_has_snapshot ? calendar_day_events(&s_snapshot, s_day, s_indices) : 0;
  if (s_selected >= s_day_count) s_selected = s_day_count - 1;
  if (s_agenda_menu) {
    layout_views();
    menu_layer_reload_data(s_agenda_menu);
    MenuIndex chosen = {0, s_day_count && s_selected >= 0 ? s_selected + 1 : 0};
    menu_layer_set_selected_index(s_agenda_menu, chosen, !chosen.row ? MenuRowAlignTop : MenuRowAlignCenter, false);
  }
  if (s_canvas) layer_mark_dirty(s_canvas);
  if (s_agenda_canvas) layer_mark_dirty(s_agenda_canvas);
  if (s_calendar_canvas) layer_mark_dirty(s_calendar_canvas);
}

static bool in_received_window(time_t day) {
  int code = calendar_day_code(day, false);
  return code >= calendar_day_code(time(NULL), false) &&
    code <= calendar_day_code(calendar_shift_day(calendar_midnight(time(NULL)), 7), false);
}

static void draw_calendar(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int width = bounds.size.w, height = bounds.size.h;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  char month[32]; struct tm selected = *localtime(&s_calendar_day);
  strftime(month, sizeof(month), "%B %Y", &selected);
  ui_text(ctx, month, FONT_KEY_GOTHIC_28_BOLD, GColorBlack, GRect(8, -4, width - 16, 36),
          GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);
  const char *week[] = { "M", "T", "W", "T", "F", "S", "S" };
  int cell_width = (width - 12) / 7, cell_height = (height - 92) / 6;
  time_t cells[42]; calendar_month_grid(s_calendar_day, cells);
  int today = calendar_day_code(time(NULL), false), chosen = calendar_day_code(s_calendar_day, false);
  for (int column = 0; column < 7; ++column)
    ui_text(ctx, week[column], FONT_KEY_GOTHIC_18_BOLD, GColorDarkGray,
            GRect(6 + column * cell_width, 30, cell_width, 23), GTextAlignmentCenter, GTextOverflowModeWordWrap);
  for (int i = 0; i < 42; ++i) {
    struct tm day = *localtime(&cells[i]);
    int code = calendar_day_code(cells[i], false), x = 6 + (i % 7) * cell_width, y = 56 + (i / 7) * cell_height;
    bool current_month = day.tm_mon == selected.tm_mon && day.tm_year == selected.tm_year;
    bool highlighted = code == chosen;
    if (highlighted) {
      graphics_context_set_fill_color(ctx, GColorCobaltBlue);
      graphics_fill_rect(ctx, GRect(x, y, cell_width - 2, cell_height - 1), 2, GCornersAll);
    } else if (code == today) {
      graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
      graphics_draw_round_rect(ctx, GRect(x, y, cell_width - 2, cell_height - 1), 2);
    }
    char number[4]; snprintf(number, sizeof(number), "%d", day.tm_mday);
    ui_text(ctx, number, FONT_KEY_GOTHIC_18_BOLD, highlighted ? GColorWhite : current_month ? GColorBlack : GColorDarkGray,
            GRect(x, y - 5, cell_width - 2, 24), GTextAlignmentCenter, GTextOverflowModeWordWrap);
    int indices[EVENT_LIMIT];
    if (s_has_snapshot && calendar_day_events(&s_snapshot, cells[i], indices)) {
      graphics_context_set_fill_color(ctx, highlighted ? GColorWhite : GColorCobaltBlue);
      graphics_fill_circle(ctx, GPoint(x + (cell_width - 2) / 2, y + cell_height - 3), 1);
    }
  }
  int indices[EVENT_LIMIT];
  int count = s_has_snapshot ? calendar_day_events(&s_snapshot, s_calendar_day, indices) : 0;
  char status[48];
  if (!s_has_snapshot) snprintf(status, sizeof(status), "No received schedule");
  else if (!in_received_window(s_calendar_day)) snprintf(status, sizeof(status), "Outside upcoming list");
  else snprintf(status, sizeof(status), "%d received / %s", count, s_cached ? "cached" : "upcoming");
  if (s_demo_build) snprintf(status, sizeof(status), "DEMO / received events only");
  draw_text(ctx, status, GRect(8, height - 40, width - 16, 22), FONT_KEY_GOTHIC_18, GColorDarkGray);
  draw_text(ctx, "Select: use this day", GRect(8, height - 21, width - 16, 24), FONT_KEY_GOTHIC_18_BOLD, GColorCobaltBlue);
}

static void draw_day_header(GContext *ctx, int width) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, 0, width, header_height()), 0, GCornerNone);
  char heading[40], date[40]; struct tm date_tm = *localtime(&s_day);
  bool today = calendar_day_code(s_day, false) == calendar_day_code(time(NULL), false);
  if (today) snprintf(heading, sizeof(heading), "%s", s_demo_build ? "Today / Demo" : "Today");
  else strftime(heading, sizeof(heading), "%A", &date_tm);
  strftime(date, sizeof(date), "%a, %b %e", &date_tm);
  draw_text(ctx, heading, GRect(8, 0, width - 20, 34), FONT_KEY_GOTHIC_28_BOLD, GColorWhite);
  draw_text(ctx, date, GRect(8, 32, width - 20, ui_line_height()), ui_body_font(), GColorWhite);
}

static void draw_main(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int width = bounds.size.w, height = bounds.size.h, top = header_height();
  int footer_top = height - footer_height(), line = ui_line_height();
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  draw_day_header(ctx, width);
  if (s_day_count) {
    char count[40]; snprintf(count, sizeof(count), "%d received event%s", s_day_count, s_day_count == 1 ? "" : "s");
    draw_text(ctx, count, GRect(8, top + 3, width - 20, line), ui_body_bold_font(), GColorCobaltBlue);
    // Prefer the current/next timed event over an all-day item in the preview.
    int preview = 0;
    for (int i = 0; i < s_day_count; ++i) {
      CalendarEvent *event = &s_snapshot.events[s_indices[i]];
      if (!(event->flags & FLAG_ALL_DAY) && event->end > time(NULL)) { preview = i; break; }
    }
    CalendarEvent *event = &s_snapshot.events[s_indices[preview]];
    int available = footer_top - top - line - 10;
    int title_height = available - (ui_large_text() ? 0 : line + 3);
    draw_text(ctx, event->title, GRect(8, top + line + 6, width - 20, title_height), ui_event_font(), GColorBlack);
    if (!ui_large_text()) {
      char times[48]; event_time(event, times, sizeof(times));
      draw_text(ctx, times, GRect(8, top + line + 9 + title_height, width - 20, line), ui_body_font(), GColorDarkGray);
    }
  } else {
    const char *title = s_has_snapshot ? (in_received_window(s_day) ? "No events received" : "No schedule data") :
      s_cache_cleared ? "Saved list cleared" : "Calendar unavailable";
    const char *body = s_has_snapshot ? (in_received_window(s_day) ? "Other events may exist." : "Outside upcoming list.") :
      s_cache_cleared ? "Select to refresh." : s_status[0] ? s_status : "Select for options.";
    if (ui_large_text()) {
      title = s_has_snapshot ? (in_received_window(s_day) ? "None received" : "No schedule") : s_cache_cleared ? "List cleared" : "No calendar";
      body = s_has_snapshot ? (in_received_window(s_day) ? "Upcoming list" : "Outside list") : strstr(s_status, "Enable plugins") ? "Enable Core plugins" :
        strstr(s_status, "Allow Calendar") ? "Allow Calendar" : "Select for options";
    }
    if (s_loading && !s_has_snapshot) { title = "Loading agenda"; body = "Keep Core connected."; }
    int title_height = graphics_text_layout_get_content_size(title, fonts_get_system_font(ui_event_font()),
      GRect(0, 0, width - 20, 1000), GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
    ui_text(ctx, title, ui_event_font(), GColorBlack, GRect(8, top + 4, width - 20, title_height + 4), GTextAlignmentLeft, GTextOverflowModeWordWrap);
    draw_text(ctx, body, GRect(8, top + title_height + 10, width - 20, footer_top - top - title_height - 12), ui_body_font(), GColorBlack);
  }
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(8, footer_top), GPoint(width - 8, footer_top));
  draw_text(ctx, s_day_count ? "Select: day agenda" : "Select: options", GRect(8, footer_top + 2, width - 20, line), ui_body_bold_font(), GColorCobaltBlue);
  char status[48];
  if (s_loading) snprintf(status, sizeof(status), "Refreshing...");
  else if (s_status[0]) snprintf(status, sizeof(status), "%s", s_has_snapshot ? "Offline / saved list" : s_cache_cleared ? "Saved list cleared" : "Phone sync unavailable");
  else if (s_has_snapshot) {
    int age = (int)(time(NULL) - s_snapshot.updated) / 60; if (age < 0) age = 0;
    snprintf(status, sizeof(status), "%s %dm%s", s_cached ? "Saved" : "Updated", age, s_cache_saved ? "" : " / RAM");
  } else snprintf(status, sizeof(status), "No saved schedule");
  draw_text(ctx, status, GRect(8, footer_top + line + 2, width - 20, line), ui_body_font(), GColorDarkGray);
}

static void draw_agenda(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  draw_day_header(ctx, bounds.size.w);
  draw_text(ctx, s_demo_build ? "Demo / received events" : "Upcoming: max 10 / 7 days",
    GRect(8, bounds.size.h - ui_line_height() - 2, bounds.size.w - 16, ui_line_height()), ui_body_font(), GColorDarkGray);
}

static uint16_t agenda_rows(MenuLayer *menu, uint16_t section, void *context) { return s_day_count + 1; }
static int16_t agenda_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return index->row ? 96 : (ui_large_text() ? 40 : 32);
}
static void agenda_draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  GRect bounds = layer_get_bounds(cell);
  bool selected = menu_cell_layer_is_highlighted(cell);
  GColor ink = selected ? GColorWhite : GColorBlack;
  if (!index->row) {
    draw_text(ctx, "Calendar & options", GRect(8, 0, bounds.size.w - 16, bounds.size.h), ui_body_bold_font(), ink);
    return;
  }
  CalendarEvent *event = &s_snapshot.events[s_indices[index->row - 1]];
  char times[48], source[84]; event_time(event, times, sizeof(times));
  time_t now = time(NULL);
  bool ongoing = !(event->flags & FLAG_ALL_DAY) && event->start <= now && now < event->end;
  bool next = false;
  if (calendar_day_code(s_day, false) == calendar_day_code(now, false)) {
    for (int i = 0; i < s_day_count; ++i) {
      CalendarEvent *candidate = &s_snapshot.events[s_indices[i]];
      if (!(candidate->flags & FLAG_ALL_DAY) && candidate->start > now) { next = candidate == event; break; }
    }
  }
  snprintf(source, sizeof(source), "%s%s", ongoing ? "NOW / " : next ? "NEXT / " : "", event->calendar[0] ? event->calendar : "Calendar");
  draw_text(ctx, times, GRect(8, 0, bounds.size.w - 16, ui_line_height()), ui_body_bold_font(), selected ? GColorWhite : GColorCobaltBlue);
  draw_text(ctx, event->title, GRect(8, ui_line_height(), bounds.size.w - 16, ui_large_text() ? 34 : 50), ui_event_font(), ink);
  draw_text(ctx, source, GRect(8, bounds.size.h - ui_line_height() - 2, bounds.size.w - 16, ui_line_height()), ui_body_font(), selected ? GColorWhite : GColorDarkGray);
}
static void agenda_selection(MenuLayer *menu, MenuIndex index, MenuIndex old, void *context) { s_selected = (int)index.row - 1; }
static void agenda_select(MenuLayer *menu, MenuIndex *index, void *context) {
  if (!index->row) open_options();
  else event_detail_open(&s_snapshot.events[s_indices[index->row - 1]], s_cached);
}
static void agenda_options(MenuLayer *menu, MenuIndex *index, void *context) { open_options(); }

static void set_failure(const char *status) {
  s_loading = false; s_receiving = false; s_cached = true;
  memset(&s_staging, 0, sizeof(s_staging));
  snprintf(s_status, sizeof(s_status), "%s", status);
  refresh_view();
}

static void timeout_callback(void *context) {
  s_timeout = NULL;
  set_failure(s_has_snapshot ? "Offline / showing cached list" : "No response. Select options to retry.");
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
      event_detail_close();
      set_failure("Allow Calendar access in Core.");
    } else if (strcmp(code, "PLUGIN_UNAVAILABLE") == 0) set_failure("Enable plugins in Pebble Core.");
    else if (strcmp(code, "INVALID_DATA") == 0) set_failure("Calendar data could not be read.");
    else set_failure(s_has_snapshot ? "Offline / showing cached list" : "Sync failed. Select options to retry.");
    return;
  }
  if (!number(iterator, MESSAGE_KEY_GENERATION, &generation) || generation <= 0) return;
  if (type == MSG_BEGIN) {
    int32_t count, updated, version, demo;
    if (!number(iterator, MESSAGE_KEY_COUNT, &count) || count < 0 || count > EVENT_LIMIT ||
        !number(iterator, MESSAGE_KEY_UPDATED, &updated) || updated <= 0 ||
        !number(iterator, MESSAGE_KEY_VERSION, &version) || version != SNAPSHOT_VERSION ||
        !number(iterator, MESSAGE_KEY_DEMO, &demo) || (demo != 0 && demo != 1) || (bool)demo != s_demo_build) { set_failure("Sync data invalid. Retry."); return; }
    memset(&s_staging, 0, sizeof(s_staging));
    s_staging.version = version; s_staging.count = count; s_staging.updated = updated; s_staging.demo = demo;
    s_generation = generation; s_received = 0; s_receiving = true; s_loading = true; reset_timeout();
  } else if (s_receiving && (uint32_t)generation == s_generation && type == MSG_EVENT) {
    int32_t index, flags;
    if (!number(iterator, MESSAGE_KEY_INDEX, &index) || index < 0 || index >= s_staging.count ||
        !number(iterator, MESSAGE_KEY_FLAGS, &flags) || flags < 0 || flags > 7) { set_failure("Sync data invalid. Retry."); return; }
    CalendarEvent *event = &s_staging.events[index];
    if (!number(iterator, MESSAGE_KEY_START, &event->start) || !number(iterator, MESSAGE_KEY_END, &event->end) ||
        !string_value(iterator, MESSAGE_KEY_TITLE, event->title, sizeof(event->title)) ||
        !string_value(iterator, MESSAGE_KEY_LOCATION, event->location, sizeof(event->location)) ||
        !string_value(iterator, MESSAGE_KEY_CALENDAR, event->calendar, sizeof(event->calendar)) ||
        !string_value(iterator, MESSAGE_KEY_ATTENDEES, event->attendees, sizeof(event->attendees)) ||
        !string_value(iterator, MESSAGE_KEY_ROOM_NAME, event->room.name, sizeof(event->room.name)) ||
        !string_value(iterator, MESSAGE_KEY_OFFICE, event->room.office, sizeof(event->room.office)) ||
        !string_value(iterator, MESSAGE_KEY_BUILDING, event->room.building, sizeof(event->room.building)) ||
        !string_value(iterator, MESSAGE_KEY_ROOM_NUMBER, event->room.number, sizeof(event->room.number)) ||
        !string_value(iterator, MESSAGE_KEY_FACILITIES, event->room.facilities, sizeof(event->room.facilities)) ||
        !number(iterator, MESSAGE_KEY_FLOOR, &event->room.floor) ||
        !number(iterator, MESSAGE_KEY_CAPACITY, &event->room.capacity)) { set_failure("Sync data invalid. Retry."); return; }
    int32_t room_source;
    if (!number(iterator, MESSAGE_KEY_ROOM_SOURCE, &room_source) || room_source < 0 || room_source > 2) { set_failure("Sync data invalid. Retry."); return; }
    event->room.source = room_source;
    event->flags = flags;
    if (!calendar_event_valid(event)) { set_failure("Sync data invalid. Retry."); return; }
    s_received |= 1 << index; reset_timeout();
  } else if (s_receiving && (uint32_t)generation == s_generation && type == MSG_COMMIT) {
    if (s_received != (uint16_t)((1 << s_staging.count) - 1)) { set_failure("Sync interrupted. Retry."); return; }
    if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; }
    event_detail_close();
    if (!s_has_snapshot) s_selected = 0;
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

static void inbox_dropped(AppMessageResult reason, void *context) { set_failure("Sync interrupted. Select options."); }

static void connection_changed(bool connected) {
  if (connected && s_auto_refresh) { s_request_attempts = 0; request_refresh(); }
  else if (!connected) set_failure(s_has_snapshot ? "Offline / showing cached list" : "Connect your phone to refresh.");
}

enum { ACT_CALENDAR = 1, ACT_TODAY, ACT_PREV_DAY, ACT_NEXT_DAY, ACT_PREV_MONTH, ACT_NEXT_MONTH, ACT_REFRESH, ACT_CLEAR };

static void shift_date(int amount, bool month) {
  time_t *day = s_month_view ? &s_calendar_day : &s_day;
  time_t next = month ? calendar_shift_month(*day, amount) : calendar_shift_day(*day, amount);
  time_t first = calendar_month_start(calendar_shift_month(calendar_midnight(time(NULL)), -12));
  time_t end = calendar_month_start(calendar_shift_month(calendar_midnight(time(NULL)), 13));
  if (next >= first && next < end) *day = next;
  s_selected = 0; refresh_view();
}
static void close_agenda(void) {
  if (window_stack_contains_window(s_agenda_window)) window_stack_remove(s_agenda_window, false);
}
static void home_up(ClickRecognizerRef ref, void *context) { shift_date(-1, false); }
static void home_down(ClickRecognizerRef ref, void *context) { shift_date(1, false); }
static void home_select(ClickRecognizerRef ref, void *context) {
  if (s_day_count) { s_selected = 0; window_stack_push(s_agenda_window, true); refresh_view(); }
  else open_options();
}
static void home_options(ClickRecognizerRef ref, void *context) { open_options(); }
static void home_clicks(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, home_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, home_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, home_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 650, home_options, NULL);
}
static void open_calendar(void) {
  event_detail_close(); close_agenda();
  if (!window_stack_contains_window(s_calendar_window)) {
    s_calendar_day = s_day; s_month_view = true; window_stack_push(s_calendar_window, true);
  }
}
static void choose_day(void) {
  s_day = s_calendar_day; s_selected = 0; s_month_view = false; window_stack_pop(true); refresh_view();
}
static void calendar_up(ClickRecognizerRef ref, void *context) { shift_date(-1, false); }
static void calendar_down(ClickRecognizerRef ref, void *context) { shift_date(1, false); }
static void calendar_select(ClickRecognizerRef ref, void *context) { choose_day(); }
static void calendar_prev_month(ClickRecognizerRef ref, void *context) { shift_date(-1, true); }
static void calendar_next_month(ClickRecognizerRef ref, void *context) { shift_date(1, true); }
static void calendar_options(ClickRecognizerRef ref, void *context) { open_options(); }
static void calendar_clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, calendar_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, calendar_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, calendar_select);
  window_long_click_subscribe(BUTTON_ID_UP, 650, calendar_prev_month, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 650, calendar_next_month, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 650, calendar_options, NULL);
}
static void clear_confirm(ClickRecognizerRef ref, void *context) {
  s_auto_refresh = false; ++s_token;
  if (s_retry) { app_timer_cancel(s_retry); s_retry = NULL; }
  if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; }
  send_control(MSG_CLEAR); calendar_store_clear();
  memset(&s_snapshot, 0, sizeof(s_snapshot)); memset(&s_staging, 0, sizeof(s_staging));
  s_cache_cleared = true; s_has_snapshot = s_loading = s_receiving = false;
  snprintf(s_status, sizeof(s_status), "Saved list cleared.");
  window_stack_pop(true); event_detail_close(); refresh_view();
  close_agenda();
}
static void clear_clicks(void *context) { window_single_click_subscribe(BUTTON_ID_SELECT, clear_confirm); }
static void clear_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  ui_text(ctx, "Clear saved events?", FONT_KEY_GOTHIC_28_BOLD, GColorBlack,
          GRect(10, 24, bounds.size.w - 20, 82), GTextAlignmentLeft, GTextOverflowModeWordWrap);
  ui_text(ctx, "Clear this watch's saved copy.", ui_body_font(), GColorBlack,
          GRect(10, 110, bounds.size.w - 20, bounds.size.h - 145), GTextAlignmentLeft, GTextOverflowModeWordWrap);
  draw_text(ctx, "Back: cancel", GRect(10, bounds.size.h - 30, bounds.size.w - 20, 28), ui_body_bold_font(), GColorCobaltBlue);
}
static void clear_load(Window *window) {
  GRect bounds = layer_get_bounds(window_get_root_layer(window)); bounds.size.w -= ACTION_BAR_WIDTH;
  s_clear_canvas = layer_create(bounds); layer_set_update_proc(s_clear_canvas, clear_draw);
  layer_add_child(window_get_root_layer(window), s_clear_canvas);
  s_clear_bar = ui_confirmation_bar(window, clear_clicks);
}
static void clear_unload(Window *window) {
  action_bar_layer_remove_from_window(s_clear_bar); action_bar_layer_destroy(s_clear_bar);
  layer_destroy(s_clear_canvas); s_clear_bar = NULL; s_clear_canvas = NULL;
}
static void action_chosen(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  s_pending_action = (int)(intptr_t)action_menu_item_get_action_data(item);
}
static void actions_closed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  action_menu_hierarchy_destroy(s_actions_root, NULL, NULL); s_actions_root = NULL; s_actions = NULL;
  int action = s_pending_action; s_pending_action = 0;
  if (action == ACT_CALENDAR) { if (s_month_view) choose_day(); else open_calendar(); }
  else if (action == ACT_TODAY) {
    if (s_month_view) s_calendar_day = calendar_midnight(time(NULL));
    else { event_detail_close(); close_agenda(); s_day = calendar_midnight(time(NULL)); }
    s_selected = 0; refresh_view();
  } else if (action == ACT_PREV_DAY || action == ACT_NEXT_DAY) {
    event_detail_close(); close_agenda(); shift_date(action == ACT_PREV_DAY ? -1 : 1, false);
  } else if (action == ACT_PREV_MONTH || action == ACT_NEXT_MONTH) {
    open_calendar(); shift_date(action == ACT_PREV_MONTH ? -1 : 1, true);
  } else if (action == ACT_REFRESH) { s_request_attempts = 0; request_refresh(); }
  else if (action == ACT_CLEAR) window_stack_push(s_clear_window, true);
}
static void open_options(void) {
  if (s_actions) return;
  const char *labels[] = {s_month_view ? "Use selected day" : "Calendar", "Today", "Previous day", "Next day",
                         "Previous month", "Next month", "Refresh", "Clear saved events"};
  s_pending_action = 0; s_actions_root = action_menu_level_create(8);
  for (int i = 0; i < 8; ++i)
    action_menu_level_add_action(s_actions_root, labels[i], action_chosen, (void *)(intptr_t)(i + 1));
  ActionMenuConfig config = {.root_level = s_actions_root, .colors = {.background = GColorCobaltBlue, .foreground = GColorWhite},
                             .did_close = actions_closed, .align = ActionMenuAlignTop};
  s_actions = action_menu_open(&config);
}
static void calendar_appear(Window *window) { s_month_view = true; }

static void tick(struct tm *time_value, TimeUnits units) {
  if (s_has_snapshot && calendar_expired(&s_snapshot, time(NULL))) {
    calendar_store_clear(); memset(&s_snapshot, 0, sizeof(s_snapshot)); s_has_snapshot = false;
    event_detail_close();
    snprintf(s_status, sizeof(s_status), "Saved list expired. Refresh.");
  }
  refresh_view();
}
static void layout_views(void) {
  if (s_canvas) {
    GRect bounds = layer_get_unobstructed_bounds(window_get_root_layer(s_window));
    bounds.origin.y += STATUS_BAR_LAYER_HEIGHT; bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;
    layer_set_frame(s_canvas, bounds);
  }
  if (s_agenda_canvas) {
    GRect bounds = layer_get_unobstructed_bounds(window_get_root_layer(s_agenda_window));
    bounds.origin.y += STATUS_BAR_LAYER_HEIGHT; bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;
    layer_set_frame(s_agenda_canvas, bounds);
    GRect list = GRect(0, bounds.origin.y + header_height(), bounds.size.w, bounds.size.h - header_height() - ui_line_height() - 4);
    layer_set_frame(menu_layer_get_layer(s_agenda_menu), list);
  }
  if (s_calendar_canvas) {
    GRect bounds = layer_get_unobstructed_bounds(window_get_root_layer(s_calendar_window));
    bounds.origin.y += STATUS_BAR_LAYER_HEIGHT; bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;
    layer_set_frame(s_calendar_canvas, bounds);
  }
  event_detail_relayout();
}
static void unobstructed(void *context) { layout_views(); refresh_view(); }
static void load(Window *window) {
  s_canvas = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_canvas, draw_main); layer_add_child(window_get_root_layer(window), s_canvas);
  s_status_bar = ui_status_bar(window); layout_views(); refresh_view();
}
static void unload(Window *window) {
  status_bar_layer_destroy(s_status_bar); layer_destroy(s_canvas);
  s_status_bar = NULL; s_canvas = NULL;
}
static void agenda_load(Window *window) {
  s_agenda_canvas = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_agenda_canvas, draw_agenda); layer_add_child(window_get_root_layer(window), s_agenda_canvas);
  s_agenda_menu = menu_layer_create(GRect(0, 0, 1, 1));
  menu_layer_set_normal_colors(s_agenda_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_agenda_menu, GColorCobaltBlue, GColorWhite);
  menu_layer_set_callbacks(s_agenda_menu, NULL, (MenuLayerCallbacks){.get_num_rows = agenda_rows, .get_cell_height = agenda_height,
    .draw_row = agenda_draw, .select_click = agenda_select, .select_long_click = agenda_options, .selection_changed = agenda_selection});
  menu_layer_set_click_config_onto_window(s_agenda_menu, window);
  layer_add_child(window_get_root_layer(window), menu_layer_get_layer(s_agenda_menu));
  s_agenda_status_bar = ui_status_bar(window); layout_views(); refresh_view();
}
static void agenda_unload(Window *window) {
  menu_layer_destroy(s_agenda_menu); status_bar_layer_destroy(s_agenda_status_bar); layer_destroy(s_agenda_canvas);
  s_agenda_menu = NULL; s_agenda_status_bar = NULL; s_agenda_canvas = NULL;
}
static void calendar_load(Window *window) {
  s_calendar_canvas = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_calendar_canvas, draw_calendar);
  layer_add_child(window_get_root_layer(window), s_calendar_canvas);
  s_calendar_status_bar = ui_status_bar(window); layout_views();
}
static void calendar_unload(Window *window) {
  layer_destroy(s_calendar_canvas); status_bar_layer_destroy(s_calendar_status_bar);
  s_calendar_canvas = NULL; s_calendar_status_bar = NULL; s_month_view = false;
}

static void init(void) {
  ui_theme_init();
  s_day = s_calendar_day = calendar_midnight(time(NULL));
  s_has_snapshot = calendar_store_load(&s_snapshot, time(NULL));
  if (s_has_snapshot && s_snapshot.demo != s_demo_build) { calendar_store_clear(); memset(&s_snapshot, 0, sizeof(s_snapshot)); s_has_snapshot = false; }
  if (s_has_snapshot) s_status[0] = 0;
  s_day_count = s_has_snapshot ? calendar_day_events(&s_snapshot, s_day, s_indices) : 0;
  s_window = window_create(); s_agenda_window = window_create(); event_detail_create(open_options); s_calendar_window = window_create(); s_clear_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = load, .unload = unload });
  window_set_click_config_provider(s_window, home_clicks);
  window_set_background_color(s_agenda_window, GColorWhite);
  window_set_window_handlers(s_agenda_window, (WindowHandlers){.load = agenda_load, .unload = agenda_unload});
  window_set_background_color(s_calendar_window, GColorWhite); window_set_background_color(s_clear_window, GColorWhite);
  window_set_window_handlers(s_calendar_window, (WindowHandlers){.load = calendar_load, .unload = calendar_unload, .appear = calendar_appear});
  window_set_click_config_provider(s_calendar_window, calendar_clicks);
  window_set_window_handlers(s_clear_window, (WindowHandlers){.load = clear_load, .unload = clear_unload});
  window_stack_push(s_window, false);
  app_message_register_inbox_received(inbox); app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed); app_message_open(2048, 128);
  connection_service_subscribe((ConnectionHandlers){ .pebble_app_connection_handler = connection_changed });
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers){.did_change = unobstructed}, NULL);
  request_refresh();
}

static void deinit(void) {
  if (s_timeout) app_timer_cancel(s_timeout);
  if (s_retry) app_timer_cancel(s_retry);
  tick_timer_service_unsubscribe(); connection_service_unsubscribe(); app_message_deregister_callbacks();
  unobstructed_area_service_unsubscribe();
  s_pending_action = 0; if (s_actions) action_menu_close(s_actions, false);
  window_destroy(s_clear_window); window_destroy(s_calendar_window); event_detail_destroy(); window_destroy(s_agenda_window); window_destroy(s_window); ui_theme_deinit();
  memset(&s_snapshot, 0, sizeof(s_snapshot)); memset(&s_staging, 0, sizeof(s_staging));
}

int main(void) { init(); app_event_loop(); deinit(); }
