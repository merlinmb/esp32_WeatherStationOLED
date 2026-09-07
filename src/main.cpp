#define FORMAT_SPIFFS_IF_FAILED true

#define DEBUG 1
 
#ifdef DEBUG
#define DEBUG_PRINT(x)			Serial.print (x)
#define DEBUG_PRINTDEC(x,DEC)	Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)		Serial.println (x)
#define DEBUG_PRINTLNDEC(x,DEC)	Serial.println (x, DEC)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x,DEC)
#define DEBUG_PRINTLN(x) 
#define DEBUG_PRINTLNDEC(x,DEC)
#endif

#include <SPIFFS.h>

// #include <WiFiManager.h>  // version 2.0.17
#include <TFT_eSPI.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ArduinoJson.h> // 7.1.0
#include <HTTPClient.h>  // version 0.6.1
#include <ESP32Time.h>   // verison 2.0.6
#include "fonts/tinyFont.h"
#include "fonts/smallFont.h"
#include "fonts/midleFont.h"
#include "fonts/bigFont.h"
#include "fonts/font18.h"
#include "Arduino.h"

#include "connectionDetails.h"

#include "merlinNetwork.h"
#include "merlinUpdateWebServer.h"

#include "OneButton.h"

#include "fonts/NotoSansBold15.h"


#define MCMDVERSION 1.4

#define WEATHER_TPH_MQTT_TOPIC "zigbee2mqtt/mcmdhome/0x00158d000949f915"
#define WEATHER_WINDSPEED_MQTT_TOPIC "stat/espAnemometer/mps_avg"

String _mqttTopic = WEATHER_TPH_MQTT_TOPIC;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite errSprite = TFT_eSprite(&tft);
ESP32Time rtc(0);
String _updatedTime="";

// #################### EDIT THIS  ###################
// time zone
int zone = 1;
String town = "Newbury";
String myAPI = "REDACTED_OPENWEATHERMAP_KEY";
String units = "metric"; //  metric, imperial
// #################### end of edits ###################

const char *ntpServer = "pool.ntp.org";
String server = "https://api.openweathermap.org/data/2.5/weather?q=" + town + "&appid=" + myAPI + "&units=" + units;

// additional variables
int ani = 100;
float maxT=-100;
float minT=100;
unsigned long _timePassed = 0;
int counter = 0;
float _windowStartTemp = 0;

//................colors
#define bck TFT_BLACK
unsigned short grays[13];

// static strings of data showed on right side
char *PPlbl[] = {"HUM", "PRESS", "WIND"};
String PPlblU[] = {"%", "hPa", "m/s"};

// data that changes
float temperature = 22.2;
float wData[3];
float PPpower[24] = {};  // graph
float PPpowerT[24] = {}; // graph
int PPgraph[24] = {0};   // graph

#define MAXBRIGHTNESS 255
#define MINBRIGHTNESS 20

// T-Display-S3 gates LCD power through this pin - must be HIGH or the panel stays dark
#define LCD_POWER_ON_PIN 15

int _brightnesses[5] = {0, 51, 115, 192, 255};
int _selectedBrightness = 4;
bool _brightnessHigh;
byte _brightness = 255;

// scroling message on bottom right side
String Wmsg = "";
String _lastMQTTMessage = "";

bool _forceUpdate = false;

int _configFlipSreen = 999;

#define BUTTON1 0
#define BUTTON2 14

OneButton _button1 = OneButton(BUTTON1, true, true);
OneButton _button2 = OneButton(BUTTON2, true, true);

#define BACKGROUNDCOLOR TFT_BLACK

#define DEBUGFONT NotoSansBold15
const byte DEBUGBUFFERLENGTH = 8;
byte _debugBufferPosition = 0;
String _debugBuffer[DEBUGBUFFERLENGTH];

void clearSprite()
{
  sprite.fillSprite(BACKGROUNDCOLOR);
}


