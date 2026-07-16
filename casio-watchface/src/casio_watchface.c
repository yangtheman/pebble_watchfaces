#include <pebble.h>

// Window and layers
static Window *s_main_window;
static Layer *s_canvas_layer;

// Time tracking
static struct tm s_last_time;

// Helper function to convert degrees to radians
static int32_t degrees_to_angle(int degrees) {
  return TRIG_MAX_ANGLE * degrees / 360;
}

// Helper function to draw a line from center at a given angle and length
static void draw_hand(GContext *ctx, GPoint center, int32_t angle, int length, int width) {
  GPoint end_point = {
    .x = center.x + (sin_lookup(angle) * length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(angle) * length / TRIG_MAX_RATIO)
  };
  
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, center, end_point);
}

// Helper function to draw hour markers
static void draw_hour_markers(GContext *ctx, GPoint center, int radius) {
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  
  // Draw markers at 12, 3, 6, 9
  for (int hour = 0; hour < 12; hour += 3) {
    int32_t angle = degrees_to_angle(hour * 30);
    int marker_length = 12;
    
    GPoint outer = {
      .x = center.x + (sin_lookup(angle) * radius / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * radius / TRIG_MAX_RATIO)
    };
    
    GPoint inner = {
      .x = center.x + (sin_lookup(angle) * (radius - marker_length) / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * (radius - marker_length) / TRIG_MAX_RATIO)
    };
    
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx, inner, outer);
  }
}

// Helper function to draw a subdial
static void draw_subdial(GContext *ctx, GPoint center, int radius, int value, int max_value) {
  // Draw subdial circle
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, radius);
  
  // Draw subdial hand
  int32_t angle = TRIG_MAX_ANGLE * value / max_value;
  int hand_length = radius - 5;
  
  GPoint end_point = {
    .x = center.x + (sin_lookup(angle) * hand_length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(angle) * hand_length / TRIG_MAX_RATIO)
  };
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, center, end_point);
  
  // Draw center dot
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_circle(ctx, center, 2);
}

// Helper function to draw day of week subdial
static void draw_day_subdial(GContext *ctx, GPoint center, int radius, int day_num) {
  // Draw subdial circle
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, radius);
  
  // Draw day letters (S M T W T F S)
  const char *days[] = {"S", "M", "T", "W", "T", "F", "S"};
  graphics_context_set_text_color(ctx, GColorLightGray);
  
  for (int i = 0; i < 7; i++) {
    int32_t angle = degrees_to_angle(i * 360 / 7 - 90);
    int label_radius = radius - 8;
    
    GPoint label_pos = {
      .x = center.x + (cos_lookup(angle) * label_radius / TRIG_MAX_RATIO) - 4,
      .y = center.y + (sin_lookup(angle) * label_radius / TRIG_MAX_RATIO) - 7
    };
    
    if (i == day_num) {
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, GColorLightGray);
    }
    
    graphics_draw_text(ctx, days[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                      GRect(label_pos.x, label_pos.y, 10, 14),
                      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  
  // Draw hand pointing to current day
  int32_t angle = TRIG_MAX_ANGLE * day_num / 7;
  int hand_length = radius - 12;
  
  GPoint end_point = {
    .x = center.x + (sin_lookup(angle) * hand_length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(angle) * hand_length / TRIG_MAX_RATIO)
  };
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, center, end_point);
  
  // Draw center dot
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_circle(ctx, center, 2);
}

// Helper function to draw moon phase
static void draw_moon_phase(GContext *ctx, GPoint center, int radius) {
  // Draw dark background circle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, radius);
  
  // Draw a few stars
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(center.x - 8, center.y - 5), 1);
  graphics_fill_circle(ctx, GPoint(center.x - 3, center.y - 8), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 6, center.y - 6), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 4, center.y - 2), 1);
  
  // Draw moon (simplified - always show as crescent for visual interest)
  GPoint moon_center = GPoint(center.x, center.y + 2);
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_circle(ctx, moon_center, 7);
  
  // Draw shadow to create crescent effect
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(moon_center.x + 3, moon_center.y), 6);
}

