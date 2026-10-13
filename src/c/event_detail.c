#include "event_detail.h"

static Window *s_window;
static ScrollLayer *s_scroll;
static Layer *s_content;
static CalendarEvent s_event;
static bool s_cached;
static int s_title_height, s_room_height, s_people_height, s_calendar_height;
static int s_room_top, s_people_top, s_calendar_top, s_total_height;
static char s_start[20], s_end[20], s_start_date[32], s_end_date[32];
static char s_room_text[512], s_people[384], s_calendar[80];

static GSize measure(const char *text, const char *font, int width) {
  return graphics_text_layout_get_content_size(text, fonts_get_system_font(font), GRect(0, 0, width, 3000),
    GTextOverflowModeWordWrap, GTextAlignmentLeft);
}

static void text(GContext *ctx, const char *value, const char *font, GColor color, GRect rect) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, value, fonts_get_system_font(font), rect, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void label(GContext *ctx, const char *value, int y, int width) {
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(10, y), GPoint(width - 10, y));
  text(ctx, value, FONT_KEY_GOTHIC_14_BOLD, GColorCobaltBlue, GRect(10, y + 6, width - 20, 19));
}

static void format_moment(int32_t epoch, bool all_day, bool end, char *clock, size_t clock_size, char *date, size_t date_size) {
  time_t instant = epoch - (all_day && end ? 1 : 0);
  struct tm value = *(all_day ? gmtime(&instant) : localtime(&instant));
  strftime(date, date_size, "%a, %b %e", &value);
  if (all_day) snprintf(clock, clock_size, "All day");
  else {
    strftime(clock, clock_size, clock_is_24h_style() ? "%H:%M" : "%I:%M%p", &value);
    if (!clock_is_24h_style() && clock[0] == '0') memmove(clock, clock + 1, strlen(clock));
  }
}

static void draw(Layer *layer, GContext *ctx) {
  int width = layer_get_bounds(layer).size.w;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, GRect(0, 0, width, s_title_height + 39), 0, GCornerNone);
  text(ctx, "EVENT NAME", FONT_KEY_GOTHIC_14_BOLD, GColorWhite, GRect(10, 6, width - 20, 18));
  text(ctx, s_event.title, FONT_KEY_GOTHIC_24_BOLD, GColorWhite, GRect(10, 23, width - 20, s_title_height + 8));

  int y = s_title_height + 48, half = (width - 24) / 2;
  text(ctx, "START TIME", FONT_KEY_GOTHIC_14_BOLD, GColorDarkGray, GRect(10, y, half, 18));
  text(ctx, "END TIME", FONT_KEY_GOTHIC_14_BOLD, GColorDarkGray, GRect(width / 2 + 3, y, half, 18));
  text(ctx, s_start, FONT_KEY_GOTHIC_24_BOLD, GColorBlack, GRect(10, y + 17, half, 30));
  text(ctx, s_end, FONT_KEY_GOTHIC_24_BOLD, GColorBlack, GRect(width / 2 + 3, y + 17, half, 30));
  text(ctx, s_start_date, FONT_KEY_GOTHIC_14, GColorDarkGray, GRect(10, y + 47, half, 20));
  text(ctx, s_end_date, FONT_KEY_GOTHIC_14, GColorDarkGray, GRect(width / 2 + 3, y + 47, half, 20));

  label(ctx, "ROOM", s_room_top, width);
  text(ctx, s_room_text, FONT_KEY_GOTHIC_18, GColorBlack, GRect(10, s_room_top + 26, width - 20, s_room_height + 8));
  label(ctx, "ATTENDEE LIST", s_people_top, width);
  text(ctx, s_people, FONT_KEY_GOTHIC_18, GColorBlack, GRect(10, s_people_top + 26, width - 20, s_people_height + 8));
  label(ctx, "CALENDAR", s_calendar_top, width);
  text(ctx, s_calendar, FONT_KEY_GOTHIC_18, GColorDarkGray, GRect(10, s_calendar_top + 26, width - 20, s_calendar_height + 8));
  const char *footer = s_event.flags & FLAG_TRUNCATED ? "Long metadata shortened" : s_cached ? "Saved event / refresh for updates" : "Up / Down to scroll";
  text(ctx, footer, FONT_KEY_GOTHIC_14, GColorDarkGray, GRect(10, s_total_height - 25, width - 20, 22));
}

