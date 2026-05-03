/*
 * Eyes.cpp - Implementation for eyes (emoji) functionality on
 * ESP8266 Deauther.
 *
 * This module uses the FluxGarage RoboEyes library to render a
 * pair of animated eyes on the SSD1306 display.  It is designed
 * to run independently of the regular DisplayUI; when eyesMode
 * is true the main loop bypasses the normal UI and calls
 * eyesLoop(), which updates the animation and responds to
 * button presses.  A short press cycles through a set of
 * emotions, while a long press exits eyes mode.
 */

#include "Eyes.h"

#include <Wire.h>
#include <Adafruit_SSD1306.h>

// Arduino cores (e.g. esp8266) define a macro named DEFAULT.  The
// RoboEyes library also defines its own DEFAULT macro for moods.
// Undefine any existing DEFAULT before including RoboEyes to avoid
// redefinition warnings.
#ifdef DEFAULT
#undef DEFAULT
#endif

#include <FluxGarage_RoboEyes.h>

// I2C pins for the OLED display on the ESP8266 Deauther.
// These definitions mirror those used by the stock DisplayUI.
// If you have customised your wiring, adjust SDA_PIN and
// SCL_PIN accordingly.
#define SDA_PIN D2
#define SCL_PIN D1

// GPIO pin used for cycling emotions.  This button is configured
// with INPUT_PULLUP, so a press reads LOW.  A separate exit button
// (EXIT_BUTTON_PIN) can be used to leave eyes mode.
#define BUTTON_PIN D5

// GPIO pin used for exiting eyes mode.  Pressing this button
// immediately exits eyes/emoji mode and returns control to the
// main UI.  Also configured with INPUT_PULLUP.
#define EXIT_BUTTON_PIN D7

// Create a separate display instance for the eyes.  Although the
// standard DisplayUI also uses an SSD1306, instantiating our own
// driver allows us to take complete control of the display while
// eyesMode is active without interfering with the deauther UI.
static Adafruit_SSD1306 roboDisplay(128, 64, &Wire, -1);
static RoboEyes<Adafruit_SSD1306> roboEyes(roboDisplay);

// Flag imported from the header indicating whether eyes mode is
// currently active.  Defined in this translation unit for use in
// the loop and emotion handling.
bool eyesMode = false;

// Internal state for tracking button presses and emotion.  The
// emotion cycles through the enumeration on each short press.
enum Emotion {
  INTEREST, JOY, SURPRISE, SADNESS, ANGER,
  DISGUST, CONTEMPT, SELF_HOSTILITY,
  FEAR, SHAME, SHYNESS, GUILT,
  NUM_EMOTIONS
};

static Emotion currentEmotion = JOY;
static bool lastButtonState = HIGH;
// Timestamp of when the button was first pressed.  Used to
// differentiate short and long presses.
static unsigned long pressStart = 0;

/*
 * Apply the settings for a given emotion.  Each emotion tweaks
 * the RoboEyes mood, flicker and position to reflect a different
 * facial expression.  Emotions not explicitly handled fall
 * through to a neutral default.
 */
static void setEmotion(Emotion e) {
  // Reset to a neutral baseline before applying specific traits.
  roboEyes.setHFlicker(OFF, 0);
  roboEyes.setVFlicker(OFF, 0);
  roboEyes.setCuriosity(OFF);
  roboEyes.setHeight(36, 36);
  // Do not call setPosition(CENTER) here.  Some versions of the
  // RoboEyes library do not define a CENTER constant, causing a
  // compilation error.  Each emotion below will set its own
  // position when necessary.
  roboEyes.setMood(DEFAULT);

  switch (e) {
    case INTEREST:
      roboEyes.setMood(DEFAULT);
      roboEyes.setCuriosity(ON);
      roboEyes.setPosition(E);
      break;
    case JOY:
      roboEyes.setMood(HAPPY);
      break;
    case SURPRISE:
      roboEyes.setHeight(50, 50);
      break;
    case SADNESS:
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(S);
      break;
    case ANGER:
      roboEyes.setMood(ANGRY);
      roboEyes.setHFlicker(ON, 1);
      break;
    case DISGUST:
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(W);
      break;
    case CONTEMPT:
      roboEyes.setPosition(NE);
      break;
    case SELF_HOSTILITY:
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(S);
      break;
    case FEAR:
      roboEyes.setHeight(55, 55);
      roboEyes.setVFlicker(ON, 2);
      break;
    case SHAME:
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(S);
      break;
    case SHYNESS:
      roboEyes.setMood(HAPPY);
      roboEyes.setPosition(SE);
      break;
    case GUILT:
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(SW);
      break;
    default:
      roboEyes.setMood(DEFAULT);
      break;
  }
}

