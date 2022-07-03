// функция перевода из двоично - десятичной системы в десятичную.
byte bcdToDec(byte value)
{
 return ((value / 16) * 10 + value % 16);
}
// И обратно
byte decToBcd(byte value){
 return (value / 10 * 16 + value % 10);
}

void setPCF8563(aqua_time_t b_time)
{
 Wire.beginTransmission(PCF8563address);
 Wire.write(0x02);
 Wire.write(decToBcd(b_time.second)); 
 Wire.write(decToBcd(b_time.minute));
 Wire.write(decToBcd(b_time.hour));   
 Wire.write(decToBcd(b_time.dayOfMonth));
 Wire.write(decToBcd(b_time.dayOfWeek)); 
 Wire.write(decToBcd(b_time.month));
 Wire.write(decToBcd(b_time.year));
 Wire.endTransmission();
}

aqua_time_t readPCF8563()
{
  aqua_time_t b_time;
 Wire.beginTransmission(PCF8563address);
 Wire.write(0x02);
 Wire.endTransmission();
 Wire.requestFrom(PCF8563address, 7);
 b_time.second   = bcdToDec(Wire.read() & B01111111); // удаление ненужных бит из данных 
 b_time.minute   = bcdToDec(Wire.read() & B01111111); 
 b_time.hour    = bcdToDec(Wire.read() & B00111111); 
 b_time.dayOfMonth = bcdToDec(Wire.read() & B00111111);
 b_time.dayOfWeek = bcdToDec(Wire.read() & B00000111); 
 b_time.month   = bcdToDec(Wire.read() & B00011111); 
 b_time.year    = bcdToDec(Wire.read());

 return b_time;
}

void setPCF8563alarm(uint8_t alarmMinute, uint8_t alarmHour)
{
 byte am, ah, ad, adow;
 am = decToBcd(alarmMinute);
 am = am | 100000000; // Установка бита включение будильника по минутам   
 ah = decToBcd(alarmHour);
 ah = ah | 100000000; // Установка бита включение будильника по часам  

 // запись минут и часов срабатывания будильника на PCF8563
 Wire.beginTransmission(PCF8563address);
 Wire.write(0x09);
 Wire.write(am); 
 Wire.write(ah);

 // опционально. запись дня месяца и недели на срабатывания будильника
 Wire.endTransmission();
  
 // опционально . включение INT пина при хардовой проверки
 // отключается при использование функции PCF8563alarmOff()

 Wire.beginTransmission(PCF8563address);
 Wire.write(0x01);
 Wire.write(B00000010);
 Wire.endTransmission();
}

void PCF8563alarmOff()
// функция выключения будильника с обнулением регистра.
{
 byte test;
 // получение значения регистра 0x01h
 Wire.beginTransmission(PCF8563address);
 Wire.write(0x01);
 Wire.endTransmission();
 Wire.requestFrom(PCF8563address, 1);
 test = Wire.read();

 // установка 3 го бита в 0 
 test = test - B00001000;

 // запись нового значения в регистр 0x01h
 Wire.beginTransmission(PCF8563address);
 Wire.write(0x01);
 Wire.write(test);
 Wire.endTransmission();
}

ICACHE_RAM_ATTR void alarm_interrupt()
{
  alarm_flag = true;
}

void print_time ()
{
   aqua_time_t b_time;
   b_time = readPCF8563();
   Serial.print(days[b_time.dayOfWeek]); 
   Serial.print(" "); 
   Serial.print(b_time.dayOfMonth, DEC);
   Serial.print("/");
   Serial.print(b_time.month , DEC);
   Serial.print("/20");
   Serial.print(b_time.year , DEC);
   Serial.print(" - ");
   Serial.print(b_time.hour, DEC);
   Serial.print(":");
   if ( b_time.minute < 10)
   {
    Serial.print("0");
   }
   Serial.print(b_time.minute, DEC);
   Serial.print(":"); 
   if (b_time.second < 10)
   {
    Serial.print("0");
   } 
   Serial.println(b_time.second, DEC); 

}

aqua_time_t unix_to_time (int64_t unix_time)
{
    aqua_time_t b_time = {0};
    uint32_t aCoef,bCoef,cCoef,dCoef,eCoef,mCoef,jd,jdn;
  
  jd = ((unix_time + 43200)/(86400>>1)) + (2440587<<1) + 1;
  jdn = jd>>1;
  
  b_time.second = unix_time%60;
  unix_time /= 60;
  b_time.minute = unix_time%60;
  unix_time /= 60;
  b_time.hour = unix_time%24;
  unix_time /= 24;
  
  b_time.dayOfWeek = jdn%7 + 1;
  
  aCoef = jdn + 32044;
  bCoef = ((4 * aCoef) + 3) / 146097;
  cCoef = aCoef - (146097 * bCoef) / 4;
  dCoef = ((4 * cCoef) + 3) / 1461;
  eCoef = cCoef - (1461 * dCoef) / 4;
  mCoef = ((5 * eCoef) + 2) / 153;
  
  b_time.dayOfMonth = eCoef - ((153 * mCoef) + 2)/5 +1;
  b_time.month = mCoef + 3 - (12 * (mCoef / 10));
  b_time.year = ((100 * bCoef) + dCoef - 4800 + (mCoef / 10)-2000);

    return b_time;
}