static void prepare(int width) {
  bool all_day = s_event.flags & FLAG_ALL_DAY;
  format_moment(s_event.start, all_day, false, s_start, sizeof(s_start), s_start_date, sizeof(s_start_date));
  format_moment(s_event.end, all_day, true, s_end, sizeof(s_end), s_end_date, sizeof(s_end_date));
  if (s_event.room.source) {
    CalendarRoom *room = &s_event.room;
    snprintf(s_room_text, sizeof(s_room_text), "%s\nOffice: %s\nBuilding: %s\nFloor %ld / Room %s\nCapacity: %ld people%s%s\n\n%s",
      room->name, room->office, room->building, (long)room->floor, room->number, (long)room->capacity,
      room->facilities[0] ? "\n" : "", room->facilities, room->source == 1 ? "From room attendee" : "From event location");
  } else {
    snprintf(s_room_text, sizeof(s_room_text), "%s%s%s", "Not provided",
      s_event.location[0] ? "\n\nLocation\n" : "", s_event.location);
  }
  snprintf(s_people, sizeof(s_people), "%s", s_event.flags & FLAG_ATTENDEES_AVAILABLE ?
    (s_event.attendees[0] ? s_event.attendees : "No people listed") : "Not provided by phone");
  snprintf(s_calendar, sizeof(s_calendar), "%s", s_event.calendar[0] ? s_event.calendar : "Not provided");
  s_title_height = measure(s_event.title, FONT_KEY_GOTHIC_24_BOLD, width - 20).h;
  s_room_height = measure(s_room_text, FONT_KEY_GOTHIC_18, width - 20).h;
  s_people_height = measure(s_people, FONT_KEY_GOTHIC_18, width - 20).h;
  s_calendar_height = measure(s_calendar, FONT_KEY_GOTHIC_18, width - 20).h;
  s_room_top = s_title_height + 125;
  s_people_top = s_room_top + s_room_height + 40;
  s_calendar_top = s_people_top + s_people_height + 40;
  s_total_height = s_calendar_top + s_calendar_height + 75;
}

static void load(Window *window) {
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  prepare(bounds.size.w);
  s_scroll = scroll_layer_create(bounds);
  s_content = layer_create(GRect(0, 0, bounds.size.w, s_total_height));
  layer_set_update_proc(s_content, draw);
  scroll_layer_add_child(s_scroll, s_content);
  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, s_total_height));
  layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(s_scroll));
}

static void unload(Window *window) {
  layer_destroy(s_content); scroll_layer_destroy(s_scroll); s_content = NULL; s_scroll = NULL;
  memset(&s_event, 0, sizeof(s_event)); memset(s_room_text, 0, sizeof(s_room_text)); memset(s_people, 0, sizeof(s_people));
}

static void scroll(int offset) {
  GPoint position = scroll_layer_get_content_offset(s_scroll); position.y += offset;
  scroll_layer_set_content_offset(s_scroll, position, false);
}
static void up(ClickRecognizerRef recognizer, void *context) { scroll(45); }
static void down(ClickRecognizerRef recognizer, void *context) { scroll(-45); }
static void back(ClickRecognizerRef recognizer, void *context) { window_stack_pop(true); }
static void click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, down);
  window_single_click_subscribe(BUTTON_ID_BACK, back);
}

Window *event_detail_create(void) {
  s_window = window_create(); window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = load, .unload = unload });
  window_set_click_config_provider(s_window, click_config);
  return s_window;
}
void event_detail_open(const CalendarEvent *event, bool cached) { s_event = *event; s_cached = cached; window_stack_push(s_window, true); }
void event_detail_close(void) { if (s_window && window_stack_contains_window(s_window)) window_stack_remove(s_window, false); }
void event_detail_destroy(void) { window_destroy(s_window); s_window = NULL; }
