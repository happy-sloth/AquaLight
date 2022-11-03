#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "FS.h"

#include <Wire.h>
//#include "ESP8266TimerInterrupt.h"
#include <pcf8574_esp.h>
#define NUM_LEDS 60
#include "FastLED.h"

#define RGB_LINE_PIN 0
CRGB leds[NUM_LEDS];
byte counter;

#define LOOP_TIMESLOT 1

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const long utcOffsetInSeconds = 3600 * 5;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

#define PCF8563address 0x51
#define PCF8574adress  0x38
/* define pwm pins */
const int LED_COLD_WHITE = 5,
          LED_WARM_WHITE = 4,
          LED_RED_BLUE   = 2;

String days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

enum
{
  LED_LINE_COLD_WHITE_INDEX = 0,
  LED_LINE_WARM_WHITE_INDEX,
  LED_LINE_BLUE_RED_INDEX,
  LED_LINE_RGB_INDEX,

  NUM_OF_LINES,
} led_lines;

typedef struct
{
  byte second;
  byte minute;
  byte hour;
  byte dayOfWeek;
  byte dayOfMonth;
  byte month;
  byte year;
} aqua_time_t;

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint32_t duration_sec;
  bool cold_line;
  bool warm_line;
  uint8_t finish_brightness;
} timer_param_t;

typedef struct
{
  bool power_state;
  uint8_t target_brightness;
  uint8_t current_brightness;
} line_state_t;

bool server_start = false;
//ESP8266Timer ITimer;
aqua_time_t  current_time;


PCF857x pcf8574(PCF8574adress, &Wire);

bool ntp_begin = false;
bool time_is_synchronized = false;
bool timers_state = true;

volatile uint64_t msec_counter = 0;
timer_param_t* current_timer = NULL;


timer_param_t on_timer = {.hours = 7,
                          .minutes = 0,
                          .duration_sec = 300,
                          .cold_line = true,
                          .warm_line = false,
                          .finish_brightness = 255,
                         };
timer_param_t off_timer = {.hours = 23,
                           .minutes = 0,
                           .duration_sec = 300,
                           .cold_line = true,
                           .warm_line = false,
                           .finish_brightness = 0,
                          };

line_state_t lines_states[NUM_OF_LINES] = {{.power_state = true, .target_brightness = 255, .current_brightness = 0},
  {.power_state = false, .target_brightness = 255, .current_brightness = 0},
  {.power_state = false, .target_brightness = 255, .current_brightness = 0},
  {.power_state = false, .target_brightness = 255, .current_brightness = 0}
};

uint32_t rgb_line_color_code = 16711935;

bool button_backlight_state = true;
volatile bool alarm_flag = false;
bool timers_change = false;

typedef enum
{
  COLD_WHITE_BUTTON = 3,
  WARM_WHITE_BUTTON = 2,
  RED_BLUE_BUTTON = 0,
  RGB_BUTTON = 1,
} extender_button_channels;

enum
{
  COLD_WHITE_BUTTON_BACKLIGHT = 7,
  WARM_WHITE_BUTTON_BACKLIGHT = 6,
  RED_BLUE_BUTTON_BACKLIGHT = 4,
  RGB_BUTTON_BACKLIGHT = 5,
} extender_backlight_channels;

typedef enum
{
  BUTTON_IDLE = 0,
  BUTTON_WAIT_LONG_PUSH,
  BUTTON_WAIT_DOUBLE_LONG_PUSH,
  BUTTON_LONG_PUSH,
  BUTTON_DOUBLE_LONG_PUSH,
} button_fsm_state_t;

typedef enum
{
  SHORT_PUSH = 0,
  LONG_PUSH,
  DOUBLE_LONG_PUSH,
} button_event_t;

#define SHORT_PUSH_TIMEOUT 1000
#define DOUBLE_LONG_PUSH_WAITING_TIMEOUT  500
#define LONG_PUSH_BRIGHT_INCREMENT_TIMESLOT 20

bool button_curr_state[4] = {0};// 0 - released, 1 - pushed
static uint64_t button_last_time_point[4] = {0};
static button_fsm_state_t button_state[4] = {BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE};

void led_routine(void);
void start_dimming(uint8_t chan, uint64_t dim_time, uint8_t target_brightness);
void do_dimming();
void switch_line(uint8_t chan, bool state);
void toggle_line_state(uint8_t chan);


void button_backlight_routine();
void button_routine();

void print_time ();
ICACHE_RAM_ATTR void alarm_interrupt();
void setPCF8563alarm(uint8_t alarmMinute, uint8_t alarmHour);
aqua_time_t readPCF8563();
void setPCF8563(aqua_time_t b_time);
aqua_time_t unix_to_time (int64_t unix_time);
//void /*IRAM_ATTR*/ TimerHandler();
void aqua_timers_routine();
void find_next_timer();

