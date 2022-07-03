
void led_routine(void)
{ 
//   int b_bright = 0;

//   if (lines_states[LED_LINE_COLD_WHITE_INDEX].power_state)
//   {
//      b_bright = (int)((float)(255.0 / 100.0)*(float)lines_states[LED_LINE_COLD_WHITE_INDEX].current_brightness);
      analogWrite(LED_COLD_WHITE, lines_states[LED_LINE_COLD_WHITE_INDEX].current_brightness);
//   }
//   else
//      analogWrite(LED_COLD_WHITE, 0);
//
//   if (lines_states[LED_LINE_WARM_WHITE_INDEX].power_state)
//   {
//      b_bright = (int)((float)(255.0 / 100.0)*(float)lines_states[LED_LINE_WARM_WHITE_INDEX].current_brightness);
      analogWrite(LED_WARM_WHITE, lines_states[LED_LINE_WARM_WHITE_INDEX].current_brightness);
//   }
//   else
//      analogWrite(LED_WARM_WHITE, 0);

//   if (lines_states[LED_LINE_BLUE_RED_INDEX].power_state)
//   {
//      b_bright = (int)((float)(255.0 / 100.0)*(float)lines_states[LED_LINE_BLUE_RED_INDEX].current_brightness);
      analogWrite(LED_RED_BLUE, lines_states[LED_LINE_BLUE_RED_INDEX].current_brightness);
//   }
//   else
//      analogWrite(LED_RED_BLUE, 0);
//
//   if (lines_states[LED_LINE_RGB_INDEX].power_state)
//   {
      for (int i = 0; i < NUM_LEDS; i++ ) 
      {
        int b_r = 0x0000FF & (rgb_line_color_code >> 16);
        int b_g = 0x0000FF & (rgb_line_color_code >> 8);
        int b_b = 0x0000FF & rgb_line_color_code;
        leds[i] = CRGB(b_r, b_g, b_b); 
      }
//      b_bright = 2.55 * lines_states[LED_LINE_RGB_INDEX].current_brightness;
      FastLED.setBrightness(lines_states[LED_LINE_RGB_INDEX].current_brightness);
      FastLED.show();      
//   }
//   else
//   {
//      FastLED.setBrightness(0);
//      FastLED.show();  
//   }  
}

static uint64_t increment_time[NUM_OF_LINES] = {0};
static uint64_t last_time_point[NUM_OF_LINES] = {0};

void start_dimming(uint8_t chan, uint64_t dim_time, uint8_t target_brightness)
{
  if (target_brightness && dim_time)
  {
    lines_states[chan].power_state = true;
    lines_states[chan].target_brightness = target_brightness;
    increment_time[chan] = dim_time/(target_brightness - lines_states[chan].current_brightness);
    last_time_point[chan] = msec_counter;
//    buff_float_current[chan] = lines_states[chan].current_brightness;
  }
  else if (!target_brightness && dim_time)
  {
    lines_states[chan].power_state = false;
    increment_time[chan] = dim_time/(0 - lines_states[chan].current_brightness);
//    buff_float_current[chan] = lines_states[chan].current_brightness;
    last_time_point[chan] = msec_counter;
  }
  else if (target_brightness && !dim_time)
  {
    lines_states[chan].power_state = true;
    lines_states[chan].current_brightness = target_brightness;
    lines_states[chan].target_brightness = target_brightness;
  }
  else
  {
    lines_states[chan].power_state = false;
    lines_states[chan].current_brightness = 0;
  }
}

void do_dimming()
{
  for (int i = 0; i < NUM_OF_LINES; i++)
  {
     if (lines_states[i].power_state)
     {
        if(lines_states[i].current_brightness == lines_states[i].target_brightness)
          continue;
        
        if (lines_states[i].current_brightness <= lines_states[i].target_brightness)
        {
          if (msec_counter - last_time_point[i] >= increment_time[i])
          {
            lines_states[i].current_brightness ++;
            last_time_point[i] = msec_counter;
          }
        }
        else if  (lines_states[i].current_brightness >= lines_states[i].target_brightness)
        {
          if (msec_counter - last_time_point[i] >= increment_time[i])
          {
            lines_states[i].current_brightness--;
            last_time_point[i] = msec_counter;
          }  
        }
     }
     else
     {
//        buff_float_current[i] += bright_step[i];
        if (lines_states[i].current_brightness > 0)
        {
          if (msec_counter - last_time_point[i] >= increment_time[i])
          {
             lines_states[i].current_brightness --;
             last_time_point[i] = msec_counter;
          }
        }
     }
  }
}

void switch_line(uint8_t chan, bool state)
{
  if (state)
  {
    lines_states[chan].power_state = true;
    start_dimming(chan, 1000, lines_states[chan].target_brightness);
  }
  else
    start_dimming(chan, 1000, 0);

    Serial.printf("chan %d power state is %s, target brightness is %d, current brighness is %d\n", 
          chan, lines_states[chan].power_state ? "on" : "off", lines_states[chan].target_brightness, lines_states[chan].current_brightness);
}

void toggle_line_state(uint8_t chan)
{
  if (lines_states[chan].power_state)
    start_dimming(chan, 1000, 0);
  else
    start_dimming(chan, 1000, lines_states[chan].target_brightness);
}
