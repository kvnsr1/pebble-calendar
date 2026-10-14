#pragma once
#include <pebble.h>

bool ui_large_text(void);
const char *ui_body_font(void);
const char *ui_body_bold_font(void);
const char *ui_event_font(void);
int ui_line_height(void);
void ui_text(GContext *ctx, const char *value, const char *font, GColor color,
             GRect rect, GTextAlignment align, GTextOverflowMode overflow);
StatusBarLayer *ui_status_bar(Window *window);
ActionBarLayer *ui_confirmation_bar(Window *window, ClickConfigProvider clicks);
void ui_theme_init(void);
void ui_theme_deinit(void);
