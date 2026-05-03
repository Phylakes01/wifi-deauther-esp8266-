/*
 * Eyes.h - Header for eyes (emoji) functionality on ESP8266 Deauther.
 *
 * This module encapsulates the setup and runtime handling for an
 * animated pair of eyes displayed on the SSD1306 OLED.  When
 * enabled, it takes over the display and runs its own update
 * loop while the rest of the deauther UI is suspended.  A
 * momentary press on the configured button will cycle through
 * predefined emotions; a long press exits the eyes mode and
 * returns control to the regular menu UI.
 */

#ifndef EYES_H
#define EYES_H

#include <Arduino.h>

/*
 * Global flag indicating whether the device is currently in
 * eyes/emoji mode.  When true, the main loop will call
 * eyesLoop() instead of the normal deauther tasks.  When
 * eyesLoop() detects a long button press, it will set this
 * flag back to false to exit the mode.
 */
extern bool eyesMode;

/*
 * Perform initialisation for the eyes display.  This sets up
 * the underlying OLED and RoboEyes objects, configures the
 * autoblinker and idle animation, and selects a default emotion.
 * Call this from setup() once the regular display is ready.
 */
void eyesSetup();

/*
 * Update the eyes animation.  This function should be called
 * repeatedly while eyesMode is true.  It handles drawing the
 * eyes, cycling between emotions on short button presses and
 * exiting eyes mode on a long press.
 */
void eyesLoop();

#endif /* EYES_H */