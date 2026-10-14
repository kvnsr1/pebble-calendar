#include "ui_theme.h"
#include "calendar.h"

static bool s_large;
static GBitmap *s_check;

bool ui_large_text(void) { return s_large; }
const char *ui_body_font(void) { return s_large ? FONT_KEY_GOTHIC_24 : FONT_KEY_GOTHIC_18; }
const char *ui_body_bold_font(void) { return s_large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD; }
const char *ui_event_font(void) { return s_large ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD; }
int ui_line_height(void) { return s_large ? 28 : 22; }

void ui_text(GContext *ctx, const char *value, const char *font, GColor color,
             GRect rect, GTextAlignment align, GTextOverflowMode overflow) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, value, fonts_get_system_font(font), rect, overflow, align, NULL);
}

StatusBarLayer *ui_status_bar(Window *window) {
  StatusBarLayer *bar = status_bar_layer_create();
  status_bar_layer_set_colors(bar, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(window_get_root_layer(window), status_bar_layer_get_layer(bar));
  return bar;
}

ActionBarLayer *ui_confirmation_bar(Window *window, ClickConfigProvider clicks) {
  ActionBarLayer *bar = action_bar_layer_create();
  action_bar_layer_set_background_color(bar, GColorBlack);
  action_bar_layer_set_click_config_provider(bar, clicks);
  action_bar_layer_set_icon(bar, BUTTON_ID_SELECT, s_check);
  action_bar_layer_add_to_window(bar, window);
  return bar;
}

void ui_theme_init(void) {
  s_large = preferred_content_size() == PreferredContentSizeExtraLarge;
  s_check = gbitmap_create_with_resource(RESOURCE_ID_ACTION_CHECK);
}

void ui_theme_deinit(void) {
  gbitmap_destroy(s_check);
}

GColor ui_day_color(time_t day) {
  int relation = calendar_day_relation(day, time(NULL));
  return relation < 0 ? GColorDarkGray : relation > 0 ? GColorMidnightGreen : GColorOxfordBlue;
}
void ui_date_icon(GContext *ctx, time_t instant, bool utc, GPoint origin, GColor ink) {
  struct tm date = *(utc ? gmtime(&instant) : localtime(&instant));
  char month[8], day[4]; strftime(month, sizeof(month), "%b", &date); snprintf(day, sizeof(day), "%d", date.tm_mday);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(origin.x, origin.y, 50, 58), 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, ink);
  graphics_draw_line(ctx, GPoint(origin.x + 4, origin.y + 18), GPoint(origin.x + 46, origin.y + 18));
  ui_text(ctx, month, FONT_KEY_GOTHIC_14_BOLD, ink, GRect(origin.x, origin.y, 50, 18), GTextAlignmentCenter, GTextOverflowModeWordWrap);
  ui_text(ctx, day, FONT_KEY_LECO_32_BOLD_NUMBERS, ink, GRect(origin.x, origin.y + 20, 50, 38), GTextAlignmentCenter, GTextOverflowModeWordWrap);
}
void ui_room_icon(GContext *ctx, GPoint origin, GColor ink) {
  graphics_context_set_stroke_color(ctx, ink);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_rect(ctx, GRect(origin.x, origin.y, 20, 28));
  graphics_draw_line(ctx, GPoint(origin.x - 3, origin.y + 28), GPoint(origin.x + 23, origin.y + 28));
  graphics_context_set_fill_color(ctx, ink); graphics_fill_circle(ctx, GPoint(origin.x + 14, origin.y + 15), 1);
  graphics_context_set_stroke_width(ctx, 1);
}
void ui_select_indent(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(bounds.size.w + 4, bounds.size.h / 2), 13);
}
