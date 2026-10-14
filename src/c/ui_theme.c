#include "ui_theme.h"

static bool s_large;
static GBitmap *s_up, *s_down, *s_more, *s_check;

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

ActionBarLayer *ui_action_bar(Window *window, ClickConfigProvider clicks, bool confirm) {
  ActionBarLayer *bar = action_bar_layer_create();
  action_bar_layer_set_background_color(bar, GColorBlack);
  action_bar_layer_set_click_config_provider(bar, clicks);
  if (!confirm) {
    action_bar_layer_set_icon(bar, BUTTON_ID_UP, s_up);
    action_bar_layer_set_icon(bar, BUTTON_ID_DOWN, s_down);
  }
  action_bar_layer_set_icon(bar, BUTTON_ID_SELECT, confirm ? s_check : s_more);
  action_bar_layer_add_to_window(bar, window);
  return bar;
}

void ui_theme_init(void) {
  s_large = preferred_content_size() == PreferredContentSizeExtraLarge;
  s_up = gbitmap_create_with_resource(RESOURCE_ID_ACTION_UP);
  s_down = gbitmap_create_with_resource(RESOURCE_ID_ACTION_DOWN);
  s_more = gbitmap_create_with_resource(RESOURCE_ID_ACTION_MORE);
  s_check = gbitmap_create_with_resource(RESOURCE_ID_ACTION_CHECK);
}

void ui_theme_deinit(void) {
  gbitmap_destroy(s_up); gbitmap_destroy(s_down); gbitmap_destroy(s_more); gbitmap_destroy(s_check);
}