void DisplayOut(String outStr)
{

  _debugBufferPosition++;
  if (_debugBufferPosition >= DEBUGBUFFERLENGTH)
  {
    for (byte i = 0; i < DEBUGBUFFERLENGTH - 1; i++) // StackArray - shift to left
    {
      _debugBuffer[i] = _debugBuffer[i + 1];
    }
    _debugBufferPosition = DEBUGBUFFERLENGTH - 1;
  }

  DEBUG_PRINTLN(outStr);

  // if ((!_updatingFirmware) && (!_displayInit || _initComplete))
  //   return;

  // render
  clearSprite();

  sprite.loadFont(DEBUGFONT);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK, true);
  //_sprite.setTextSize(12);
  _debugBuffer[_debugBufferPosition] = outStr;
  for (byte i = 0; i < _debugBufferPosition; i++) // StackArray - shift to left
  {
    sprite.drawString(_debugBuffer[i], 5, 20 * i, 2);
  }
  sprite.unloadFont();
  sprite.pushSprite(0, 0);
  // delay(100);
}



void setBrightness(byte brightnessValue)
{
  DEBUG_PRINTLN("setBrightness: " + String(brightnessValue));

  ledcWrite(0, brightnessValue);

  for (int i = 0; i < 5; i++)
  {
    if (brightnessValue == _brightnesses[i])
    {
      _selectedBrightness = i;
      break;
    }
  }
}


void rotateBrightness()
{
  DEBUG_PRINTLN("rotateBrightness");
  _selectedBrightness--;
  if (_selectedBrightness < 0)
    _selectedBrightness = 4;

  setBrightness(_brightnesses[_selectedBrightness]);
}
/***************************************************
  SPIFFS functions
****************************************************/
bool parseConfigValue(String key, String value)
{
  DEBUG_PRINTLN("Parsing Config Value, " + key + ": " + value);

  key.toLowerCase();
  value.trim();

  if (key=="mqtt_topic")
  {
    if (value.length() > 0)
    {
      if (value!=_mqttTopic)
      {
        DEBUG_PRINTLN("unsubscribing from old MQTT Topic: " + _mqttTopic);
        xSemaphoreTake(_mqttMutex, portMAX_DELAY);
        _mqttClient.unsubscribe(_mqttTopic.c_str());
        _mqttTopic = value;
        DEBUG_PRINTLN("subscribing to new MQTT Topic: " + _mqttTopic);
        _mqttClient.subscribe(_mqttTopic.c_str());
        xSemaphoreGive(_mqttMutex);
        

      }
      // _forceRender = true;
    }
    else
    {
      DEBUG_PRINTLN("MQTT Topic is empty, not set");
    }
  }

  if (key == "station")
  {
    _forceUpdate = true;
  }

  if (key == "flipscreen")
  {
    int __intValue = (value == "true" ? 3 : 1);
    if (_configFlipSreen != __intValue)
    {
      _configFlipSreen = __intValue;
      tft.setRotation(_configFlipSreen);
      //_forceRender = true;
    }
  }

  if (key == "brightness")
  {
    int __newVal = value.toInt();
    if (_brightness != __newVal)
    {
      _brightness = value.toInt();
      setBrightness(_brightness);
      //_forceRender = true;
    }
  }

  DEBUG_PRINTLN("parseConfigValue() - completed...");

  return true;
}

void setupSPIFFS()
{
  if (SPIFFS.begin())
  {
    DEBUG_PRINTLN("SPIFFS: Mounted file system");
  }
  else
  {
    DEBUG_PRINTLN("SPIFFS: FAILED to mount file system!");
  }
}

void loadCustomParamsSPIFFS()
{
  // read configuration from FS json
  DEBUG_PRINTLN("loadCustomParamsSPIFFS() - Open config file...");

  // if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
  //   DEBUG_PRINTLN("SPIFFS Mount Failed... Formatted to initialize");
  //   return;
  // }

  DEBUG_PRINTLN("SPIFFS Mounted ... Opening config");
  File __configFile = SPIFFS.open("/config.ini", FILE_READ);
  if (__configFile)
  {
    DEBUG_PRINTLN("Reading config file [" + String(__configFile.size()) + " bytes]");
    while (__configFile.available())
    {
      String __inString = __configFile.readStringUntil('\n');
      DEBUG_PRINTLN("Read line: " + __inString);
      int __equalsLoc = __inString.indexOf('=');

      String __key = __inString.substring(0, __equalsLoc);
      String __value = __inString.substring(__equalsLoc + 1, __inString.length());

      parseConfigValue(__key, __value);
    }

    DEBUG_PRINTLN("loadCustomParamsSPIFFS() - close config file...");
    __configFile.close();
    DEBUG_PRINTLN("... Done");
  }
}

void writeStrtoFile(File file, String key, String value)
{
  DEBUG_PRINTLN("    " + key + ": " + value);
  file.println(key + "=" + value);
}

