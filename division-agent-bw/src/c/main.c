/*
 * Division Agent — classic-hardware port of the SHD watchface: tick ring,
 * 7-segment time with seconds, date, ACTIVATED status, steps, battery and
 * weather. Orange on color displays (chalk), white-on-black on monochrome
 * (diorite / Pebble 2 SE).
 */
#include <pebble.h>

#define PERSIST_KEY_TEMP 1
#define PERSIST_KEY_UNIT 2

// Division orange where the display allows it
#define ACCENT_COLOR PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite)
#define PALE_COLOR   PBL_IF_COLOR_ELSE(GColorRajah, GColorWhite)

static Window *s_window;
static Layer *s_layer;
static GFont s_time_font;

static int s_battery_percent = -1;
static bool s_has_weather;
static int s_weather_temp;
static char s_weather_unit = 'C';

/* ---------- helpers ---------- */

static GPoint radial_point(GPoint center, int radius, int angle_deg) {
  int32_t angle = DEG_TO_TRIGANGLE(angle_deg);
  return GPoint(
    center.x + (sin_lookup(angle) * radius / TRIG_MAX_RATIO),
    center.y - (cos_lookup(angle) * radius / TRIG_MAX_RATIO));
}

static void draw_ring(GContext *ctx, GPoint c, int r) {
  // solid accent band
  graphics_context_set_fill_color(ctx, ACCENT_COLOR);
  graphics_fill_circle(ctx, c, r);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, c, r - 9);

  // black ticks cut into the band: heavy at hours, light at minutes
  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (int i = 0; i < 60; i++) {
    int angle = i * 6;
    if (i % 5 == 0) {
      graphics_context_set_stroke_width(ctx, 4);
      graphics_draw_line(ctx, radial_point(c, r - 11, angle), radial_point(c, r + 2, angle));
    } else {
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, radial_point(c, r - 4, angle), radial_point(c, r + 2, angle));
    }
  }
}

static void draw_battery(GContext *ctx, GPoint p) {
  graphics_context_set_stroke_color(ctx, ACCENT_COLOR);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, GRect(p.x - 9, p.y - 5, 17, 10), 2);
  graphics_context_set_fill_color(ctx, ACCENT_COLOR);
  graphics_fill_rect(ctx, GRect(p.x + 8, p.y - 2, 2, 4), 0, GCornerNone);
  if (s_battery_percent >= 0) {
    int bars = (s_battery_percent + 24) / 25;
    if (bars < 1) bars = 1;
    for (int i = 0; i < bars; i++)
      graphics_fill_rect(ctx, GRect(p.x - 7 + i * 4, p.y - 3, 3, 6), 0, GCornerNone);
  }
}

static void draw_sun(GContext *ctx, GPoint p) {
  graphics_context_set_fill_color(ctx, ACCENT_COLOR);
  graphics_fill_circle(ctx, p, 3);
  graphics_context_set_stroke_color(ctx, ACCENT_COLOR);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < 8; i++)
    graphics_draw_line(ctx, radial_point(p, 5, i * 45), radial_point(p, 8, i * 45));
}

static void draw_emblem(GContext *ctx, GPoint c) {
  graphics_context_set_stroke_color(ctx, ACCENT_COLOR);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, c, 12);

  // stylized SHD phoenix: body, head, fanned wings
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(c.x, c.y - 1), GPoint(c.x, c.y + 7));
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(c.x, c.y - 4), 1);
  graphics_context_set_stroke_width(ctx, 1);
  for (int s = -1; s <= 1; s += 2) {
    graphics_draw_line(ctx, GPoint(c.x + s * 2, c.y + 1), GPoint(c.x + s * 9, c.y - 5));
    graphics_draw_line(ctx, GPoint(c.x + s * 2, c.y + 3), GPoint(c.x + s * 8, c.y - 1));
    graphics_draw_line(ctx, GPoint(c.x + s * 2, c.y + 5), GPoint(c.x + s * 6, c.y + 2));
  }
}