void server_index_handler(AsyncWebServerRequest *request);
void notifyClients();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);


void setup()
{
  Serial.begin(115200);

  Serial.println();
  SPIFFS.begin();
  analogWriteFreq(100);

  WiFi.begin("SSID", "PWD");
  Serial.print("start wifi");
  pinMode(14, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(14), alarm_interrupt, FALLING);

  Wire.begin(13, 12);

  //  uint8_t b_config = 0;
  //  b_config = (1 << COLD_WHITE_BUTTON) |  (1 << WARM_WHITE_BUTTON) |  (1 << RED_BLUE_BUTTON) |  (1 << RGB_BUTTON);
  pcf8574.begin();
  FastLED.addLeds<TM1804, RGB_LINE_PIN, RBG>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );

  //  ITimer.attachInterruptInterval(1000/*usec*/, TimerHandler);
}

void loop()
{
  static int counter = 0;
  /* Network section*/
  if (WiFi.status() == WL_CONNECTED)
  {
    if (server_start)
    {
      ws.cleanupClients();
    }
    else
    {
      ws.onEvent(onEvent);
      server.addHandler(&ws);
      server.on("/", HTTP_GET, server_index_handler);
      server.begin();
      server_start = true;
    }

    if (!ntp_begin)
    {
      timeClient.begin();
      ntp_begin = true;
    }
    else if (!time_is_synchronized)
    {
      aqua_time_t b_time;
      timeClient.update();
      b_time = unix_to_time(timeClient.getEpochTime());
      Serial.printf("Get time %ll \n", timeClient.getEpochTime());
      setPCF8563(b_time);
      time_is_synchronized = true;
    }
  }
  static uint64_t last_time = 0;
  if (millis() - last_time >= 1000)
  {
    print_time();
    last_time = millis();
  }

  msec_counter = millis();
  do_dimming();
  led_routine();

  //  if (button_backlight_state)
  //    button_backlight_routine();

  button_routine();
  current_time = readPCF8563();
  aqua_timers_routine();

  delay(LOOP_TIMESLOT);
}

void aqua_timers_routine()
{

  if (!timers_state)
  {
    PCF8563alarmOff();
    return;
  }

  if (current_timer == NULL)
  {
    find_next_timer();
  }
  //  else
  //  {
  //    Serial.printf("current_timer is %d, on_timer : %d, off_timer : %d", (int)current_timer, (int)&on_timer, (int)&off_timer);
  //  }
  if (timers_change)
  {
    timers_change = false;
    find_next_timer();
  }

  if (alarm_flag)
  {
    Serial.print("Alarm!");
    alarm_flag = false;
    current_time = readPCF8563();
    if (current_timer != NULL)
    {
      Serial.print("Go timer!");
      if (current_timer->cold_line)
      {
        start_dimming(LED_LINE_COLD_WHITE_INDEX, current_timer->duration_sec * 1000, current_timer->finish_brightness);
      }

      if (current_timer->warm_line)
        start_dimming(LED_LINE_WARM_WHITE_INDEX, current_timer->duration_sec * 1000, current_timer->finish_brightness);
    }
    PCF8563alarmOff();
    find_next_timer();
    notifyClients();
  }
}

void find_next_timer()
{
  uint32_t b_on_timer_timestamp, b_off_timer_timestamp, b_curr_time_timestamp;

  b_on_timer_timestamp = on_timer.hours * 3600 + on_timer.minutes * 60;
  b_off_timer_timestamp = off_timer.hours * 3600 + off_timer.minutes * 60;

  uint32_t b_hour = current_time.hour, b_min = current_time.minute, b_sec = current_time.second;

  b_curr_time_timestamp = (uint32_t)current_time.hour * 3600 + (uint32_t)current_time.minute * 60 + current_time.second;


  //    Serial.printf("b_on_timer_timestamp = %d, b_off_timer_timestamp = %d, b_curr_time_timestamp = %d",
  //                    b_on_timer_timestamp, b_off_timer_timestamp, b_curr_time_timestamp);

  if (b_on_timer_timestamp <= b_curr_time_timestamp)
    b_on_timer_timestamp += 24 * 3600;

  if (b_off_timer_timestamp <= b_curr_time_timestamp)
    b_off_timer_timestamp += 24 * 3600;

  if (b_on_timer_timestamp > b_curr_time_timestamp && b_on_timer_timestamp < b_off_timer_timestamp)
  {
    current_timer = &on_timer;
    Serial.printf("next timer is on_timer on time %d:%d\n", on_timer.hours, on_timer.minutes);
  }
  else
  {
    current_timer = &off_timer;
    Serial.printf("next timer is off_timer on time %d:%d\n", off_timer.hours, off_timer.minutes);
  }

  setPCF8563alarm(current_timer->minutes, current_timer->hours);
}

//void /*IRAM_ATTR*/ TimerHandler()
//{
//  msec_counter++;
//}