void saveConfigValuesSPIFFS()
{
  DEBUG_PRINTLN("saveConfigValuesSPIFFS()");
  if (SPIFFS.remove("/config.ini"))
  {
    DEBUG_PRINTLN("Deleted old file");
  }

  DEBUG_PRINTLN("Open File in Write Mode");
  // open the file in write mode
  File __configFile = SPIFFS.open("/config.ini", FILE_WRITE);
  DEBUG_PRINTLN("Saving config to FS");

  writeStrtoFile(__configFile, "flipscreen", String(_configFlipSreen == 3));
  writeStrtoFile(__configFile, "brightness", String(_brightness));
  writeStrtoFile(__configFile, "mqtt_topic", String(_mqttTopic));
  __configFile.close();
  DEBUG_PRINTLN("... Done");
  delay(250); // give SPIFFS chance to settle
}

void toggleBrightness(bool isBright)
{
  _brightness = (isBright) ? MAXBRIGHTNESS : MINBRIGHTNESS;

  //_display.setContrast(isBright ? 255 : 80);
  //_currentNeoPixelColour = pixelBrightness(_currentNeoPixelColour, _brightness);
  setBrightness(_brightness);

  _brightnessHigh = isBright;
}

void setTime()
{
  configTime(3600 * zone, 0, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    rtc.setTimeStruct(timeinfo);
  }
}