static void draw_text_centered(GContext *ctx, const char *text, GFont font, int cx, int y, int h) {
  graphics_draw_text(ctx, text, font, GRect(cx - 70, y, 140, h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

/* ---------- main draw ---------- */

static void layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint c = grect_center_point(&bounds);
  int r = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 2;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  draw_ring(ctx, c, r);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // vertical offsets scale with the ring radius (tuned at r=70 on diorite)
  #define SY(v) ((v) * r / 70)

  // top row: battery, emblem, temperature
  draw_battery(ctx, GPoint(c.x - SY(34), c.y - SY(36)));
  draw_emblem(ctx, GPoint(c.x, c.y - SY(42)));
  draw_sun(ctx, GPoint(c.x + SY(34), c.y - SY(36)));

  graphics_context_set_text_color(ctx, GColorWhite);
  GFont label_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  char batt[12] = "--";
  if (s_battery_percent >= 0)
    snprintf(batt, sizeof(batt), "%d%%", s_battery_percent);
  graphics_draw_text(ctx, batt, label_font, GRect(c.x - SY(34) - 24, c.y - SY(31), 48, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char temp[12] = "--C";
  if (s_has_weather)
    snprintf(temp, sizeof(temp), "%d%c", s_weather_temp, s_weather_unit);
  graphics_draw_text(ctx, temp, label_font, GRect(c.x + SY(34) - 24, c.y - SY(31), 48, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // 7-segment time with seconds (12h without leading zero)
  int hour = t->tm_hour % 12;
  if (hour == 0) hour = 12;
  char time_str[12];
  snprintf(time_str, sizeof(time_str), "%d:%02d:%02d", hour, t->tm_min, t->tm_sec);
  graphics_context_set_text_color(ctx, ACCENT_COLOR);
  draw_text_centered(ctx, time_str, s_time_font, c.x, c.y - 13, 26);

  // date 06/17/19
  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%02d/%02d/%02d",
           t->tm_mon + 1, t->tm_mday, (t->tm_year + 1900) % 100);
  graphics_context_set_text_color(ctx, PALE_COLOR);
  draw_text_centered(ctx, date_str, label_font, c.x, c.y + SY(16), 16);

  graphics_context_set_text_color(ctx, GColorWhite);
  draw_text_centered(ctx, "ACTIVATED", label_font, c.x, c.y + SY(31), 16);

  char steps_str[24] = "-- STEPS";
  HealthValue steps = health_service_sum_today(HealthMetricStepCount);
  if (steps >= 0)
    snprintf(steps_str, sizeof(steps_str), "%d STEPS", (int)steps);
  draw_text_centered(ctx, steps_str, fonts_get_system_font(FONT_KEY_GOTHIC_14), c.x, c.y + SY(45), 16);

  #undef SY
}

/* ---------- services ---------- */

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  layer_mark_dirty(s_layer);
}

static void battery_handler(BatteryChargeState state) {
  s_battery_percent = state.charge_percent;
  layer_mark_dirty(s_layer);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *temp = dict_find(iter, MESSAGE_KEY_WeatherTemp);
  Tuple *unit = dict_find(iter, MESSAGE_KEY_WeatherUnit);
  if (temp) {
    s_weather_temp = temp->value->int32;
    s_has_weather = true;
    if (unit && unit->value->cstring[0])
      s_weather_unit = unit->value->cstring[0];
    persist_write_int(PERSIST_KEY_TEMP, s_weather_temp);
    persist_write_int(PERSIST_KEY_UNIT, s_weather_unit);
    layer_mark_dirty(s_layer);
  }
}

/* ---------- window lifecycle ---------- */

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, layer_update);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
}

static void init(void) {
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG7_22));

  if (persist_exists(PERSIST_KEY_TEMP)) {
    s_weather_temp = persist_read_int(PERSIST_KEY_TEMP);
    s_weather_unit = (char)persist_read_int(PERSIST_KEY_UNIT);
    s_has_weather = true;
  }

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
  fonts_unload_custom_font(s_time_font);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
