#include "event_detail.h"
#include "ui_theme.h"

static Window *s_window;
static ScrollLayer *s_scroll;
static Layer *s_content, *s_indicator, *s_action_indent;
static StatusBarLayer *s_status_bar;
static CalendarEvent s_event;
static bool s_cached;
static void (*s_options)(void);
static int s_width, s_viewport_height, s_total_height;
static int s_title_height, s_room_name_height, s_room_height, s_people_height, s_calendar_height;
static int s_title_top, s_header_height;
static int s_time_top, s_end_top, s_time_height, s_room_top, s_people_top, s_calendar_top;
static char s_start[16], s_end[16], s_start_period[8], s_end_period[8], s_start_date[32], s_end_date[32];
static char s_room_name[128], s_room_text[512], s_people[384], s_calendar[80];

static int measure(const char *value, const char *font, int width) {
  if (!value[0]) return 0;
  return graphics_text_layout_get_content_size(value, fonts_get_system_font(font), GRect(0, 0, width, 6000),
      GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
}
static void text(GContext *ctx, const char *value, const char *font, GColor color, GRect rect) {
  ui_text(ctx, value, font, color, rect, GTextAlignmentLeft, GTextOverflowModeWordWrap);
}
static void label(GContext *ctx, const char *value, int y, int width) {
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(10, y), GPoint(width - 10, y));
  text(ctx, value, ui_body_bold_font(), GColorCobaltBlue, GRect(10, y + 6, width - 20, ui_line_height()));
}
static void format_moment(int32_t epoch, bool all_day, bool end, char *clock, char *period, char *date) {
  time_t instant = epoch - (all_day && end ? 1 : 0);
  struct tm value = *(all_day ? gmtime(&instant) : localtime(&instant));
  strftime(date, 32, ui_large_text() ? "%a, %b %e" : "%a\n%b %e", &value);
  period[0] = 0;
  if (all_day) snprintf(clock, 16, "All day");
  else {
    strftime(clock, 16, clock_is_24h_style() ? "%H:%M" : "%I:%M", &value);
    if (!clock_is_24h_style()) {
      if (clock[0] == '0') memmove(clock, clock + 1, strlen(clock));
      strftime(period, 8, "%p", &value);
    }
  }
}
static void draw_time(GContext *ctx, int x, int y, int width, const char *caption,
                      const char *clock, const char *period, const char *date) {
  text(ctx, caption, ui_body_bold_font(), GColorCobaltBlue, GRect(x, y, width, ui_line_height()));
  text(ctx, clock, s_event.flags & FLAG_ALL_DAY ? ui_body_bold_font() : ui_large_text() ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD,
       GColorBlack, GRect(x, y + ui_line_height(), width, 35));
  int date_top = y + ui_line_height() + 32;
  if (period[0]) {
    text(ctx, period, ui_body_font(), GColorDarkGray, GRect(x, date_top, width, ui_line_height()));
    date_top += ui_line_height();
  }
  text(ctx, date, ui_body_font(), GColorDarkGray, GRect(x, date_top, width, s_time_height));
}
static void draw_header(GContext *ctx, int width) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, 0, width, s_header_height), 0, GCornerNone);
  // Calendar glyph is drawn from primitives; no private Timeline API is used.
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(10, 12, 50, 58), 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  graphics_draw_line(ctx, GPoint(14, 30), GPoint(56, 30));
  time_t instant = s_event.start;
  struct tm date = *((s_event.flags & FLAG_ALL_DAY) ? gmtime(&instant) : localtime(&instant));
  char month[8], day[4]; strftime(month, sizeof(month), "%b", &date); snprintf(day, sizeof(day), "%d", date.tm_mday);
  ui_text(ctx, month, FONT_KEY_GOTHIC_14_BOLD, GColorOxfordBlue, GRect(10, 12, 50, 18), GTextAlignmentCenter, GTextOverflowModeWordWrap);
  ui_text(ctx, day, FONT_KEY_LECO_32_BOLD_NUMBERS, GColorOxfordBlue, GRect(10, 32, 50, 38), GTextAlignmentCenter, GTextOverflowModeWordWrap);
  int line = ui_line_height();
  char start[32], end[32];
  snprintf(start, sizeof(start), "%s%s%s", s_start, s_start_period[0] ? " " : "", s_start_period);
  snprintf(end, sizeof(end), "%s%s%s", s_end, s_end_period[0] ? " " : "", s_end_period);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < 4; ++i) graphics_fill_rect(ctx, GRect(70 + i, 21 + i, 1, 9 - i * 2), 0, GCornerNone);
  text(ctx, start, ui_body_bold_font(), GColorWhite, GRect(84, 12, width - 98, line + 4));
  graphics_fill_rect(ctx, GRect(70, 24 + line, 8, 8), 0, GCornerNone);
  text(ctx, end, ui_body_bold_font(), GColorWhite, GRect(84, 12 + line, width - 98, line + 4));
  text(ctx, s_event.title, FONT_KEY_GOTHIC_28_BOLD, GColorWhite, GRect(10, s_title_top, width - 24, s_title_height + 8));
}
static void draw(Layer *layer, GContext *ctx) {
  int width = layer_get_bounds(layer).size.w, line = ui_line_height();
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  draw_header(ctx, width);
  label(ctx, s_event.room.source || !s_event.location[0] ? "Room" : "Location", s_room_top, width);
  text(ctx, s_room_name, ui_event_font(), GColorBlack, GRect(10, s_room_top + line + 10, width - 24, s_room_name_height + 8));
  text(ctx, s_room_text, ui_body_font(), GColorBlack, GRect(10, s_room_top + line + s_room_name_height + 16, width - 24, s_room_height + 8));
  if (ui_large_text()) {
    draw_time(ctx, 10, s_time_top, width - 24, "Start time", s_start, s_start_period, s_start_date);
    draw_time(ctx, 10, s_end_top, width - 24, "End time", s_end, s_end_period, s_end_date);
  } else {
    int half = (width - 30) / 2;
    draw_time(ctx, 10, s_time_top, half, "Start time", s_start, s_start_period, s_start_date);
    draw_time(ctx, 16 + half, s_time_top, half, "End time", s_end, s_end_period, s_end_date);
  }
  label(ctx, "Attendee list", s_people_top, width);
  text(ctx, s_people, ui_body_font(), GColorBlack, GRect(10, s_people_top + line + 10, width - 24, s_people_height + 8));
  label(ctx, "Calendar", s_calendar_top, width);
  text(ctx, s_calendar, ui_body_font(), GColorBlack, GRect(10, s_calendar_top + line + 10, width - 24, s_calendar_height + 8));
  const char *footer = s_event.flags & FLAG_TRUNCATED ? "Long metadata shortened" : s_cached ? "Saved event / refresh for updates" : "";
  text(ctx, footer, ui_body_font(), GColorDarkGray, GRect(10, s_calendar_top + line + s_calendar_height + 26, width - 24, 90));
}
static void draw_indent(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(bounds.size.w + 4, bounds.size.h / 2), 13);
}
static void prepare(int width) {
  bool all_day = s_event.flags & FLAG_ALL_DAY;
  format_moment(s_event.start, all_day, false, s_start, s_start_period, s_start_date);
  format_moment(s_event.end, all_day, true, s_end, s_end_period, s_end_date);
  if (s_event.room.source) {
    CalendarRoom *room = &s_event.room;
    snprintf(s_room_name, sizeof(s_room_name), "%s", room->name);
    snprintf(s_room_text, sizeof(s_room_text), "%s / %s\nFloor %ld / Room %s\nCapacity: %ld people%s%s\n\n%s",
      room->office, room->building, (long)room->floor, room->number, (long)room->capacity,
      room->facilities[0] ? "\n" : "", room->facilities, room->source == 1 ? "From room attendee" : "From event location");
  } else {
    snprintf(s_room_name, sizeof(s_room_name), "%s", s_event.location[0] ? s_event.location : "Not provided");
    s_room_text[0] = 0;
  }
  snprintf(s_people, sizeof(s_people), "%s", s_event.flags & FLAG_ATTENDEES_AVAILABLE ?
      (s_event.attendees[0] ? s_event.attendees : "No people listed") : "Not provided by phone");
  snprintf(s_calendar, sizeof(s_calendar), "%s", s_event.calendar[0] ? s_event.calendar : "Not provided");
  int body_width = width - 24, line = ui_line_height();
  s_title_height = measure(s_event.title, FONT_KEY_GOTHIC_28_BOLD, body_width);
  s_room_name_height = measure(s_room_name, ui_event_font(), body_width);
  s_room_height = measure(s_room_text, ui_body_font(), body_width);
  s_people_height = measure(s_people, ui_body_font(), body_width);
  s_calendar_height = measure(s_calendar, ui_body_font(), body_width);
  int time_width = ui_large_text() ? body_width : (width - 30) / 2;
  int start_date = measure(s_start_date, ui_body_font(), time_width), end_date = measure(s_end_date, ui_body_font(), time_width);
  s_time_height = line + 32 + (s_start_period[0] || s_end_period[0] ? line : 0) + (start_date > end_date ? start_date : end_date) + 12;
  s_title_top = ui_large_text() ? 80 : 76;
  s_header_height = s_title_top + s_title_height + 14;
  s_room_top = s_header_height + 6;
  s_time_top = s_room_top + line + s_room_name_height + s_room_height + 38;
  s_end_top = s_time_top + s_time_height;
  s_people_top = s_time_top + s_time_height * (ui_large_text() ? 2 : 1) + 8;
  s_calendar_top = s_people_top + line + s_people_height + 28;
  const char *footer = s_event.flags & FLAG_TRUNCATED ? "Long metadata shortened" : s_cached ? "Saved event / refresh for updates" : "";
  s_total_height = s_calendar_top + line + s_calendar_height + measure(footer, ui_body_font(), body_width) + 42;
}
void event_detail_relayout(void) {
  if (!s_scroll) return;
  GRect bounds = layer_get_unobstructed_bounds(window_get_root_layer(s_window));
  int width = bounds.size.w, height = bounds.size.h - STATUS_BAR_LAYER_HEIGHT - 14;
  if (width == s_width && height == s_viewport_height) return;
  s_width = width; s_viewport_height = height; prepare(width);
  layer_set_frame(scroll_layer_get_layer(s_scroll), GRect(0, STATUS_BAR_LAYER_HEIGHT, width, height));
  layer_set_frame(s_content, GRect(0, 0, width, s_total_height));
  scroll_layer_set_content_size(s_scroll, GSize(width, s_total_height));
  layer_set_frame(status_bar_layer_get_layer(s_status_bar), GRect(0, 0, width, STATUS_BAR_LAYER_HEIGHT));
  layer_set_frame(s_indicator, GRect(width / 2 - 15, bounds.size.h - 14, 30, 14));
  layer_set_frame(s_action_indent, bounds);
}
static void up(ClickRecognizerRef ref, void *context) { scroll_layer_scroll_up_click_handler(ref, s_scroll); }
static void down(ClickRecognizerRef ref, void *context) { scroll_layer_scroll_down_click_handler(ref, s_scroll); }
static void options(ClickRecognizerRef ref, void *context) { if (s_options) s_options(); }
static void clicks(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, down);
  window_single_click_subscribe(BUTTON_ID_SELECT, options);
}
static void load(Window *window) {
  s_scroll = scroll_layer_create(GRect(0, 0, 1, 1));
  scroll_layer_set_shadow_hidden(s_scroll, true);
  s_content = layer_create(GRect(0, 0, 1, 1)); layer_set_update_proc(s_content, draw);
  scroll_layer_add_child(s_scroll, s_content);
  layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(s_scroll));
  s_status_bar = ui_status_bar(window);
  s_indicator = layer_create(GRect(0, 0, 1, 1)); layer_add_child(window_get_root_layer(window), s_indicator);
  ContentIndicatorConfig indicator = {.layer = s_indicator, .alignment = GAlignCenter, .times_out = false,
                                      .colors = {.foreground = GColorBlack, .background = GColorWhite}};
  content_indicator_configure_direction(scroll_layer_get_content_indicator(s_scroll), ContentIndicatorDirectionDown, &indicator);
  s_action_indent = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_action_indent, draw_indent);
  layer_add_child(window_get_root_layer(window), s_action_indent);
  event_detail_relayout();
}
static void unload(Window *window) {
  layer_destroy(s_action_indent); s_action_indent = NULL;
  layer_destroy(s_content); scroll_layer_destroy(s_scroll); layer_destroy(s_indicator); status_bar_layer_destroy(s_status_bar);
  s_content = s_indicator = NULL; s_scroll = NULL; s_status_bar = NULL; s_width = s_viewport_height = 0;
  memset(&s_event, 0, sizeof(s_event)); memset(s_room_name, 0, sizeof(s_room_name)); memset(s_room_text, 0, sizeof(s_room_text));
  memset(s_people, 0, sizeof(s_people)); memset(s_calendar, 0, sizeof(s_calendar));
}
Window *event_detail_create(void (*open_options)(void)) {
  s_options = open_options; s_window = window_create(); window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, clicks);
  window_set_window_handlers(s_window, (WindowHandlers){.load = load, .unload = unload});
  return s_window;
}
void event_detail_open(const CalendarEvent *event, bool cached) { s_event = *event; s_cached = cached; window_stack_push(s_window, true); }
void event_detail_close(void) { if (s_window && window_stack_contains_window(s_window)) window_stack_remove(s_window, false); }
void event_detail_destroy(void) { window_destroy(s_window); s_window = NULL; }