/*
 * Initialise the eyes subsystem.  Sets up the I2C bus for the
 * separate display, configures the RoboEyes animation and selects
 * the default emotion.  Must be called once from the main
 * setup() routine.
 */
void eyesSetup() {
  // Initialise I2C pins for the RoboEyes display.
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // Configure the exit button as an input with a pull-up resistor.
  pinMode(EXIT_BUTTON_PIN, INPUT_PULLUP);

  // Begin the SSD1306 display; reuse the address 0x3C as used by
  // the regular DisplayUI.  Use SWITCHCAPVCC to generate the
  // display voltage internally.  If your hardware uses a
  // different address, adjust here.
  roboDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  roboDisplay.clearDisplay();
  roboDisplay.display();

  // Initialise the RoboEyes animation.  Frame rate of 100ms per
  // update yields a smooth animation.
  roboEyes.begin(128, 64, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  // Start with a happy expression.
  setEmotion(currentEmotion);
}

/*
 * Update routine for eyes mode.  Should be called frequently
 * while eyesMode is true.  Updates the animation, checks the
 * emotion button for short and long presses and either cycles
 * emotion or exits the mode.
 */
void eyesLoop() {
  // Check if the exit button (OK button) is pressed.  A LOW
  // reading indicates a press.  Exiting immediately returns to
  // the normal UI without cycling emotions.
  if (digitalRead(EXIT_BUTTON_PIN) == LOW) {
    // Immediately exit eyes mode when the exit button is pressed.
    // Do not clear the display here; allowing the main UI update to
    // overwrite the eyes frame avoids a temporary black screen.  A
    // short debounce delay prevents the button from bouncing back
    // into eyes mode on the next loop iteration.
    eyesMode = false;
    // Do not introduce any delay here; allowing the main loop to
    // proceed immediately results in an instant return to the
    // menu without a pause.  Debouncing is handled in the
    // main loop check for eyesMode.
    return;
  }

  // Update the RoboEyes animation.  This should be called
  // regularly regardless of user input.
  roboEyes.update();

  // Read the emotion button state (LOW when pressed).
  bool buttonState = digitalRead(BUTTON_PIN);

  // Start timing when button is first pressed.
  if (buttonState == LOW && lastButtonState == HIGH) {
    pressStart = millis();
  }

  // Previously, holding the emotion button for more than 1.5 seconds
  // would exit eyes mode.  However, on some hardware the exit
  // button shares contacts with the emotion button, causing the
  // emotion to change before the exit event is processed.  To
  // prevent accidental emoji changes, the long‑press exit is
  // disabled.  Eyes mode now only exits via the dedicated
  // EXIT_BUTTON_PIN, ensuring that short and long presses on the
  // emotion button always cycle emotions and never trigger an exit.

  // On a short press (button release after less than threshold),
  // advance to the next emotion and update the display.  Only
  // execute this when the button transitions from pressed to
  // released.
  if (lastButtonState == LOW && buttonState == HIGH) {
    unsigned long pressDuration = millis() - pressStart;
    if (pressDuration < 1500) {
      // Advance to the next emotion in the enum.
      currentEmotion = static_cast<Emotion>((static_cast<int>(currentEmotion) + 1) % NUM_EMOTIONS);
      setEmotion(currentEmotion);
    }
  }

  // Store current state for next loop iteration.
  lastButtonState = buttonState;
}