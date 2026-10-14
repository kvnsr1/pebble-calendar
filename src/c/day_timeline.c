#include "day_timeline.h"
#include "ui_theme.h"

static Window *s_window;
static Layer *s_canvas;
static Animation *s_animation;
static const CalendarSnapshot *s_snapshot;
static int s_indices[EVENT_LIMIT], s_count, s_selected, s_offset, s_from, s_to;
static time_t s_day;
static void (*s_open_event)(const CalendarEvent *, time_t);
static void (*s_options)(void);
static int row_height(void) { return ui_large_text() ? 118 : 106; }

static void draw_icon(GContext *ctx, int x, int y) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x, y, 20, 23), 1, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, GRect(x, y, 20, 23));
  graphics_draw_line(ctx, GPoint(x + 3, y + 7), GPoint(x + 17, y + 7));
  graphics_draw_line(ctx, GPoint(x + 5, y + 12), GPoint(x + 9, y + 16));
  graphics_draw_line(ctx, GPoint(x + 9, y + 16), GPoint(x + 15, y + 10));
  graphics_draw_line(ctx, GPoint(x + 5, y - 2), GPoint(x + 5, y + 3));
  graphics_draw_line(ctx, GPoint(x + 15, y - 2), GPoint(x + 15, y + 3));
}
static void draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int rail = ui_large_text() ? 32 : 28, edge = bounds.size.w - rail;
  graphics_context_set_fill_color(ctx, GColorWhite); graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorVividCerulean);
  graphics_fill_rect(ctx, GRect(edge, 0, rail, bounds.size.h), 0, GCornerNone);
  for (int i = 0; i < 10; ++i) graphics_fill_rect(ctx, GRect(edge - i, 16 - i, 1, i * 2 + 1), 0, GCornerNone);
  if (!s_count) {
    ui_text(ctx, "No entries", ui_event_font(), GColorBlack, GRect(10, 9, edge - 20, 80), GTextAlignmentLeft, GTextOverflowModeWordWrap);
    draw_icon(ctx, edge + (rail - 20) / 2, 7); return;
  }
  for (int i = 0; i < s_count; ++i) {
    int y = 5 + i * row_height() - s_offset;
    if (y + row_height() <= 0 || y >= bounds.size.h) continue;
    const CalendarEvent *event = &s_snapshot->events[s_indices[i]];
    char moment[16], period[8] = "";
    if (event->flags & FLAG_ALL_DAY) {
      ui_text(ctx, "All day", FONT_KEY_GOTHIC_28_BOLD, GColorBlack, GRect(10, y - 5, edge - 20, 38), GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);
    } else {
      time_t instant = event->start; struct tm date = *localtime(&instant);
      strftime(moment, sizeof(moment), clock_is_24h_style() ? "%H:%M" : "%I:%M", &date);
      if (!clock_is_24h_style()) {
        if (moment[0] == '0') memmove(moment, moment + 1, strlen(moment));
        strftime(period, sizeof(period), "%p", &date);
      }
      GFont font = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
      int number_width = graphics_text_layout_get_content_size(moment, font, GRect(0, 0, edge - 20, 40), GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
      ui_text(ctx, moment, FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM, GColorBlack, GRect(10, y, edge - 20, 36), GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);
      ui_text(ctx, period, FONT_KEY_GOTHIC_18_BOLD, GColorBlack, GRect(14 + number_width, y + 7, edge - number_width - 24, 24), GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);
    }
    ui_text(ctx, event->title, ui_event_font(), GColorBlack, GRect(10, y + 34, edge - 20, ui_large_text() ? 36 : 32), GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);
    ui_text(ctx, event->location[0] ? event->location : event->calendar, ui_body_font(), GColorBlack,
      GRect(10, y + (ui_large_text() ? 72 : 65), edge - 20, ui_line_height()), GTextAlignmentLeft, GTextOverflowModeTrailingEllipsis);
    draw_icon(ctx, edge + (rail - 20) / 2, y + 3);
  }
}
static void animate(Animation *animation, AnimationProgress progress) {
  s_offset = s_from + (int32_t)(s_to - s_from) * progress / ANIMATION_NORMALIZED_MAX;
  if (s_canvas) layer_mark_dirty(s_canvas);
}
static void stopped(Animation *animation, bool finished, void *context) {
  if (animation == s_animation) {
    s_animation = NULL;
    if (finished) s_offset = s_to;
    if (s_canvas) layer_mark_dirty(s_canvas);
  }
}
static void move(int direction) {
  int next = s_selected + direction;
  if (next < 0 || next >= s_count) return;
  if (s_animation) animation_unschedule(s_animation);
  s_selected = next; s_from = s_offset; s_to = next * row_height();
  // SDK 3 destroys completed/cancelled animations: create one for each move.
  s_animation = animation_create();
  if (s_animation) {
    static const AnimationImplementation implementation = {.update = animate};
    animation_set_implementation(s_animation, &implementation);
    animation_set_duration(s_animation, 180); animation_set_curve(s_animation, AnimationCurveEaseInOut);
    animation_set_handlers(s_animation, (AnimationHandlers){.stopped = stopped}, NULL);
    if (animation_schedule(s_animation)) return;
    animation_destroy(s_animation); s_animation = NULL;
  }
  s_offset = s_to; layer_mark_dirty(s_canvas);
}
static void up(ClickRecognizerRef ref, void *context) { move(-1); }
static void down(ClickRecognizerRef ref, void *context) { move(1); }
static void select_event(ClickRecognizerRef ref, void *context) {
  if (s_animation) animation_unschedule(s_animation);
  s_offset = s_to;
  if (s_count && s_open_event) s_open_event(&s_snapshot->events[s_indices[s_selected]], s_day);
}
static void options(ClickRecognizerRef ref, void *context) { if (s_options) s_options(); }
static void back(ClickRecognizerRef ref, void *context) { day_timeline_close(); }
static void clicks(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 220, up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 220, down);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_event);
  window_single_click_subscribe(BUTTON_ID_BACK, back);
  window_long_click_subscribe(BUTTON_ID_SELECT, 650, options, NULL);
}
void day_timeline_relayout(void) {
  if (s_canvas) layer_set_frame(s_canvas, layer_get_bounds(window_get_root_layer(s_window)));
}
static void load(Window *window) {
  s_selected = s_offset = s_from = s_to = 0;
  s_canvas = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_canvas, draw); layer_add_child(window_get_root_layer(window), s_canvas);
  s_animation = NULL;
  day_timeline_relayout();
}
static void unload(Window *window) {
  if (s_animation) animation_unschedule(s_animation);
  layer_destroy(s_canvas); s_canvas = NULL;
}
Window *day_timeline_create(void (*open_event)(const CalendarEvent *, time_t), void (*options_callback)(void)) {
  s_open_event = open_event; s_options = options_callback;
  s_window = window_create(); window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){.load = load, .unload = unload});
  window_set_click_config_provider(s_window, clicks); return s_window;
}
void day_timeline_update(const CalendarSnapshot *snapshot, const int *indices, int count, time_t day) {
  s_snapshot = snapshot; s_day = day; s_count = count;
  memcpy(s_indices, indices, count * sizeof(*indices));
  if (s_selected >= count) s_selected = count ? count - 1 : 0;
  if (s_canvas) {
    if (s_animation) animation_unschedule(s_animation);
    s_offset = s_to = s_selected * row_height(); layer_mark_dirty(s_canvas);
  }
}
void day_timeline_open(void) { window_stack_push(s_window, false); }
void day_timeline_close(void) { if (window_stack_contains_window(s_window)) window_stack_remove(s_window, false); }
void day_timeline_destroy(void) { window_destroy(s_window); s_window = NULL; s_snapshot = NULL; memset(s_indices, 0, sizeof(s_indices)); }