void getData()
{
  HTTPClient http;
  http.begin(server);
  http.setTimeout(HTTPRESPONSETIMEOUT);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0)
  {
    String payload = http.getString();
    Serial.println("server response");
    Serial.println(payload);

    // Parsiranje JSON odgovora
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      wData[0] = doc["main"]["humidity"];
      wData[1] = doc["main"]["pressure"];
      wData[2] = doc["wind"]["speed"];

      int visibility = doc["visibility"];
      const char *description = doc["weather"][0]["description"];
      long dt = doc["dt"];

      Wmsg = "#Description: " + String(description) + "  #Visbility: " + String(visibility) + (_updatedTime.length()>0 ? "  #Updated: " + _updatedTime :"");

      // Show temperature on serial monitor
      DEBUG_PRINT("Temperature: ");
      DEBUG_PRINTLN(temperature);
    }
    else
    {
      Serial.print("ERROR JSON-a: ");
      Serial.println(error.c_str());
    }
  }
  else
  {
    Serial.print("HTTP ERROR ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void drawTemperature()
{
  // draw temperature
  sprite.setTextDatum(4);
  sprite.loadFont(bigFont);

  // Determine color based on temperature
  uint16_t tempColor;
  if (temperature < 0)
  {
    tempColor = tft.color565(0, 128, 128); // Teal for cold
  }
  else if (temperature < 10)
  {
    tempColor = tft.color565(0, 255, 0); // Bright green for temperate
  }
  else if (temperature < 20)
  {
    tempColor = tft.color565(255, 165, 0); // Orange for warm
  }
  else if (temperature >= 28)
  {
    tempColor = tft.color565(255, 0, 0); // Bright red for hot
  }
  else
  {
    tempColor = tft.color565(255, 255, 0); // Yellow for in-between
  }

  sprite.setTextColor(tempColor, TFT_BLACK);
  sprite.drawFloat(temperature, 1, 69, 80);
  sprite.unloadFont();
}

void draw()
{

  errSprite.fillSprite(grays[10]);
  errSprite.setTextColor(grays[1], grays[10]);
  errSprite.drawString(Wmsg, ani, 4);

  sprite.fillSprite(TFT_BLACK);
  sprite.drawLine(138, 10, 138, 164, grays[6]);
  sprite.drawLine(100, 108, 134, 108, grays[6]);
  sprite.setTextDatum(0);

  // LEFTSIDE
  sprite.loadFont(midleFont);
  sprite.setTextColor(grays[1], TFT_BLACK);
  sprite.drawString("WEATHER", 6, 10);
  sprite.unloadFont();

  sprite.loadFont(font18);
  sprite.setTextColor(grays[7], TFT_BLACK);
  sprite.drawString("TOWN:", 6, 110);
  sprite.setTextColor(grays[2], TFT_BLACK);
  if (units == "metric")
    sprite.drawString("C", 14, 50);
  if (units == "imperial")
    sprite.drawString("F", 14, 50);

  sprite.setTextColor(grays[3], TFT_BLACK);
  sprite.drawString(town, 46, 110);
  sprite.fillCircle(8, 52, 2, grays[2]);
  sprite.unloadFont();

  // draw wime without seconds
  sprite.loadFont(tinyFont);
  sprite.setTextColor(grays[4], TFT_BLACK);
  sprite.drawString(rtc.getTime().substring(0, 5), 6, 132);
  sprite.unloadFont();

  // draw some static text
  sprite.setTextColor(grays[5], TFT_BLACK);
  sprite.drawString("INTERNET", 86, 10);
  sprite.drawString("STATION", 86, 20);
  sprite.setTextColor(grays[7], TFT_BLACK);
  sprite.drawString("SECONDS", 92, 157);

  drawTemperature();

  // draw sec rectangle
  sprite.fillRoundRect(90, 132, 42, 22, 2, grays[2]);
  // draw seconds
  sprite.loadFont(font18);
  sprite.setTextColor(TFT_BLACK, grays[2]);
  sprite.drawString(rtc.getTime().substring(6, 8), 111, 144);
  sprite.unloadFont();

  sprite.drawFastHLine(316, 0, 3, (_mqttIsConnected) ? TFT_GREEN: TFT_RED);
  sprite.drawFastHLine(0, 0, 3, (WiFi.status() == WL_CONNECTED) ? TFT_GREEN: TFT_RED);
  

  sprite.setTextDatum(0);
  // RIGHT SIDE
  sprite.loadFont(font18);
  sprite.setTextColor(grays[1], TFT_BLACK);
  sprite.drawString("LAST 12 HOURS", 144, 10);
  sprite.unloadFont();

  sprite.fillRect(144, 28, 84, 2, grays[10]);

  sprite.setTextColor(grays[3], TFT_BLACK);
  sprite.drawString("MIN:" + String(minT), 254, 10);
  sprite.drawString("MAX:" + String(maxT), 254, 20);
  sprite.fillSmoothRoundRect(144, 34, 174, 60, 3, grays[11], bck);
  sprite.drawLine(170, 39, 170, 88, TFT_WHITE);
  sprite.drawLine(170, 88, 314, 88, TFT_WHITE);

  sprite.setTextDatum(4);

  for (int j = 0; j < 24; j++)
    for (int i = 0; i < PPgraph[j]; i++)
      sprite.fillRect(173 + (j * 6), 83 - (i * 4), 4, 3, grays[2]);

  sprite.setTextColor(grays[2], grays[10]);
  sprite.drawString("MAX", 158, 42);
  sprite.drawString("MIN", 158, 86);

  sprite.loadFont(font18);
  sprite.setTextColor(grays[7], grays[10]);
  sprite.drawString("T", 158, 58);
  sprite.unloadFont();

  for (int i = 0; i < 3; i++)
  {
    sprite.fillSmoothRoundRect(144 + (i * 60), 100, 54, 32, 3, grays[10], bck);
    sprite.setTextColor(grays[3], grays[9]);
    sprite.drawString(PPlbl[i], 144 + (i * 60) + 27, 107);
    sprite.setTextColor(grays[1], grays[9]);
    sprite.loadFont(font18);
    sprite.drawString(String((int)wData[i]) + PPlblU[i], 144 + (i * 60) + 27, 124);
    sprite.unloadFont();

    sprite.fillSmoothRoundRect(144, 148, 174, 16, 2, grays[11], bck);
    errSprite.pushToSprite(&sprite, 148, 150);
  }
  sprite.setTextColor(grays[4], bck);
  sprite.drawString("CURRENT WEATHER", 190, 141);
  sprite.setTextColor(grays[9], bck);
  sprite.drawString(String(counter), 310, 141);

  sprite.pushSprite(0, 0);
}

void updateData()
{

  // update always
  // part needed for scroling weather msg
  ani--;
  if (ani < -420)
    ani = 100;

  //

  if (millis() > _timePassed + 180000)
  {
    _timePassed = millis();
    counter++;
    getData();

    if (counter == 1)
      _windowStartTemp = temperature;

    if (counter == 10)
    {
      setTime();
      counter = 0;
      maxT = -50;
      minT = 1000;
      PPpower[23] = _windowStartTemp;
      for (int i = 23; i > 0; i--)
        PPpower[i - 1] = PPpowerT[i];

      for (int i = 0; i < 24; i++)
      {
        PPpowerT[i] = PPpower[i];
        if (PPpower[i] < minT)
          minT = PPpower[i];
        if (PPpower[i] > maxT)
          maxT = PPpower[i];
      }

      for (int i = 0; i < 24; i++)
      {
        PPgraph[i] = map(PPpower[i], minT, maxT, 0, 12);
      }
    }
  }
}

/***************************************************
  MQTT
****************************************************/
void mqttTransmitCustomSubscribe() {
  DEBUG_PRINT("mqttTransmitCustomSubscribe: Subscribing to custom MQTT topics...");
  xSemaphoreTake(_mqttMutex, portMAX_DELAY);
  _mqttClient.subscribe(_mqttTopic.c_str());
  xSemaphoreGive(_mqttMutex);
}


void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  char message_buff[100];
  int i = 0;
  for (i = 0; i < length; i++)
  {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String __payloadString = String(message_buff);

  DEBUG_PRINTLN(__payloadString);

  String __incomingTopic = String(topic);

  _lastMQTTMessage = __incomingTopic + " " + __payloadString;

  if (__incomingTopic == "cmnd/" + String(MQTT_CLIENTNAME) + "/reset")
  {
    DEBUG_PRINTLN("Resetting ESP");
    ESP.restart();
  }
  if (__incomingTopic == "cmnd/" + String(MQTT_CLIENTNAME) + "/info")
  {
    mqttTransmitInitStat();
  }
  if (__incomingTopic == "cmnd/mcmddevices/brightness")
  {
    _brightness = __payloadString.toInt();
    DEBUG_PRINTLN("Setting Brightness to: " + String(_brightness));
    setBrightness(_brightness);
  }

  if (__incomingTopic == "cmnd/mcmddevices/brightnesspercentage")
  {
    _brightness = __payloadString.toInt();
    _brightness = map(_brightness, 0, 100, 0, 255);
    DEBUG_PRINTLN("Setting Brightness to: " + String(_brightness));
    setBrightness(_brightness);
  }

  if (__incomingTopic == _mqttTopic)
  {

    DEBUG_PRINTLN("Received weather JSON: " + __payloadString);
    // Parse JSON payload for "temperature", "pressure", and "humidity"
    StaticJsonDocument<256> mqttDoc;
    DeserializationError mqttError = deserializeJson(mqttDoc, __payloadString);
    if (!mqttError)
    {
      if (mqttDoc.containsKey("temperature"))
      {
        temperature = mqttDoc["temperature"].as<float>();
      }
      if (mqttDoc.containsKey("humidity"))
      {
        wData[0] = mqttDoc["humidity"].as<float>();
      }
      if (mqttDoc.containsKey("pressure"))
      {
        wData[1] = mqttDoc["pressure"].as<float>();
      }
      DEBUG_PRINTLN("Parsed MQTT JSON: temperature=" + String(temperature) + ", humidity=" + String(wData[0]) + ", pressure=" + String(wData[1]));
      _updatedTime = rtc.getTime();
    }
    else
    {
      DEBUG_PRINTLN("Failed to parse MQTT JSON: " + String(mqttError.c_str()));
    }
  }
  if (__incomingTopic == WEATHER_WINDSPEED_MQTT_TOPIC)
  {
    float __windSpeed = __payloadString.toFloat();
    DEBUG_PRINTLN("Received wind speed: " + String(__windSpeed));
    wData[2] = __windSpeed;
  }
}

void mqttTransmitCustomStat() {}

void setupWebServer()
{
  DEBUG_PRINTLN("Handling Web Request...");

  _httpServer.on("/", []()
                 {
					   String __infoStr = "<html><head>"+style;
             __infoStr += "<script>  ";
             __infoStr += "function checkFlipped() {      document.getElementById('flipscreen').value=document.getElementById('flipscreenHidden').checked;  }";
             __infoStr += "function submitForm() { checkFlipped();    document.getElementById('myForm').submit(); }";
             __infoStr +="</script>";
             __infoStr += "</head>";
					   __infoStr += "<div align=left><H1><i>" + String(MQTT_CLIENTNAME) + "</i></H1>";
             __infoStr += loginIndex+loginIndex2;

					   __infoStr += "<hr class='new5'>";
             __infoStr += "<form action='/set' id='myForm'>";
             
             
					   __infoStr += "Vertically Flip Screen:&nbsp;&nbsp;<input id='flipscreenHidden' onclick='checkFlipped()' data-lpignore='true' name='flipscreenHidden' type='checkbox' value='true' width=20% ";
             __infoStr +=  String(_configFlipSreen==3?"checked":"")+"><input type='hidden' name='flipscreen' id='flipscreen' value='false' /><br>";

            __infoStr += "Screen brightness:&nbsp;&nbsp;";
            __infoStr += "<select id='brightness' name='brightness'>";
            for (int i = 0; i < 5; i++)
            {
                __infoStr += "<option value='"+String(_brightnesses[i])+"'"+ (_selectedBrightness==i?"selected='selected'":"") +">"+String(map(_brightnesses[i], 0, 255, 0, 100))+"%</option>";
            }
            
            __infoStr += "</select><br>";

             __infoStr += "(z2m) mqtt topic for TPH JSON:&nbsp;&nbsp;<input id='mqtt_topic' name='mqtt_topic' type='textbox' value='"+String(_mqttTopic)+"' width=80% ><br>";


             __infoStr += "<input type='submit' class='btn' value='Save setting(s)'>";
             __infoStr += "</form>";

         
					   __infoStr += "<hr class='new5'>Connected to: " + WiFi.SSID() + " (" + _rssiQualityPercentage + "%)<br>";
					   __infoStr += "Last Message Received:  <i>" + _lastMQTTMessage;
					   __infoStr += "</i><br>Last Message Published: <i>" + _lastPublishedMQTTMessage;

					   __infoStr += "</i><br><hr  class='new5'>IP Address: " + IpAddress2String(WiFi.localIP());
					   __infoStr += "<br>MAC Address: " + WiFi.macAddress();
					   __infoStr += "<br>" + String(MQTT_CLIENTNAME) + " - Firmware version: <b>" + String(MCMDVERSION,1);					   
					   __infoStr += "</b></div>";

					   String __retStr = __infoStr+"</html>";

					   _httpServer.sendHeader("Connection", "close");
					   _httpServer.send(200, "text/html", __retStr); });

  _httpServer.on("/serverIndex", HTTP_GET, []()
                 {
					   _httpServer.sendHeader("Connection", "close");
					   _httpServer.send(200, "text/html", serverIndex); });

  _httpServer.on("/reset", []()
                 {
					   String _webClientReturnString = "Resetting device";
					   _httpServer.send(200, "text/plain", _webClientReturnString);
					   ESP.restart();
					   delay(1000); });
  _httpServer.on("/resetSettings", []()
                 {
                   String _webClientReturnString = "Resetting Settings";
                   _httpServer.send(200, "text/plain", _webClientReturnString);

                   if (SPIFFS.exists("/config.ini"))
                   {
                     DEBUG_PRINTLN("Removing Configuration files from SPIFFS");
                     SPIFFS.remove("/config.ini");
                   } });

  _httpServer.on("/defaults", []()
                 {
                    String _webClientReturnString = "Resetting device to defaults";
                    _httpServer.send(200, "text/plain", _webClientReturnString); });

  /*handling uploading firmware file */
  _httpServer.on(
      "/update", HTTP_POST, []()
      {
			_httpServer.sendHeader("Connection", "close");
			_httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
			ESP.restart(); },
      []()
      {
        HTTPUpload &upload = _httpServer.upload();
        if (upload.status == UPLOAD_FILE_START)
        {
          DisplayOut("Updating Firmware");
          DEBUG_PRINT("Update: ");
          DEBUG_PRINTLN(upload.filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN))
          { // start with max available size
            Update.printError(Serial);
          }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
          //_updatingFirmware = true;
          /* flashing firmware to ESP*/
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          {
            Update.printError(Serial);
          }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
          if (Update.end(true))
          { // true to set the size to the current progress
            DEBUG_PRINTLN("Update Success:" + String(upload.totalSize) + "\nRebooting...\n");
            //_updatingFirmware = false;
          }
          else
          {
            Update.printError(Serial);
          }
        }
      });

  _httpServer.on("/set", HTTP_GET, []()
                 {
			String __retMessage = "";
			String __val = "";
			bool __update = false;

			for (uint8_t i = 0; i < _httpServer.args(); i++) {
				__val = _httpServer.arg(i);
				String __key = _httpServer.argName(i);
				__update = parseConfigValue(__key, __val);
				__retMessage += " " + _httpServer.argName(i) + ": " + _httpServer.arg(i) + (__update ? " set." : " not set.") + "\n";
			}
			_httpServer.send(200, "text/plain", __retMessage);

			if (__update) {
				saveConfigValuesSPIFFS();
			} });

  _httpServer.on("/favicon.ico", HTTP_GET, []()
                 {
                   _httpServer.send(204); // No Content
                 });

  _httpServer.onNotFound([]
                         {
                           handleSendToRoot(); // send to root page}
                         });

  _httpServer.begin();

  DEBUG_PRINTLN("Web Request Completed...");
}

void updateLocalTime()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    return;
  }

  adjustBST(&timeinfo);

  strftime(timeHour, 3, "%H", &timeinfo);
  strftime(timeMin, 3, "%M", &timeinfo);
  strftime(timeSec, 3, "%S", &timeinfo);
  /*ú
   strftime(timeWeekDay,10, "%A", &timeinfo);
   strftime(timeday, 3, "%d", &timeinfo);
   strftime(timemonth, 10, "%B", &timeinfo);
   strftime(timeyear, 5, "%Y", &timeinfo);
  */
}

