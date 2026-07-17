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

// In division-agent's drawEmblem(), the 3 stroked lines per wing overlap
// into a solid wedge — but that relies on line width being large relative
// to line spacing, and at this ring's smaller scale, width only rounds to
// 1 (too thin, wings stay separate spikes) or 2 (too thick, wings bleed
// into the ring). A traced silhouette of the exact merged shape hits the
// same wall from the other direction: once scaled down and rounded to this
// platform's integer coordinates, distinct points collapse together and
// the polygon self-intersects, which gpath_draw_filled renders as garbage.
// Filling a single simple (non-self-intersecting) wedge per wing side —
// the same 4 corner points, minus the middle line's inner detail — is the
// version of that shape that survives small-scale integer rounding.
static const GPathInfo WING_R_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint[]) { {1, 1}, {9, -6}, {6, 2}, {1, 5} }
};
static const GPathInfo WING_L_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint[]) { {-1, 1}, {-9, -6}, {-6, 2}, {-1, 5} }
};

static void draw_emblem(GContext *ctx, GPoint c) {
  graphics_context_set_stroke_color(ctx, ACCENT_COLOR);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, c, 12);

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(c.x, c.y - 4), 1);
  graphics_draw_line(ctx, GPoint(c.x, c.y - 1), GPoint(c.x, c.y + 6));

  GPath *wing_r = gpath_create(&WING_R_PATH_INFO);
  gpath_move_to(wing_r, c);
  gpath_draw_filled(ctx, wing_r);
  gpath_destroy(wing_r);

  GPath *wing_l = gpath_create(&WING_L_PATH_INFO);
  gpath_move_to(wing_l, c);
  gpath_draw_filled(ctx, wing_l);
  gpath_destroy(wing_l);
}

static void draw_text_centered(GContext *ctx, const char *text, GFont font, int cx, int y, int h, int w) {
  graphics_draw_text(ctx, text, font, GRect(cx - w / 2, y, w, h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static int text_width(const char *text, GFont font) {
  GSize size = graphics_text_layout_get_content_size(text, font, GRect(0, 0, 200, 30),
                                                     GTextOverflowModeFill, GTextAlignmentLeft);
  return size.w;
}

static int isqrt(int64_t v) {
  if (v <= 0) return 0;
  int64_t x = v, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + v / x) / 2; }
  return (int)x;
}

// Largest distance from center, along y, at which a chord of the given
// half-width still clears the ring: the black face has radius r-9, and the
// heavy hour ticks cut 2px further in than that, so keep a small buffer.
static int max_dy_for_halfwidth(int r, int half_w) {
  int safe_r = r - 13;
  if (safe_r <= 0) return 0;
  int64_t sr2 = (int64_t)safe_r * safe_r;
  int64_t hw2 = (int64_t)half_w * half_w;
  if (hw2 >= sr2) return 0;
  return isqrt(sr2 - hw2);
}

// Width of the widest chord available at a given distance dy from center,
// capped at `max_w` — used to size the ellipsis box once dy is finalized.
static int width_at_dy(int r, int dy, int max_w) {
  int safe_r = r - 13;
  int64_t sr2 = (int64_t)safe_r * safe_r;
  int64_t dy2 = (int64_t)dy * dy;
  if (dy2 >= sr2) return 0;
  int w = 2 * isqrt(sr2 - dy2);
  return w < max_w ? w : max_w;
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
  draw_text_centered(ctx, time_str, s_time_font, c.x, c.y - 13, 26, 140);

  // date 06/17/19
  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%02d/%02d/%02d",
           t->tm_mon + 1, t->tm_mday, (t->tm_year + 1900) % 100);
  graphics_context_set_text_color(ctx, PALE_COLOR);
  draw_text_centered(ctx, date_str, label_font, c.x, c.y + SY(9), 16, 140);

  // ACTIVATED and STEPS sit close to the ring, where the circle's chord is
  // narrower than the ring-tuned 140px box; measure each string and pull it
  // toward center (and shrink its box) just enough to clear the tick marks,
  // rather than letting the outer glyphs get overdrawn by the ring. The
  // 16px box is top-aligned but the glyphs only ink roughly its top third,
  // so the point that actually has to clear the ring is dy + INK_H, not
  // the box's full height — checking the full height starved the rows of
  // room and pushed STEPS into ACTIVATED.
  #define ROW_H 16
  #define INK_H 6
  graphics_context_set_text_color(ctx, GColorWhite);
  const char *activated_str = "ACTIVATED";
  int activated_dy = SY(23);
  int activated_half = text_width(activated_str, label_font) / 2 + 3;
  int activated_max_dy = max_dy_for_halfwidth(r, activated_half) - INK_H;
  if (activated_dy > activated_max_dy) activated_dy = activated_max_dy;
  draw_text_centered(ctx, activated_str, label_font, c.x, c.y + activated_dy, ROW_H,
                     width_at_dy(r, activated_dy + INK_H, 140));

  char steps_str[24] = "-- STEPS";
  HealthValue steps = health_service_sum_today(HealthMetricStepCount);
  if (steps >= 0)
    snprintf(steps_str, sizeof(steps_str), "%d STEPS", (int)steps);
  GFont steps_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int steps_dy = SY(36);
  int steps_half = text_width(steps_str, steps_font) / 2 + 3;
  int steps_max_dy = max_dy_for_halfwidth(r, steps_half) - INK_H;
  if (steps_dy > steps_max_dy) steps_dy = steps_max_dy;
  int steps_min_dy = activated_dy + SY(14);
  if (steps_dy < steps_min_dy) steps_dy = steps_min_dy;
  // the ring boundary wins over the from-ACTIVATED spacing floor: bumping
  // up for spacing must not push the row back past where it still clears
  // the ring, or width_at_dy below goes to 0 and the row vanishes entirely.
  if (steps_dy > steps_max_dy) steps_dy = steps_max_dy;
  draw_text_centered(ctx, steps_str, steps_font, c.x, c.y + steps_dy, ROW_H,
                     width_at_dy(r, steps_dy + INK_H, 140));
  #undef ROW_H
  #undef INK_H

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
