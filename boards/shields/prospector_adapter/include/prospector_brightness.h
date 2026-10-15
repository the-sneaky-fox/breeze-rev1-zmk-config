#pragma once

#include <stdint.h>

/*
 * Step the display backlight brightness by `delta` percentage points,
 * clamped to [1, 100]. Returns the new brightness value.
 */
uint8_t prospector_brightness_step(int8_t delta);

/* Current backlight brightness (1..100). */
uint8_t prospector_brightness_get(void);
