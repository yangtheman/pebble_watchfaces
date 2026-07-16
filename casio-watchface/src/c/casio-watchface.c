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

// Helper function to draw a pointed hand (like the reference watch)
static void draw_pointed_hand(GContext *ctx, GPoint center, int32_t angle, int length, int base_width, GColor color) {
  // Calculate end point
  GPoint tip = {
    .x = center.x + (sin_lookup(angle) * length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(angle) * length / TRIG_MAX_RATIO)
  };
  
  // Calculate perpendicular points for the base
  int32_t perp_angle = angle + TRIG_MAX_ANGLE / 4;
  int half_width = base_width / 2;
  
  GPoint base1 = {
    .x = center.x + (sin_lookup(perp_angle) * half_width / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(perp_angle) * half_width / TRIG_MAX_RATIO)
  };
  
  GPoint base2 = {
    .x = center.x - (sin_lookup(perp_angle) * half_width / TRIG_MAX_RATIO),
    .y = center.y + (cos_lookup(perp_angle) * half_width / TRIG_MAX_RATIO)
  };
  
  // Draw the triangular hand
  GPathInfo hand_path_info = {
    .num_points = 3,
    .points = (GPoint[]) {tip, base1, base2}
  };
  
  GPath *hand_path = gpath_create(&hand_path_info);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, hand_path);
  graphics_context_set_stroke_color(ctx, color);
  gpath_draw_outline(ctx, hand_path);
  gpath_destroy(hand_path);
}

// Helper function to draw hour markers at all 12 positions
static void draw_hour_markers(GContext *ctx, GPoint center, int outer_radius) {
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  
  for (int hour = 0; hour < 12; hour++) {
    // Skip 12 o'clock position (we'll draw "12" text there)
    if (hour == 0) continue;
    
    int32_t angle = degrees_to_angle(hour * 30);
    int marker_length = 10;
    int inner_radius = outer_radius - marker_length;
    
    GPoint outer = {
      .x = center.x + (sin_lookup(angle) * outer_radius / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * outer_radius / TRIG_MAX_RATIO)
    };
    
    GPoint inner = {
      .x = center.x + (sin_lookup(angle) * inner_radius / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * inner_radius / TRIG_MAX_RATIO)
    };
    
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, inner, outer);
  }
}

// Helper function to draw day of week subdial (left side)
static void draw_day_subdial(GContext *ctx, GPoint center, int radius, int day_num) {
  // Draw subdial circle with white/light border
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, radius);
  
  // Draw day letters around the dial: S M T W T F S
  // Arranged like a clock face, starting from top-left area
  const char *days[] = {"S", "M", "T", "W", "T", "F", "S"};
  
  for (int i = 0; i < 7; i++) {
    // Position days around the circle - S at top, going clockwise
    // Spread across roughly 300 degrees (not full circle) for readability
    int32_t angle = degrees_to_angle(-120 + i * 40); // Start from ~10 o'clock position
    int label_radius = radius - 6;
    
    GPoint label_pos = {
      .x = center.x + (sin_lookup(angle) * label_radius / TRIG_MAX_RATIO) - 3,
      .y = center.y - (cos_lookup(angle) * label_radius / TRIG_MAX_RATIO) - 6
    };
    
    // Highlight current day
    if (i == day_num) {
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, GColorLightGray);
    }
    
    graphics_draw_text(ctx, days[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                      GRect(label_pos.x, label_pos.y, 10, 14),
                      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
  
  // Draw hand pointing to current day
  int32_t hand_angle = degrees_to_angle(-120 + day_num * 40);
  int hand_length = radius - 8;
  
  GPoint end_point = {
    .x = center.x + (sin_lookup(hand_angle) * hand_length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(hand_angle) * hand_length / TRIG_MAX_RATIO)
  };
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, end_point);
  
  // Draw center pin
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 2);
}

// Helper function to draw seconds subdial (right side - 60 second ticker)
static void draw_seconds_subdial(GContext *ctx, GPoint center, int radius, int seconds) {
  // Draw subdial circle
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, radius);
  
  // Draw tick marks around the dial
  for (int i = 0; i < 12; i++) {
    int32_t angle = degrees_to_angle(i * 30);
    int tick_outer = radius - 2;
    int tick_inner = radius - 4;
    
    GPoint outer = {
      .x = center.x + (sin_lookup(angle) * tick_outer / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * tick_outer / TRIG_MAX_RATIO)
    };
    
    GPoint inner = {
      .x = center.x + (sin_lookup(angle) * tick_inner / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * tick_inner / TRIG_MAX_RATIO)
    };
    
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_line(ctx, inner, outer);
  }
  
  // Draw numbers 10, 20, 30 at their positions
  // 10 at roughly 2 o'clock (60 degrees), 20 at 6 o'clock (180 degrees), 30 at 10 o'clock (300 degrees)
  graphics_context_set_text_color(ctx, GColorWhite);
  
  // "30" at top (12 o'clock position)
  graphics_draw_text(ctx, "30", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    GRect(center.x - 7, center.y - radius + 4, 14, 14),
                    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // "10" at bottom right (4 o'clock position)
  graphics_draw_text(ctx, "10", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    GRect(center.x + 4, center.y + 4, 14, 14),
                    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // "20" at bottom left (8 o'clock position)
  graphics_draw_text(ctx, "20", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    GRect(center.x - 16, center.y + 4, 14, 14),
                    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw seconds hand - full rotation = 60 seconds
  int32_t hand_angle = degrees_to_angle(seconds * 6); // 360/60 = 6 degrees per second
  int hand_length = radius - 5;
  
  GPoint end_point = {
    .x = center.x + (sin_lookup(hand_angle) * hand_length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(hand_angle) * hand_length / TRIG_MAX_RATIO)
  };
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, end_point);
  
  // Draw center pin
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 2);
}