void rebootESP()
{
  ESP.restart();
}

bool setupWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);

  DEBUG_PRINTLN("setupWifi: Scanning for WiFi networks...");
  int networkCount = WiFi.scanNetworks();
  String targetSSID = getWiFIAPName();
  bool targetFound = false;
  if (networkCount == 0)
  {
    DEBUG_PRINTLN("setupWifi: No networks found.");
  }
  else
  {
    DEBUG_PRINTLN("setupWifi: Networks found: " + String(networkCount) + "  (target='" + targetSSID + "')");
    for (int i = 0; i < networkCount; i++)
    {
      String ssid = WiFi.SSID(i);
      bool isTarget = (ssid == targetSSID);
      if (isTarget) targetFound = true;
      DEBUG_PRINTLN("  " + String(isTarget ? ">>>" : "   ") + " [" + String(i + 1) + "] SSID: '" + ssid + "'"
                    "  RSSI: " + String(WiFi.RSSI(i)) + " dBm"
                    "  Ch: " + String(WiFi.channel(i)) +
                    (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "  [open]" : "  [encrypted]"));
    }
    if (!targetFound)
      DEBUG_PRINTLN("setupWifi: WARNING - target SSID '" + targetSSID + "' NOT found in scan!");
  }
  WiFi.scanDelete();

  DisplayOut("Initialising WiFi: 1st AP");
  DisplayOut(getWiFIAPName());

  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Use full TX power for better link budget
  

  if (!isWiFiConnected(_mqttClientId))
  {
    flipAPDetails();
    DisplayOut("Initialising WiFi: 2nd AP");
    DisplayOut(getWiFIAPName());

    if (!isWiFiConnected(_mqttClientId))
    {
      DisplayOut("WiFi connection failed");
      return false;
    }

  }
  
  if(WiFi.status() == WL_CONNECTED)
  {
    DisplayOut("WiFi connected");
    DisplayOut("IP: " + WiFi.localIP().toString());
    DisplayOut("SSID: " + WiFi.SSID());
    DisplayOut("Signal: " + String(WiFi.RSSI()) + " dBm");
    return true;
  }

  DisplayOut("WiFi connection failed");
  return false;
}