// Canvas update procedure - this is where all drawing happens
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  
  // Fill background with black
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Calculate main dial radius
  int main_radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 20;
  
  // Draw hour markers
  draw_hour_markers(ctx, center, main_radius);
  
  // Draw "12" at the top
  graphics_context_set_text_color(ctx, GColorLightGray);
  GRect twelve_rect = GRect(center.x - 15, center.y - main_radius + 5, 30, 25);
  graphics_draw_text(ctx, "12", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                    twelve_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw "CASIO" brand
  GRect casio_rect = GRect(center.x - 25, center.y - main_radius / 3, 50, 20);
  graphics_draw_text(ctx, "CASIO", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                    casio_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Subdial positions and sizes
  int subdial_radius = main_radius / 4;
  int subdial_y_offset = center.y + main_radius / 7;
  
  // Left subdial - Day of week
  GPoint left_subdial = GPoint(center.x - main_radius / 2, subdial_y_offset);
  draw_day_subdial(ctx, left_subdial, subdial_radius, s_last_time.tm_wday);
  
  // Right subdial - Seconds
  GPoint right_subdial = GPoint(center.x + main_radius / 2, subdial_y_offset);
  draw_subdial(ctx, right_subdial, subdial_radius, s_last_time.tm_sec, 60);
  
  // Center moon phase
  int moon_radius = subdial_radius - 3;
  GPoint moon_center = GPoint(center.x, subdial_y_offset);
  draw_moon_phase(ctx, moon_center, moon_radius);
  
  // Draw "WATER RESIST 50M" text
  graphics_context_set_text_color(ctx, GColorLightGray);
  GRect water_rect1 = GRect(center.x - 40, center.y + main_radius / 2, 80, 14);
  graphics_draw_text(ctx, "WATER RESIST", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    water_rect1, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  GRect water_rect2 = GRect(center.x - 40, center.y + main_radius / 2 + 12, 80, 14);
  graphics_draw_text(ctx, "50M", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    water_rect2, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw "JAPAN MOVT" at bottom
  graphics_context_set_text_color(ctx, GColorDarkGray);
  GRect japan_rect = GRect(center.x - 30, center.y + main_radius - 15, 25, 14);
  graphics_draw_text(ctx, "JAPAN", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    japan_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  
  GRect movt_rect = GRect(center.x + 5, center.y + main_radius - 15, 25, 14);
  graphics_draw_text(ctx, "MOVT", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    movt_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  
  // Calculate hand angles
  int hour = s_last_time.tm_hour % 12;
  int minute = s_last_time.tm_min;
  int second = s_last_time.tm_sec;
  
  int32_t second_angle = degrees_to_angle(second * 6);
  int32_t minute_angle = degrees_to_angle(minute * 6 + second / 10);
  int32_t hour_angle = degrees_to_angle(hour * 30 + minute / 2);
  
  // Draw hour hand
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  draw_hand(ctx, center, hour_angle, main_radius * 45 / 100, 4);
  
  // Draw minute hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  draw_hand(ctx, center, minute_angle, main_radius * 65 / 100, 3);
  
  // Draw second hand (thin)
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  draw_hand(ctx, center, second_angle, main_radius * 70 / 100, 1);
  
  // Draw center cap
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_circle(ctx, center, 5);
  
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_circle(ctx, center, 3);
}

// Update time and request redraw
static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  s_last_time = *tick_time;
  
  // Redraw the canvas
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// Tick handler - called every second
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

// Window load handler
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create canvas layer for custom drawing
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
  
  // Initial time update
  update_time();
}

// Window unload handler
static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

// Initialize the app
static void init() {
  // Create main window
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  
  // Set window handlers
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // Show the window
  window_stack_push(s_main_window, true);
  
  // Subscribe to time updates (every second for smooth second hand)
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

// Deinitialize the app
static void deinit() {
  window_destroy(s_main_window);
}

// Main function
int main(void) {
  init();
  app_event_loop();
  deinit();
}