// Helper function to draw moon phase window (semicircle at bottom)
static void draw_moon_phase(GContext *ctx, GPoint center, int width, int height) {
  // Draw the semicircle window background (dark blue/black with stars)
  GRect moon_rect = GRect(center.x - width/2, center.y - height/2, width, height);
  
  // Fill with dark background
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, moon_rect, 0, GCornerNone);
  
  // Draw border
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, moon_rect);
  
  // Draw stars
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(center.x + 8, center.y - 3), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 12, center.y + 2), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 5, center.y + 4), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 15, center.y - 1), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 2, center.y - 2), 1);
  
  // Draw crescent moon on left side
  GPoint moon_center = GPoint(center.x - 6, center.y);
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_circle(ctx, moon_center, 6);
  
  // Create crescent by overlapping with dark circle
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_circle(ctx, GPoint(moon_center.x + 3, moon_center.y), 5);
}

// Canvas update procedure - this is where all drawing happens
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  
  // Fill background with dark gray (like the watch face)
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Calculate main dial radius - use most of the screen
  int main_radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 5;
  
  // Draw hour markers at all 12 positions
  draw_hour_markers(ctx, center, main_radius - 3);
  
  // Draw "12" at the top - large and prominent
  graphics_context_set_text_color(ctx, GColorLightGray);
  GRect twelve_rect = GRect(center.x - 12, center.y - main_radius + 2, 24, 24);
  graphics_draw_text(ctx, "12", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                    twelve_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw "CASIO" brand - below 12, above center
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect casio_rect = GRect(center.x - 25, center.y - main_radius + 26, 50, 16);
  graphics_draw_text(ctx, "CASIO", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                    casio_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Subdial sizing and positioning
  int subdial_radius = 18;
  int subdial_y = center.y + 5;
  int subdial_x_offset = 32;
  
  // Left subdial - Day of week
  GPoint left_subdial = GPoint(center.x - subdial_x_offset, subdial_y);
  draw_day_subdial(ctx, left_subdial, subdial_radius, s_last_time.tm_wday);
  
  // Right subdial - Seconds (showing 10, 20, 30)
  GPoint right_subdial = GPoint(center.x + subdial_x_offset, subdial_y);
  draw_seconds_subdial(ctx, right_subdial, subdial_radius, s_last_time.tm_sec);
  
  // Moon phase - centered below the subdials
  GPoint moon_center = GPoint(center.x, center.y + 30);
  draw_moon_phase(ctx, moon_center, 40, 16);
  
  // Draw "WATER RESIST" and "50M" text below moon
  graphics_context_set_text_color(ctx, GColorLightGray);
  GRect water_rect = GRect(center.x - 35, center.y + 42, 70, 12);
  graphics_draw_text(ctx, "WATER RESIST", fonts_get_system_font(FONT_KEY_GOTHIC_09),
                    water_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  GRect fifty_rect = GRect(center.x - 15, center.y + 52, 30, 12);
  graphics_draw_text(ctx, "50M", fonts_get_system_font(FONT_KEY_GOTHIC_09),
                    fifty_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw "JAPAN" and "MOV'T" at bottom
  graphics_context_set_text_color(ctx, GColorDarkGray);
  GRect japan_rect = GRect(center.x - 30, center.y + main_radius - 18, 25, 12);
  graphics_draw_text(ctx, "JAPAN", fonts_get_system_font(FONT_KEY_GOTHIC_09),
                    japan_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  GRect movt_rect = GRect(center.x + 5, center.y + main_radius - 18, 25, 12);
  graphics_draw_text(ctx, "MOV'T", fonts_get_system_font(FONT_KEY_GOTHIC_09),
                    movt_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Calculate hand angles
  int hour = s_last_time.tm_hour % 12;
  int minute = s_last_time.tm_min;
  int second = s_last_time.tm_sec;
  
  int32_t minute_angle = degrees_to_angle(minute * 6 + second / 10);
  int32_t hour_angle = degrees_to_angle(hour * 30 + minute / 2);
  
  // Draw hour hand - shorter and wider
  draw_pointed_hand(ctx, center, hour_angle, main_radius * 40 / 100, 8, GColorLightGray);
  
  // Draw minute hand - longer and thinner
  draw_pointed_hand(ctx, center, minute_angle, main_radius * 60 / 100, 6, GColorLightGray);
  
  // Draw center cap
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 4);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_circle(ctx, center, 2);
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
