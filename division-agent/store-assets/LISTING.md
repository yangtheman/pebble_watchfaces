# Store Listing — Division Agent (Alloy: Pebble Time 2 / Round 2)

**App type:** Watchface
**Title:** Division Agent
**Author:** yangtheman
**Category:** Faces
**Compatible platforms:** emery (Pebble Time 2), gabbro (Pebble Round 2)
**Website / Source:** (optional — add your repo URL if you publish the code)

## Description

Tactical agent watchface inspired by The Division's SHD tech aesthetic.

Features:
- Orange bezel tick ring with hour and minute marks
- Big 7-segment digital time with live seconds
- Date, agent status, and daily step count
- Battery meter with charge segments
- Current temperature via Open-Meteo (no API key or account needed)
- Settings page: Celsius/Fahrenheit, phone GPS or a manual "City, State" location

Built with Pebble's new Alloy (JavaScript) SDK.

## Release notes (v1.0.1)

Fixed a display bug where the ACTIVATED status and step count could be
clipped by the tick ring for longer strings (e.g. higher step counts) —
they now stay clear of the ring on every screen size. Weather now refreshes
immediately on launch and retries automatically instead of waiting on the
next scheduled update.

(v1.0.0: initial release — time with seconds, date, steps, battery, weather
with configurable units and location.)

## Files to upload

- division-face.pbw
- screenshot-emery.png  (rectangular screenshot — Pebble Time 2)
- screenshot-gabbro.png (round screenshot — Pebble Round 2)
