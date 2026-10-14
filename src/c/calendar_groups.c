#include "calendar_groups.h"
#include "ui_theme.h"
#define GROUP_BASE 350
#define GROUP_VERSION 1

typedef struct { uint64_t hash; uint32_t version; int32_t group; uint32_t checksum; } GroupPreference;
static Window *s_window;
static MenuLayer *s_menu;
static ActionMenu *s_actions;
static ActionMenuLevel *s_root;
static GroupPreference s_preferences[EVENT_LIMIT];
static char s_names[EVENT_LIMIT][64];
static int s_count, s_chosen;
static void (*s_changed)(void);
static const char *group_name(CalendarGroup group) { return group == CalendarGroupWork ? "Work" : group == CalendarGroupPersonal ? "Personal" : "Other"; }
static uint32_t checksum(const GroupPreference *preference) {
  const uint8_t *bytes = (const uint8_t *)preference; uint32_t hash = 2166136261u;
  for (size_t i = 0; i < offsetof(GroupPreference, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}
static uint16_t rows(MenuLayer *menu, uint16_t section, void *context) { return s_count ? s_count : 1; }
static int16_t height(MenuLayer *menu, MenuIndex *index, void *context) { return ui_large_text() ? 88 : 70; }
static void draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  menu_cell_basic_draw(ctx, cell, s_count ? s_names[index->row] : "No calendars received",
    s_count ? group_name(calendar_group(s_names[index->row])) : NULL, NULL);
}
static void chosen(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  CalendarGroup group = (CalendarGroup)(intptr_t)action_menu_item_get_action_data(item);
  uint64_t hash = calendar_name_hash(s_names[s_chosen]); int slot = -1;
  for (int i = 0; i < EVENT_LIMIT; ++i) {
    if (s_preferences[i].hash == hash) { slot = i; break; }
    if (slot < 0 && !s_preferences[i].hash) slot = i;
  }
  if (slot < 0) return;
  GroupPreference preference; memset(&preference, 0, sizeof(preference)); preference.version = GROUP_VERSION; preference.hash = hash; preference.group = group; preference.checksum = checksum(&preference);
  if (persist_write_data(GROUP_BASE + slot, &preference, sizeof(preference)) != (int)sizeof(preference)) return;
  memcpy(&s_preferences[slot], &preference, sizeof(preference)); calendar_group_restore(hash, group);
  if (s_changed) s_changed();
}
static void closed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  action_menu_hierarchy_destroy(s_root, NULL, NULL); s_root = NULL; s_actions = NULL;
  if (s_menu) menu_layer_reload_data(s_menu);
}
static void select_group(MenuLayer *menu, MenuIndex *index, void *context) {
  if (!s_count || s_actions) return;
  s_chosen = index->row; s_root = action_menu_level_create(3);
  action_menu_level_add_action(s_root, "Work", chosen, (void *)(intptr_t)CalendarGroupWork);
  action_menu_level_add_action(s_root, "Personal", chosen, (void *)(intptr_t)CalendarGroupPersonal);
  action_menu_level_add_action(s_root, "Other", chosen, (void *)(intptr_t)CalendarGroupOther);
  ActionMenuConfig config = {.root_level = s_root, .did_close = closed, .colors = {.background = GColorCobaltBlue, .foreground = GColorWhite}};
  s_actions = action_menu_open(&config);
}
static void load(Window *window) {
  s_menu = menu_layer_create(layer_get_bounds(window_get_root_layer(window)));
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack); menu_layer_set_highlight_colors(s_menu, GColorCobaltBlue, GColorWhite);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){.get_num_rows = rows, .get_cell_height = height, .draw_row = draw, .select_click = select_group});
  layer_add_child(window_get_root_layer(window), menu_layer_get_layer(s_menu)); menu_layer_set_click_config_onto_window(s_menu, window);
}
static void unload(Window *window) { menu_layer_destroy(s_menu); s_menu = NULL; memset(s_names, 0, sizeof(s_names)); s_count = 0; }
void calendar_groups_init(void (*changed)(void)) {
  s_changed = changed; calendar_groups_reset();
  for (int i = 0; i < EVENT_LIMIT; ++i) {
    GroupPreference *preference = &s_preferences[i];
    if (persist_read_data(GROUP_BASE + i, preference, sizeof(*preference)) != (int)sizeof(*preference) ||
      preference->version != GROUP_VERSION || preference->checksum != checksum(preference) || preference->group < 0 || preference->group > CalendarGroupPersonal || !calendar_group_restore(preference->hash, preference->group)) {
      memset(preference, 0, sizeof(*preference)); persist_delete(GROUP_BASE + i);
    }
  }
  s_window = window_create(); window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){.load = load, .unload = unload});
}
void calendar_groups_open(const CalendarSnapshot *snapshot) {
  memset(s_names, 0, sizeof(s_names)); s_count = 0;
  for (int i = 0; i < snapshot->count; ++i) {
    const char *name = snapshot->events[i].calendar; if (!name[0]) continue;
    bool duplicate = false;
    for (int j = 0; j < s_count; ++j) if (!strcmp(name, s_names[j])) duplicate = true;
    if (!duplicate) snprintf(s_names[s_count++], sizeof(s_names[0]), "%s", name);
  }
  window_stack_push(s_window, true);
}
void calendar_groups_destroy(void) {
  if (s_actions) action_menu_close(s_actions, false);
  window_destroy(s_window); s_window = NULL; memset(s_preferences, 0, sizeof(s_preferences)); calendar_groups_reset();
}
