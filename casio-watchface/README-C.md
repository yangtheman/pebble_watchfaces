# Casio-Inspired Pebble Watchface (C Version)

A Pebble watchface written in C that's inspired by the Casio analog watch design, featuring:

- **Analog time display** with hour, minute, and second hands
- **Day of week subdial** on the left with letter indicators
- **Seconds subdial** on the right  
- **Moon phase indicator** in the center with stars
- **Clean design** with "CASIO" branding and water resistance markings
- **Hour markers** at 12, 3, 6, and 9 positions

## Features

### Main Display
- Large "12" numeral at top
- "CASIO" branding below the 12
- Silver/gray colored hands on black background
- Smooth second hand movement
- Custom-drawn graphics using Pebble's graphics API

### Subdials
1. **Left Subdial**: Day of the week with letter indicators (S M T W T F S)
2. **Right Subdial**: Running seconds display
3. **Center**: Moon phase visualization with stars and crescent moon

### Design Elements
- "WATER RESIST 50M" text in middle
- "JAPAN MOVT" text at bottom
- Minimalist hour markers

## Installation

### Prerequisites
1. Install the Pebble SDK and Pebble Tool:
   ```bash
   pip install pebble-tool
   ```

2. Verify you have SDK 3.x or 4.x installed:
   ```bash
   pebble sdk list
   ```

### Setup

1. Create a new Pebble project:
   ```bash
   pebble new-project casio-watchface
   cd casio-watchface
   ```

2. Replace the contents of `src/casio-watchface.c` (or your main .c file) with the code from `casio_watchface.c`

3. Update `package.json` with the provided configuration from `package-c.json`

### Build and Install

1. Build the watchface:
   ```bash
   pebble build
   ```

2. Install on emulator:
   ```bash
   pebble install --emulator basalt
   ```

3. Install on physical device (via phone):
   ```bash
   pebble install --phone <phone_ip>
   ```

## Customization

You can customize various aspects of the watchface by modifying the C code:

### Colors
The watchface uses Pebble's color constants:
- `GColorBlack` - Background
- `GColorWhite` - Main elements
- `GColorLightGray` - Secondary elements
- `GColorDarkGray` - Tertiary elements
- `GColorYellow` - Moon

Change these in the `canvas_update_proc()` function.

### Sizes
Adjust these in the drawing code:
- `main_radius` - Overall size of the watch face
- `subdial_radius` - Size of the subdials
- Hand lengths (e.g., `main_radius * 65 / 100` for minute hand)

### Update Frequency
The watchface updates every second for smooth second hand movement. To save battery:

Change this line in `init()`:
```c
tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
```
to:
```c
tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
```

You may also want to remove the second hand drawing code in `canvas_update_proc()`.

## Compatibility

This watchface works on:
- ✅ Pebble (aplite) - Black & white
- ✅ Pebble Time (basalt) - Color
- ✅ Pebble Time Round (chalk) - Color, round
- ✅ Pebble Time Steel (basalt) - Color
- ✅ Pebble 2 (diorite) - Black & white

Works with SDK 3.x and 4.x (C-based SDKs).

## Technical Details

The watchface uses:
- **Layer with custom update proc** - For drawing all graphics
- **Graphics Context API** - For drawing lines, circles, and text
- **TickTimerService** - For time updates
- **Trigonometry functions** - For calculating hand positions (sin_lookup, cos_lookup)
- **System fonts** - GOTHIC_14, GOTHIC_14_BOLD, GOTHIC_24_BOLD

### Key Functions
- `canvas_update_proc()` - Main drawing function, called whenever display needs updating
- `draw_hand()` - Draws clock hands using trigonometry
- `draw_subdial()` - Draws a subdial with hand
- `draw_day_subdial()` - Draws day of week subdial with letters
- `draw_moon_phase()` - Draws moon phase visualization

## Battery Usage

The watchface updates every second to show smooth animation. Expected battery impact:
- Updates per day: 86,400
- Relatively low impact due to efficient C code
- To extend battery life, switch to MINUTE_UNIT updates

## Credits

Inspired by the Casio analog watch design, implemented in C using the Pebble SDK.

## License

Free to use and modify for personal projects.