void keepWiFiAlive(void *parameters)
{
	for (;;)
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			DEBUG_PRINTLN("keepWiFiAlive: WiFi connected");
			vTaskDelay(10000 / portTICK_PERIOD_MS);
			continue;
		}

    DEBUG_PRINTLN("keepWiFiAlive: WiFi not connected, attempting reconnect...");
    setupWifi();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
	}
}

void keepMQTTAlive(void *parameters)
{
	for (;;)
	{
		xSemaphoreTake(_mqttMutex, portMAX_DELAY);
		bool __connected = _mqttClient.connected();
		if (__connected)
		{
			_mqttClient.loop();
		}
		_mqttIsConnected = __connected;
		xSemaphoreGive(_mqttMutex);

		if (__connected)
		{
			// Run frequently so keepalive PINGREQs and inbound messages are
			// serviced promptly - this is what the broker's keepalive timeout
			// depends on, so starving this loop is what causes disconnects.
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}

		DEBUG_PRINTLN("keepMQTTAlive: MQTT not connected, attempting reconnect...");
		mqttReconnect();
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void setup()
{
  Serial.begin(115200);
  DEBUG_PRINTLN("Setup: Starting setup function...");

  DEBUG_PRINTLN("Setup: Initializing SPIFFS...");
  setupSPIFFS();
  loadCustomParamsSPIFFS();

  DEBUG_PRINTLN("Setup: Powering on LCD panel...");
  pinMode(LCD_POWER_ON_PIN, OUTPUT);
  digitalWrite(LCD_POWER_ON_PIN, HIGH);

  DEBUG_PRINTLN("Setup: Initializing TFT display...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  DisplayOut("Initializing");

  
  DEBUG_PRINTLN("Setup: Creating main sprite...");
  sprite.createSprite(320, 170);
  DEBUG_PRINTLN("Setup: Main sprite created.");
  errSprite.createSprite(164, 15);

  //_mqttPostFix = String(random(0xffff), HEX);
  _mqttClientId = MQTT_CLIENTNAME;
  _deviceClientName = MQTT_CLIENTNAME;


  DEBUG_PRINTLN("Setup: Setting up buttons");
  _button1.attachClick(rotateBrightness);
  _button2.attachDuringLongPress(rebootESP);

  // connect board to wifi , if cant, esp32 will make wifi network, connect to that network with password "password"
  DisplayOut("Starting WiFi");

  bool __wifiConnected = setupWifi();

  if (__wifiConnected)
  {
    DEBUG_PRINTLN("Setup: WiFi connected.");
    DisplayOut("Connected.");
  }
  else
  {
    DEBUG_PRINTLN("Setup: WiFi not connected. Background reconnect task will continue retrying.");
    DisplayOut("WiFi reconnect running in background");
  }

  // set brightness
  DEBUG_PRINTLN("Setup: Setting up LEDC for brightness...");
  ledcSetup(0, 10000, 8);
  ledcAttachPin(38, 0);
  ledcWrite(0, 255);


  DisplayOut("Setting time...");
  setTime();

  DisplayOut("Getting weather data...");
  getData();

  DisplayOut("Web Server config");
  DEBUG_PRINTLN("Setup: Setting up web server...");
  setupWebServer();

  DisplayOut("Configuring MQTT");
  DEBUG_PRINTLN("Setup: Setting up MQTT...");
  setupMQTT();
  mqttSendInitStat();

  DisplayOut("Setup Time Server");
  DEBUG_PRINTLN("Setup: Checking BST...");
  checkBST();

  DisplayOut("Attempting MQTT: ");
  DisplayOut(String(MQTT_SERVER_IP) + ":1883");
  DEBUG_PRINTLN("Setup: Setting MQTT server and callback...");
  //_mqttClient.setServer(MQTT_SERVER_IP, 1883);
  _mqttClient.setCallback(mqttCallback);

  // Start the server
  // DEBUG_PRINT(F("********** Free Heap: "));   DEBUG_PRINTLN(ESP.getFreeHeap());
  // DisplayOut("Web Server starting");
  // DEBUG_PRINTLN("Setup: Starting HTTP server...");
  // _httpServer.begin();

  // generate 13 levels of gray
  DEBUG_PRINTLN("Setup: Generating grayscale colors...");
  int co = 255;
  for (int i = 0; i < 13; i++)
  {
    grays[i] = tft.color565(co, co, co);
    co = co - 20;
    if (grays[i] < 0)
      grays[i] = 0;
  }


	// Run on the core opposite to loop() (which does blocking HTTP/draw work) so
	// MQTT keepalive/reconnect isn't starved when loop() stalls, and give it a
	// slightly higher priority than the default loop task.
	xTaskCreatePinnedToCore(keepMQTTAlive, "MQTTConnect", 4096, NULL, 2, NULL, !CONFIG_ARDUINO_RUNNING_CORE);
	xTaskCreatePinnedToCore(keepWiFiAlive, "WiFiConnect", 4096, NULL, 2, NULL, !CONFIG_ARDUINO_RUNNING_CORE);


  DisplayOut("Setup function complete.");
}

void loop()
{

  updateData();
  draw();
  _button1.tick();
  _button2.tick();
  ArduinoOTA.handle();        /* this function will handle incomming chunk of SW, flash and respond sender */
  _httpServer.handleClient(); //// Check if a client has connected

}
