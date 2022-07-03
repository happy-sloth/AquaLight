void button_event_handler (button_event_t event, uint8_t chan)
{
  return;
  extender_button_channels b_chan = (extender_button_channels)chan;
  uint8_t b_line = 0;
  
  switch(b_chan)
  {
    case COLD_WHITE_BUTTON:
    {
      b_line = LED_LINE_COLD_WHITE_INDEX;
      break;
    }
    case WARM_WHITE_BUTTON:
    {
      b_line = LED_LINE_WARM_WHITE_INDEX;
      break;
    }
    case RED_BLUE_BUTTON:
    {
      b_line = LED_LINE_BLUE_RED_INDEX;
      break;
    }
    case RGB_BUTTON:
    {
      b_line = LED_LINE_RGB_INDEX;
      break;
    }
  }

  switch (event) 
  {
      case SHORT_PUSH:
      {
        toggle_line_state(b_line);
        break;
      }
      case LONG_PUSH:
      {
          if (++lines_states[b_line].current_brightness > 255) lines_states[b_line].current_brightness = 255 ;
          if (++lines_states[b_line].target_brightness > 255) lines_states[b_line].target_brightness = 255;
          Serial.println(lines_states[b_line].current_brightness);
      }
      case DOUBLE_LONG_PUSH:
      {
          if (--lines_states[b_line].current_brightness < 1) lines_states[b_line].current_brightness=1 ;
          if (--lines_states[b_line].target_brightness < 1) lines_states[b_line].target_brightness=1;
      }
      default:
        break;
  }     
}

void update_buttons_state()
{
  uint8_t b_data = pcf8574.read8();
  for (int i = 0; i < 4; i++)
  {
    button_curr_state[i] = !(b_data & (1 << i));
  }
//  Serial.println(b_data, BIN);
}

void button_backlight_routine()
{
  /* Send data to extender */
  pcf8574.write(COLD_WHITE_BUTTON_BACKLIGHT, lines_states[LED_LINE_COLD_WHITE_INDEX].power_state);
  pcf8574.write(WARM_WHITE_BUTTON_BACKLIGHT, lines_states[LED_LINE_WARM_WHITE_INDEX].power_state);
  pcf8574.write(RED_BLUE_BUTTON_BACKLIGHT, lines_states[LED_LINE_BLUE_RED_INDEX].power_state);
  pcf8574.write(RGB_BUTTON_BACKLIGHT, lines_states[LED_LINE_RGB_INDEX].power_state);
}

void button_routine()
{
  update_buttons_state();

  for (int i = 0; i < 4; i++)
  {
    if (button_curr_state[i])
    {
      switch (button_state[i])
      {
        case BUTTON_IDLE:
        {
          button_state[i] = BUTTON_WAIT_LONG_PUSH;
          button_last_time_point[i] = msec_counter;
          break;
        }
        case BUTTON_WAIT_DOUBLE_LONG_PUSH:
        {
          if (msec_counter - button_last_time_point[COLD_WHITE_BUTTON] >= SHORT_PUSH_TIMEOUT)
          {
            button_state[i] = BUTTON_DOUBLE_LONG_PUSH;
            button_last_time_point[i] = msec_counter;
            button_event_handler(DOUBLE_LONG_PUSH, i);
          }
          break;
        }
        case BUTTON_LONG_PUSH:
        {
          if (msec_counter - button_last_time_point[COLD_WHITE_BUTTON] >= LONG_PUSH_BRIGHT_INCREMENT_TIMESLOT)
          {
            Serial.println("long push!");
            button_event_handler(LONG_PUSH, i);
          }
          break;
        }
        case BUTTON_WAIT_LONG_PUSH:
        {
          if (msec_counter - button_last_time_point[COLD_WHITE_BUTTON] >= SHORT_PUSH_TIMEOUT)
          {
            button_state[i] = BUTTON_LONG_PUSH;
            button_last_time_point[i] = msec_counter;
            button_event_handler(LONG_PUSH, i);
          }
          break;
        }
        case BUTTON_DOUBLE_LONG_PUSH:
        {
          if (msec_counter - button_last_time_point[COLD_WHITE_BUTTON] >= LONG_PUSH_BRIGHT_INCREMENT_TIMESLOT)
          {
            button_event_handler(DOUBLE_LONG_PUSH, i);
          }
          break;
        }
        default:
          break;
      }  
    }
    else
    {
      switch (button_state[i])
      {
        case BUTTON_WAIT_LONG_PUSH:
        {
          button_state[i] = BUTTON_WAIT_DOUBLE_LONG_PUSH;
          button_last_time_point[i] = msec_counter;
          break;
        }
        case BUTTON_WAIT_DOUBLE_LONG_PUSH:
        {
          if (msec_counter - button_last_time_point[COLD_WHITE_BUTTON] >= DOUBLE_LONG_PUSH_WAITING_TIMEOUT)
          {
            button_state[i] = BUTTON_IDLE;
            button_last_time_point[i] = msec_counter;
            button_event_handler(SHORT_PUSH, i);
          }
          break;
        }
        case BUTTON_LONG_PUSH:
        {
          button_state[i] = BUTTON_IDLE; 
          break;
        }
        case BUTTON_DOUBLE_LONG_PUSH:
        {
          button_state[i] = BUTTON_IDLE; 
          break;
        }
        case BUTTON_IDLE:
        default:
          break;
      }       
    }
  }
}
