void server_index_handler(AsyncWebServerRequest *request)
{
  Serial.print("Request!\n");
  File f = SPIFFS.open("/index.html", "r");
  if (f) 
  {
    request->send(SPIFFS, "/index.html");
  }
  else
    Serial.print("File not found!");
}

void notifyClients()
{
  String dev_state;

  DynamicJsonDocument doc(1024);

  doc["coldWhiteState"] = lines_states[LED_LINE_COLD_WHITE_INDEX].power_state;
  doc["coldWhiteBright"] = lines_states[LED_LINE_COLD_WHITE_INDEX].target_brightness;
  doc["warmWhiteState"] = lines_states[LED_LINE_WARM_WHITE_INDEX].power_state;
  doc["warmWhiteBright"] = lines_states[LED_LINE_WARM_WHITE_INDEX].target_brightness;
  doc["redblueState"] = lines_states[LED_LINE_BLUE_RED_INDEX].power_state;
  doc["redblueBright"] = lines_states[LED_LINE_BLUE_RED_INDEX].target_brightness;
  doc["rgbState"] = lines_states[LED_LINE_RGB_INDEX].power_state;
  doc["rgbBright"] = lines_states[LED_LINE_RGB_INDEX].target_brightness;
  char b_str[8] = {0};
  sprintf(b_str, "#%.6x", rgb_line_color_code);
  doc["rgbColor"] = b_str;
 
  doc["timerEnable"] = timers_state;
  memset(b_str, 0, 8);
  sprintf(b_str, "%.2d:%.2d", on_timer.hours, on_timer.minutes);
  doc["onTime"] = b_str;
  memset(b_str, 0, 8);
  sprintf(b_str, "%.2d:%.2d", off_timer.hours, off_timer.minutes);
  doc["offTime"] = b_str;
  doc["timerColdWhite"] = on_timer.cold_line;
  doc["timerWarmWhite"] = on_timer.warm_line;
  doc["timerTargetBrightness"] = on_timer.finish_brightness;
  doc["timerSunsetDuration"] = on_timer.duration_sec/60;
   
  serializeJson(doc, dev_state);
  ws.textAll(dev_state);
  
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    //Serial.print((char*)data);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, data);

    if (doc.containsKey("coldWhiteState"))
      switch_line(LED_LINE_COLD_WHITE_INDEX, doc["coldWhiteState"]);

    if (doc.containsKey("coldWhiteBright"))
      start_dimming(LED_LINE_COLD_WHITE_INDEX, 1000, doc["coldWhiteBright"]);

    if (doc.containsKey("warmWhiteState"))
      switch_line(LED_LINE_WARM_WHITE_INDEX, doc["warmWhiteState"]);

    if (doc.containsKey("warmWhiteBright"))
      start_dimming(LED_LINE_WARM_WHITE_INDEX, 1000, doc["warmWhiteBright"]);

    if (doc.containsKey("redblueState"))
      switch_line(LED_LINE_BLUE_RED_INDEX, doc["redblueState"]);

    if (doc.containsKey("redblueBright"))
      start_dimming(LED_LINE_BLUE_RED_INDEX, 1000, doc["redblueBright"]);

    if (doc.containsKey("rgbState"))
      switch_line(LED_LINE_RGB_INDEX, doc["rgbState"]);

    if (doc.containsKey("rgbBright"))
      start_dimming(LED_LINE_RGB_INDEX, 1000, doc["rgbBright"]);

    if (doc.containsKey("rgbColor"))
    {
      String str = String(doc["rgbColor"]);
      str = str.substring(1);
      char buf[7];
      str.toCharArray(buf, 7);
      rgb_line_color_code = strtol(buf, 0, 16);
    }

     if (doc.containsKey("timerEnable"))
     {
      timers_state = doc["timerEnable"];
     }

     if (doc.containsKey("onTime"))
     {
        String b_str(doc["onTime"]);
        int index = b_str.indexOf(':', 0);
        char b_min[3] = {0}, b_hour[3] = {0};

        b_str.substring(0, index).toCharArray(b_hour, 3);
        b_str.substring(index + 1).toCharArray(b_min, 3);
        on_timer.hours = atoi(b_hour); 
        on_timer.minutes = atoi(b_min);
        timers_change = true;
     }

     if (doc.containsKey("offTime"))
     {
        String b_str(doc["offTime"]);
        int index = b_str.indexOf(':', 0);
        char b_min[3] = {0}, b_hour[3] = {0};

        b_str.substring(0, index).toCharArray(b_hour, 3);
        b_str.substring(index + 1).toCharArray(b_min, 3);
        
        off_timer.hours = atoi(b_hour); 
        off_timer.minutes = atoi(b_min);
        timers_change = true;
     }

     if (doc.containsKey("timerColdWhite"))
      on_timer.cold_line = doc["timerColdWhite"];

     if (doc.containsKey("timerWarmWhite"))
      on_timer.warm_line = doc["timerWarmWhite"];

     if (doc.containsKey("timerTargetBrightness"))
      on_timer.finish_brightness = doc["timerTargetBrightness"];
  
     if (doc.containsKey("timerSunsetDuration"))
     {
      int buff = doc["timerSunsetDuration"];
      on_timer.duration_sec = buff*60;
     }
    notifyClients();
   }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
 void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
