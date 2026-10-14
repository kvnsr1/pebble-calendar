#include "event_detail.h"
#include "room_notes.h"
#include "ui_theme.h"

static Window *s_window, *s_error_window;
static Layer *s_canvas, *s_error_canvas, *s_indicator, *s_indent;
static ScrollLayer *s_scroll;
static DictationSession *s_dictation;
static AppTimer *s_room_redraw;
static CalendarEvent s_event;
static SpokenRoom s_room;
static time_t s_day;
static bool s_has_room, s_demo, s_layout_dirty;
static int s_width, s_viewport, s_total, s_title_height, s_room_top, s_room_height, s_floor_height;
static char s_start[32], s_end[32], s_room_text[48], s_floor_text[64];
static const char *s_error;

static void moment(int32_t epoch, char *result, size_t size) {
  if (s_event.flags & FLAG_ALL_DAY) { snprintf(result, size, "All day"); return; }
  time_t instant = epoch; struct tm date = *localtime(&instant);
  strftime(result, size, clock_is_24h_style() ? "%H:%M" : "%I:%M %p", &date);
  if (!clock_is_24h_style() && result[0] == '0') memmove(result, result + 1, strlen(result));
}
static int measure(const char *value, const char *font, int width) {
  if (!value[0]) return 0;
  return graphics_text_layout_get_content_size(value, fonts_get_system_font(font), GRect(0, 0, width, 1024),
    GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
}
static void background(Layer *layer, GContext *ctx) {
  GColor color = ui_day_color(s_day); int width = s_width, line = ui_line_height();
  graphics_context_set_fill_color(ctx, color); graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(10, 12, 50, 58), 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, color); graphics_draw_line(ctx, GPoint(14, 30), GPoint(56, 30));
  time_t instant = s_event.start; struct tm date = *((s_event.flags & FLAG_ALL_DAY) ? gmtime(&instant) : localtime(&instant));
  char month[8], day[4]; strftime(month, sizeof(month), "%b", &date); snprintf(day, sizeof(day), "%d", date.tm_mday);
  ui_text(ctx, month, FONT_KEY_GOTHIC_14_BOLD, color, GRect(10, 12, 50, 18), GTextAlignmentCenter, GTextOverflowModeWordWrap);
  ui_text(ctx, day, FONT_KEY_LECO_32_BOLD_NUMBERS, color, GRect(10, 32, 50, 38), GTextAlignmentCenter, GTextOverflowModeWordWrap);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < 4; ++i) graphics_fill_rect(ctx, GRect(70 + i, 21 + i, 1, 9 - i * 2), 0, GCornerNone);
  ui_text(ctx, s_start, ui_body_bold_font(), GColorWhite, GRect(84, 12, width - 98, line + 4), GTextAlignmentLeft, GTextOverflowModeWordWrap);
  graphics_fill_rect(ctx, GRect(70, 24 + line, 8, 8), 0, GCornerNone);
  ui_text(ctx, s_end, ui_body_bold_font(), GColorWhite, GRect(84, 12 + line, width - 98, line + 4), GTextAlignmentLeft, GTextOverflowModeWordWrap);
  ui_text(ctx, s_event.title, FONT_KEY_GOTHIC_28_BOLD, GColorWhite, GRect(10, 78, width - 24, s_title_height + 8), GTextAlignmentLeft, GTextOverflowModeWordWrap);
  if (s_has_room) {
    ui_room_icon(ctx, GPoint(12, s_room_top + 8), GColorWhite);
    ui_text(ctx, s_room_text, ui_event_font(), GColorWhite, GRect(48, s_room_top, width - 62, s_room_height + 8), GTextAlignmentLeft, GTextOverflowModeWordWrap);
    ui_text(ctx, s_floor_text, ui_body_font(), GColorWhite, GRect(48, s_room_top + s_room_height + 10, width - 62, s_floor_height + 8), GTextAlignmentLeft, GTextOverflowModeWordWrap);
  }
}
static void draw_indent(Layer *layer, GContext *ctx) { ui_select_indent(layer, ctx); }
static void draw_indicator(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, ui_day_color(s_day)); graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (scroll_layer_get_content_offset(s_scroll).y > s_viewport - s_total) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    for (int i = 0; i < 4; ++i) graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 4 + i, 3 + i, 9 - i * 2, 1), 0, GCornerNone);
  }
}
static void scrolled(struct ScrollLayer *scroll, void *context) { if (s_indicator) layer_mark_dirty(s_indicator); }
void event_detail_relayout(void) {
  if (!s_scroll) return;
  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));
  if (!s_layout_dirty && s_width == bounds.size.w && s_viewport == bounds.size.h - 12) return;
  s_layout_dirty = false; s_width = bounds.size.w; s_viewport = bounds.size.h - 12;
  s_has_room = room_notes_get(&s_event, s_demo, time(NULL), &s_room);
  moment(s_event.start, s_start, sizeof(s_start)); moment(s_event.end, s_end, sizeof(s_end));
  s_title_height = measure(s_event.title, FONT_KEY_GOTHIC_28_BOLD, s_width - 24);
  s_room_top = 78 + s_title_height + 30;
  s_room_text[0] = s_floor_text[0] = 0;
  if (s_has_room) {
    snprintf(s_room_text, sizeof(s_room_text), "%s / %u.%02u", s_room.building, s_room.floor, s_room.number);
    snprintf(s_floor_text, sizeof(s_floor_text), "Floor %u / Room %u", s_room.floor, s_room.number);
  }
  s_room_height = measure(s_room_text, ui_event_font(), s_width - 62);
  s_floor_height = measure(s_floor_text, ui_body_font(), s_width - 62);
  s_total = s_has_room ? s_room_top + s_room_height + s_floor_height + 28 : 78 + s_title_height + 18;
  if (s_total < s_viewport) s_total = s_viewport;
  layer_set_frame(scroll_layer_get_layer(s_scroll), GRect(0, 0, s_width, s_viewport));
  layer_set_frame(s_canvas, GRect(0, 0, s_width, s_total)); scroll_layer_set_content_size(s_scroll, GSize(s_width, s_total));
  layer_set_frame(s_indicator, GRect(0, s_viewport, s_width, 12)); layer_set_frame(s_indent, bounds);
  layer_mark_dirty(s_canvas); layer_mark_dirty(s_indicator); layer_mark_dirty(s_indent);
}
static void error_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  ui_text(ctx, s_error, FONT_KEY_GOTHIC_28_BOLD, GColorWhite, GRect(12, 65, bounds.size.w - 36, 140), GTextAlignmentLeft, GTextOverflowModeWordWrap);
}
static void error_back(ClickRecognizerRef ref, void *context) { window_stack_pop(true); }
static void error_clicks(void *context) { window_single_click_subscribe(BUTTON_ID_SELECT, error_back); }
static void error_load(Window *window) {
  s_error_canvas = layer_create(layer_get_bounds(window_get_root_layer(window))); layer_set_update_proc(s_error_canvas, error_draw);
  layer_add_child(window_get_root_layer(window), s_error_canvas);
}
static void error_unload(Window *window) { layer_destroy(s_error_canvas); s_error_canvas = NULL; }
static void show_error(const char *error) { s_error = error; window_set_background_color(s_error_window, ui_day_color(s_day)); window_stack_push(s_error_window, true); }
static void redraw_room(void *context) {
  s_room_redraw = NULL;
  if (!s_canvas || !window_stack_contains_window(s_window)) return;
  event_detail_relayout();
  layer_mark_dirty(window_get_root_layer(s_window));
}
static void accept_room(const char *transcription) {
  SpokenRoom room = {0};
  if (!room_input_parse(transcription, &room)) { show_error("Room not recognized"); return; }
  if (!room_notes_set(&s_event, s_demo, time(NULL), &room)) { show_error("Room not saved"); return; }
  s_layout_dirty = true;
  if (s_room_redraw) app_timer_cancel(s_room_redraw);
  s_room_redraw = app_timer_register(1, redraw_room, NULL);
}
static void dictated(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if (status == DictationSessionStatusSuccess && s_canvas) accept_room(transcription);
}
static void voice_room(ClickRecognizerRef ref, void *context) {
#ifdef CALENDAR_DEMO_BUILD
  if (s_demo) { accept_room("MB2 three point seven"); return; }
#endif
  if (!s_dictation) s_dictation = dictation_session_create(128, dictated, NULL);
  if (!s_dictation) { show_error("Voice unavailable"); return; }
  dictation_session_enable_confirmation(s_dictation, true);
  if (dictation_session_start(s_dictation) != DictationSessionStatusSuccess) show_error("Voice unavailable");
}
static void up(ClickRecognizerRef ref, void *context) { scroll_layer_scroll_up_click_handler(ref, s_scroll); }
static void down(ClickRecognizerRef ref, void *context) { scroll_layer_scroll_down_click_handler(ref, s_scroll); }
static void back(ClickRecognizerRef ref, void *context) { event_detail_close(); }
static void clicks(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, up); window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, down);
  window_single_click_subscribe(BUTTON_ID_SELECT, voice_room);
  window_single_click_subscribe(BUTTON_ID_BACK, back);
}
static void load(Window *window) {
  s_scroll = scroll_layer_create(GRect(0, 0, 1, 1)); scroll_layer_set_shadow_hidden(s_scroll, true);
  scroll_layer_set_callbacks(s_scroll, (ScrollLayerCallbacks){.content_offset_changed_handler = scrolled});
  s_canvas = layer_create(GRect(0, 0, 1, 1)); layer_set_update_proc(s_canvas, background);
  scroll_layer_add_child(s_scroll, s_canvas); layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(s_scroll));
  s_indicator = layer_create(GRect(0, 0, 1, 1)); layer_set_update_proc(s_indicator, draw_indicator); layer_add_child(window_get_root_layer(window), s_indicator);
  s_indent = layer_create(layer_get_bounds(window_get_root_layer(window))); layer_set_update_proc(s_indent, draw_indent); layer_add_child(window_get_root_layer(window), s_indent);
  event_detail_relayout();
}
static void unload(Window *window) {
  if (s_room_redraw) { app_timer_cancel(s_room_redraw); s_room_redraw = NULL; }
  if (s_dictation) { dictation_session_destroy(s_dictation); s_dictation = NULL; }
  layer_destroy(s_indent); layer_destroy(s_indicator); layer_destroy(s_canvas); scroll_layer_destroy(s_scroll);
  s_indent = s_indicator = s_canvas = NULL; s_scroll = NULL; s_width = s_viewport = 0;
  memset(&s_event, 0, sizeof(s_event)); memset(&s_room, 0, sizeof(s_room)); memset(s_room_text, 0, sizeof(s_room_text)); memset(s_floor_text, 0, sizeof(s_floor_text)); s_has_room = false;
}
static void create_window(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){.load = load, .unload = unload});
  window_set_click_config_provider(s_window, clicks);
}
Window *event_detail_create(void) {
  create_window();
  s_error_window = window_create();
  window_set_window_handlers(s_error_window, (WindowHandlers){.load = error_load, .unload = error_unload});
  window_set_click_config_provider(s_error_window, error_clicks); return s_window;
}
void event_detail_open(const CalendarEvent *event, time_t day, bool demo) {
  s_layout_dirty = true; s_event = *event; s_day = day; s_demo = demo;
  window_set_background_color(s_window, ui_day_color(day));
  if (window_stack_contains_window(s_window)) event_detail_relayout();
  else window_stack_push(s_window, true);
}
void event_detail_close(void) {
  if (s_error_window && window_stack_contains_window(s_error_window)) window_stack_remove(s_error_window, false);
  if (s_window && window_stack_contains_window(s_window)) window_stack_remove(s_window, false);
}
void event_detail_destroy(void) { window_destroy(s_error_window); window_destroy(s_window); s_window = s_error_window = NULL; }
