#include "wled.h"
#include <Schedule.h>

/*
 * This file allows you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * bytes 2400+ are currently ununsed, but might be used for future wled features
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

// RGB LED is on D4 (see platformio_override.ini)

const int PC_POWERBUTTON_PIN = D5;
const int PC_RESETBUTTON_PIN = D6;

const int PC_ISPOWERED_PIN = D1;
const int PC_RESET_PIN = D2;
const int PC_POWER_PIN = D3;

// pc power/reset pins are GND activated
const int PC_PIN_DEACTIVATE = HIGH;
const int PC_PIN_ACTIVATE   = LOW;

// cable colors: green gnd, yellow vcc, brown touch
const int TOUCH_PIN = D8;

static volatile unsigned long run_touch_handler = 0;
ICACHE_RAM_ATTR void on_touch_change()
{
  int value = digitalRead(TOUCH_PIN);
  if (value == HIGH && millis() - run_touch_handler < 500)
    return;
  digitalWrite(PC_POWER_PIN, (value == HIGH) ? PC_PIN_ACTIVATE : PC_PIN_DEACTIVATE);
  run_touch_handler = millis();
}

static volatile unsigned long run_pc_shutdown_handler = 0;
ICACHE_RAM_ATTR void on_pc_shutdown()
{
  run_pc_shutdown_handler = millis();
}

inline bool pc_is_powered_off()
{
  return (digitalRead(PC_ISPOWERED_PIN) == LOW);
}

inline void pc_trigger_pin(int pin, unsigned long delay_ms)
{
  digitalWrite(pin, PC_PIN_ACTIVATE);
  delay(delay_ms);
  digitalWrite(pin, PC_PIN_DEACTIVATE);
}

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup()
{
  pinMode(PC_POWER_PIN, OUTPUT);
  pinMode(PC_RESET_PIN, OUTPUT);
  digitalWrite(PC_POWER_PIN, PC_PIN_DEACTIVATE);
  digitalWrite(PC_RESET_PIN, PC_PIN_DEACTIVATE);

#if !defined(BTNPIN) || BTNPIN < 0
  pinMode(PC_POWERBUTTON_PIN, INPUT_PULLUP);
#endif
  pinMode(PC_RESETBUTTON_PIN, INPUT_PULLUP);

  pinMode(PC_ISPOWERED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PC_ISPOWERED_PIN), on_pc_shutdown, FALLING);

  pinMode(TOUCH_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), on_touch_change, CHANGE);

  server.on("/pc/reset", HTTP_PUT, [](AsyncWebServerRequest *request) {
    if (pc_is_powered_off())
    {
      request->send_P(409, "text/html", PSTR("NOT OK"));
      return;
    }

    schedule_function([]() {
      pc_trigger_pin(PC_RESET_PIN, 100);
    });
    request->send_P(200, "text/html", PSTR("OK"));
  });

  server.on("/pc/power", HTTP_PUT, [](AsyncWebServerRequest *request) {
    schedule_function([]() {
      pc_trigger_pin(PC_POWER_PIN, 100);
    });
    request->send_P(200, "text/html", PSTR("OK"));
  });

  server.on("/pc/poweroff", HTTP_PUT, [](AsyncWebServerRequest *request) {
    if (pc_is_powered_off())
    {
      request->send_P(409, "text/html", PSTR("NOT OK"));
      return;
    }

    schedule_function([]() {
      pc_trigger_pin(PC_POWER_PIN, 5000);
    });
    request->send_P(200, "text/html", PSTR("OK"));
  });
}

void handleButton(int pin, std::function<void()> on_short_press, std::function<void()> on_long_press)
{
  if (digitalRead(pin) == LOW) //pressed
  {
    if (!buttonPressedBefore)
      buttonPressedTime = millis();
    buttonPressedBefore = true;

    if (millis() - buttonPressedTime > 600) //long press
    {
      if (!buttonLongPressed)
      {
        on_long_press();
        buttonLongPressed = true;
      }
    }
  }
  else if (digitalRead(pin) == HIGH && buttonPressedBefore) //released
  {
    long dur = millis() - buttonPressedTime;
    if (dur < 50) { buttonPressedBefore = false; return; } //too short "press", debounce

    if (!buttonLongPressed) //short press
      on_short_press();
    buttonPressedBefore = false;
    buttonLongPressed = false;
  }
}

#if 1
void handlePowerButton(int pin)
{
  if (digitalRead(pin) == LOW) //pressed
  {
    if (!buttonPressedBefore)
      buttonPressedTime = millis();
    buttonPressedBefore = true;

    if (millis() - buttonPressedTime > 600) //long press
    {
      if (!buttonLongPressed)
      {
        if (bri == 0)
          toggleOnOff();
        effectCurrent = FX_MODE_STATIC;
        effectPalette = 0;
        colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
        buttonLongPressed = true;
      }
    }
  }
  else if (digitalRead(pin) == HIGH && buttonPressedBefore) //released
  {
    long dur = millis() - buttonPressedTime;
    if (dur < 50) { buttonPressedBefore = false; return; } //too short "press", debounce

    if (!buttonLongPressed) { //short press
      if (bri == 0)
        toggleOnOff();
      _setRandomColor(false, true);
    }
    buttonPressedBefore = false;
    buttonLongPressed = false;
  }
}

// reset button
static bool rButtonPressedBefore = false;
static bool rButtonLongPressed = false;
static unsigned long rButtonPressedTime = 0;
void handleResetButton(int pin)
{
  if (digitalRead(pin) == LOW) //pressed
  {
    if (!rButtonPressedBefore)
      rButtonPressedTime = millis();
    rButtonPressedBefore = true;

    if (millis() - rButtonPressedTime > 600) //long press
    {
      if (!rButtonLongPressed)
      {
        if (bri != 0)
          toggleOnOff();
        colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
        rButtonLongPressed = true;
      }
    }
  }
  else if (digitalRead(pin) == HIGH && rButtonPressedBefore) //released
  {
    long dur = millis() - rButtonPressedTime;
    if (dur < 50) { rButtonPressedBefore = false; return; } //too short "press", debounce

    if (!rButtonLongPressed) { //short press
      if (bri == 0)
        toggleOnOff();
      else
        effectCurrent++;
      if (effectCurrent > MODE_COUNT)
        effectCurrent = FX_MODE_STATIC;
      colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
    }
    rButtonPressedBefore = false;
    rButtonLongPressed = false;
  }
}
#endif

void handlePCShutdown()
{
  if (run_pc_shutdown_handler && millis() - run_pc_shutdown_handler > 500)
  {
    run_pc_shutdown_handler = 0;
    if (pc_is_powered_off() && bri != 0)
    {
      toggleOnOff();
      effectCurrent = FX_MODE_STATIC;
      colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
    }
  }
}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{
#if 0
#if !defined(BTNPIN) || BTNPIN < 0
  handleButton(PC_POWERBUTTON_PIN, []() {
    if (bri == 0)
      toggleOnOff();
    _setRandomColor(false, true);
  }, []() {
    toggleOnOff();
    effectCurrent = FX_MODE_STATIC;
    effectPalette = 0;
    colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
  });
#endif

  handleButton(PC_RESETBUTTON_PIN, []() {
    if (bri == 0)
      toggleOnOff();
    else
      effectCurrent++;
    if (effectCurrent > MODE_COUNT)
      effectCurrent = FX_MODE_STATIC;
    colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
  }, []() {
    if (bri != 0)
      toggleOnOff();
    colorUpdated(NOTIFIER_CALL_MODE_BUTTON);
  });
#else
  handlePowerButton(PC_POWERBUTTON_PIN);
  handleResetButton(PC_RESETBUTTON_PIN);
#endif

  handlePCShutdown();
}
