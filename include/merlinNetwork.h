#pragma once
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#endif

#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

#ifdef SECURE
#include <WiFiClientSecure.h>
#endif

#include "connectionDetails.h"


#include <WiFiUdp.h>
//#include <ArduinoOTA.h>

#include <PubSubClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <NTPClient.h>
#include <TimeLib.h>
#include <Time.h>


#define MQTT_MAX_PACKET_SIZE 512


byte _networkConnection = 1;


#define HTTPRESPONSETIMEOUT 450

const char *_deviceClientName = "espTestDevice";

#ifdef SECURE
WiFiClientSecure _espWiFiClient;
#else
WiFiClient _espWiFiClient;
#endif

PubSubClient _mqttClient(_espWiFiClient);
String _mqttClientId = "";
String _lastReceivedMQTTMessage = "";
String _lastPublishedMQTTMessage = "";

int _mqttConnectionRetries = 0;

// _mqttClient (PubSubClient) is not thread-safe: it's driven by the keepMQTTAlive
// task but also touched from the web server / config-parsing code running on the
// loop() task, so all access to it must be serialized through this mutex.
SemaphoreHandle_t _mqttMutex = xSemaphoreCreateMutex();

// Explicit connection flag updated only inside the mutex, so the display can read
// a consistent value instead of racing PubSubClient's internal state directly.
volatile bool _mqttIsConnected = false;

byte _ledBuiltIn = 12;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
WiFiUDP ntpUDP;
// const byte offset = 2;
#define OFFSET 1
const char* _timeServer = "pool.ntp.org";
NTPClient _timeClient(ntpUDP, _timeServer, OFFSET, 60000);
unsigned long _unixTime;
String _ntpDate = "";
char timeHour[3];
char timeMin[3];
char timeSec[3];
char timeday[3];
char timemonth[10];
char timeyear[5];
char timeWeekDay[10];
int dayInWeek;


// Measure Signal Strength (RSSI) of Wi-Fi connection
long _rssi = 0;
// Measure Signal Strength (RSSI) of Wi-Fi connection
long _rssiQualityPercentage = 0;

const int RSSI_MAX = -50;  // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

// void ICACHE_RAM_ATTR AS3935Irq();
int dBmtoPercentage(int dBm)
{
	int quality = 2 * (dBm + 100);

	if (dBm <= RSSI_MIN)
	{
		quality = 0;
	}
	else if (dBm >= RSSI_MAX)
	{
		quality = 100;
	}

	return quality;
} // dBmtoPercentage

String macToStr(const uint8_t *mac)
{
	String result;
	for (int i = 0; i < 6; ++i)
	{
		result += String(mac[i], 16);
	}
	return result;
}

void updateTimeString()
{
	//_ntpDate = String(year(_unixTime)) + "/" + String(month(_unixTime)) + "/" + String(day(_unixTime));
	_ntpDate = _timeClient.getFormattedDate();
	_ntpDate = _ntpDate.substring(0, 16);
	_ntpDate.trim();
	DEBUG_PRINTLN(_ntpDate);
}

String getTimefromEpoch(long int timeData)
{

	time_t t = timeData;
	// time_t Z = 1515675789; //linux epoch time
	/*
	printDigits(day(t));
	tft.print("/");
	printDigits(month(t));
	tft.print("/");
	tft.print(year(t));
	tft.print(" ");
	*/
	byte __hour = (hour(t) + OFFSET) >= 24 ? hour(t) + OFFSET - 24 : hour(t) + OFFSET;
	return String(__hour) + ":" + (minute(t) < 10 ? "0" : "") + String(minute(t));
	;
}

unsigned long webUnixTime(Client &client)
{
	unsigned long time = 0;

	// Just choose any reasonably busy web server, the load is really low
	if (client.connect("g.cn", 80))
	{
		// Make an HTTP 1.1 request which is missing a Host: header
		// compliant servers are required to answer with an error that includes
		// a Date: header.
		client.print(F("GET / HTTP/1.1 \r\n\r\n"));

		char buf[5]; // temporary buffer for characters
		client.setTimeout(500);
		if (client.find((char *)"\r\nDate: ") // look for Date: header
			&& client.readBytes(buf, 5) == 5) // discard
		{
			unsigned day = client.parseInt(); // day
			client.readBytes(buf, 1);		  // discard
			client.readBytes(buf, 3);		  // month
			int year = client.parseInt();	  // year
			byte hour = client.parseInt();	  // hour
			byte minute = client.parseInt();  // minute
			byte second = client.parseInt();  // second

			int daysInPrevMonths;
			switch (buf[0])
			{
			case 'F':
				daysInPrevMonths = 31;
				break; // Feb
			case 'S':
				daysInPrevMonths = 243;
				break; // Sep
			case 'O':
				daysInPrevMonths = 273;
				break; // Oct
			case 'N':
				daysInPrevMonths = 304;
				break; // Nov
			case 'D':
				daysInPrevMonths = 334;
				break; // Dec
			default:
				if (buf[0] == 'J' && buf[1] == 'a')
					daysInPrevMonths = 0; // Jan
				else if (buf[0] == 'A' && buf[1] == 'p')
					daysInPrevMonths = 90; // Apr
				else
					switch (buf[2])
					{
					case 'r':
						daysInPrevMonths = 59;
						break; // Mar
					case 'y':
						daysInPrevMonths = 120;
						break; // May
					case 'n':
						daysInPrevMonths = 151;
						break; // Jun
					case 'l':
						daysInPrevMonths = 181;
						break; // Jul
					default:   // add a default label here to avoid compiler warning
					case 'g':
						daysInPrevMonths = 212;
						break; // Aug
					}
			}

			// This code will not work after February 2100
			// because it does not account for 2100 not being a leap year and because
			// we use the day variable as accumulator, which would overflow in 2149
			day += (year - 1970) * 365; // days from 1970 to the whole past year
			day += (year - 1969) >> 2;	// plus one day per leap year
			day += daysInPrevMonths;	// plus days for previous months this year
			if (daysInPrevMonths >= 59	// if we are past February
				&& ((year & 3) == 0))	// and this is a leap year
				day += 1;				// add one day
										// Remove today, add hours, minutes and seconds this month
			time = (((day - 1ul) * 24 + hour) * 60 + minute) * 60 + second;
		}
	}
	// delay(10);
	client.flush();
	client.stop();

	return time;
}

void getTimeUpdate()
{
	while (!_timeClient.forceUpdate())
	{
		DEBUG_PRINTLN("Fetching Time Update");
		delay(350);
	}
	DEBUG_PRINTLN(_timeClient.getFormattedTime());

	_unixTime = webUnixTime(_espWiFiClient);

	setTime(_unixTime);
}

void mqttCallback(char *topic, byte *payload, unsigned int length);
void mqttTransmitCustomStat();

void mqttTransmitCustomSubscribe();

void RenderHeadingtoSprite();

String IpAddress2String(const IPAddress &ipAddress)
{
	return String(ipAddress[0]) + String(".") +
		   String(ipAddress[1]) + String(".") +
		   String(ipAddress[2]) + String(".") +
		   String(ipAddress[3]);
}

void mqttSubscribe(String _topic)
{
	DEBUG_PRINTLN("Subscribing to: " + _topic);
	xSemaphoreTake(_mqttMutex, portMAX_DELAY);
	_mqttClient.subscribe(_topic.c_str());
	xSemaphoreGive(_mqttMutex);
	delay(50);
}

void mqttCustomPublish(String topic, String topicPostfix, String value, bool retain)
{

	DEBUG_PRINTLN("mqttCustom: Sending ");
	String __ret = (topic + "/" + topicPostfix);
	String __retVal = value;
	DEBUG_PRINTLN(__ret + " " + __retVal);
	xSemaphoreTake(_mqttMutex, portMAX_DELAY);
	_mqttClient.publish(__ret.c_str(), __retVal.c_str(), retain);
	xSemaphoreGive(_mqttMutex);
	_lastPublishedMQTTMessage = __ret + " " + __retVal;
	DEBUG_PRINTLN("mqttCustom: Sent");
}

void mqttPublishStat(String deviceName, String topicPostFix, String value, bool retain)
{
	String __ret = ("stat/" + deviceName + "/" + topicPostFix);
	String __retVal = value;

	xSemaphoreTake(_mqttMutex, portMAX_DELAY);
	_mqttClient.publish(__ret.c_str(), __retVal.c_str(), retain);
	xSemaphoreGive(_mqttMutex);
	_lastPublishedMQTTMessage = __ret + " " + __retVal;
	DEBUG_PRINT("MQTT Sent: ");
	DEBUG_PRINTLN(_lastPublishedMQTTMessage);
}

void mqttPublishStat(String name, String value, bool retain)
{
	mqttPublishStat(_deviceClientName, name, value, retain);
}

void mqttPublishStat(String deviceName, String name, String value)
{
	mqttPublishStat(deviceName, name, value, false);
}
void mqttPublishStat(String name, String value)
{
	mqttPublishStat(_deviceClientName, name, value);
}

void mqttHomePublish(String name, String value, bool retain)
{
	//DEBUG_PRINTLN("MQTT Sending ");
	String __ret = ("stat/home/" + name);
	DEBUG_PRINTLN(__ret + " " + value);
	xSemaphoreTake(_mqttMutex, portMAX_DELAY);
	_mqttClient.publish(__ret.c_str(), value.c_str(), retain);
	xSemaphoreGive(_mqttMutex);
	_lastPublishedMQTTMessage = __ret + " " + value;
	//DEBUG_PRINTLN("MQTT: Sent");
}

void mqttHomePublish(String name, String value)
{
	mqttHomePublish(name, value, false);
}

void mqttTransmitInitStat(String deviceName)
{
	mqttPublishStat("init", "{\"value1\":\"" + IpAddress2String(WiFi.localIP()) + "\",\"value2\":\"" + WiFi.macAddress() + "\",\"value3\":\"" + deviceName + "\"}");
}

void mqttTransmitInitStat()
{
	mqttTransmitInitStat(_deviceClientName);
}

void tickLED()
{
	// toggle state
	int state = digitalRead(LED_BUILTIN); // get the current state of GPIO1 pin
	if (WiFi.status() == WL_CONNECTED)	  // only switch LED if Wifi is connected
	{
		digitalWrite(LED_BUILTIN, !state); // set pin to the opposite state
	}
	else
	{
		// Turn the LED on (Note that LOW is the voltage level
		// but actually the LED is on; this is because
		// it is acive low on the ESP-01)
		digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
	}
}

void mqttHandleFriendlyCallback(String deviceName, String mqttIncomingTopic, String mqttIncomingPayload)
{
	if (mqttIncomingTopic == "cmnd/" + String(MQTT_FRIENDLYNAME) + "/sendstat")
	{
		mqttTransmitInitStat(deviceName);
		return;
	}
	if (mqttIncomingTopic == "cmnd/" + String(MQTT_FRIENDLYNAME) + "/reset")
	{
		mqttPublishStat(deviceName, "restart", mqttIncomingPayload);
		delay(1000);
#ifdef ESP8266
		ESP.reset();
#endif
#ifdef ESP32
		ESP.restart();
#endif
	}
}
void mqttHandleFriendlyCallback(String mqttIncomingTopic, String mqttIncomingPayload)
{
	mqttHandleFriendlyCallback(_deviceClientName, mqttIncomingTopic, mqttIncomingPayload);
}

bool mqttReconnect(String deviceName)
{
	_mqttClientId = deviceName;
	_mqttClientId += "_" + String(random(0xffff), HEX);
	DEBUG_PRINTLN("MQTT reconnecting with client id [" + _mqttClientId + "]");
	int __retryCount = 0;
	//_mqttClient.setSocketTimeout(500);
	xSemaphoreTake(_mqttMutex, portMAX_DELAY);
	while (!_mqttClient.connected() && __retryCount < 3)
	{
		DEBUG_PRINTLN("MQTT: Connection Attempt [" + String(__retryCount) + "]");
		if (_mqttClient.connect(_mqttClientId.c_str()))
		{
			DEBUG_PRINTLN("MQTT: Connected");
			_mqttIsConnected = true;

			DEBUG_PRINTLN("MQTT: Setting callback");
			_mqttClient.setCallback(mqttCallback);

			DEBUG_PRINTLN("MQTT: subscribing...");
			// Once connected, publish an announcement...
			String __outMessage = "tele/" + String(_deviceClientName) + "/alive";
			_mqttClient.publish(__outMessage.c_str(), "1");
			__outMessage = "tele/" + String(_deviceClientName) + "/ip";
			_mqttClient.publish(__outMessage.c_str(), IpAddress2String(WiFi.localIP()).c_str());

			_mqttClient.subscribe(("cmnd/" + deviceName + "/#").c_str());
			_mqttClient.subscribe(("cmnd/" + String(MQTT_FRIENDLYNAME) + "/#").c_str());
			//_mqttClient.subscribe(("stat/" + deviceName + "/#").c_str());

			xSemaphoreGive(_mqttMutex);
			mqttTransmitInitStat(deviceName);
			mqttTransmitCustomStat();
			mqttTransmitCustomSubscribe();
			return true;
		}
		else
		{
			DEBUG_PRINT("failed, rc=");
			DEBUG_PRINT(_mqttClient.state());
			DEBUG_PRINTLN(" trying again in a few seconds");
			_mqttIsConnected = false;

			//_mqttClient.disconnect();
			// Wait a few seconds before retrying
			xSemaphoreGive(_mqttMutex);
			delay(1500);
			xSemaphoreTake(_mqttMutex, portMAX_DELAY);
			__retryCount++;
		}
	}

	bool __connected = _mqttClient.connected();
	_mqttIsConnected = __connected;
	xSemaphoreGive(_mqttMutex);

	return __connected;
}

bool mqttReconnect()
{
	return mqttReconnect(_deviceClientName);
}

void flipAPDetails()
{
	_networkConnection = !_networkConnection;
}

const char* wifiStatusToString(wl_status_t status)
{
	switch (status)
	{
	case WL_IDLE_STATUS:
		return "IDLE";
	case WL_NO_SSID_AVAIL:
		return "NO_SSID_AVAIL";
	case WL_SCAN_COMPLETED:
		return "SCAN_COMPLETED";
	case WL_CONNECTED:
		return "CONNECTED";
	case WL_CONNECT_FAILED:
		return "CONNECT_FAILED";
	case WL_CONNECTION_LOST:
		return "CONNECTION_LOST";
	case WL_DISCONNECTED:
		return "DISCONNECTED";
	default:
		return "UNKNOWN";
	}
}

const char* wifiAuthToString(wifi_auth_mode_t authMode)
{
	switch (authMode)
	{
	case WIFI_AUTH_OPEN:
		return "OPEN";
	case WIFI_AUTH_WEP:
		return "WEP";
	case WIFI_AUTH_WPA_PSK:
		return "WPA_PSK";
	case WIFI_AUTH_WPA2_PSK:
		return "WPA2_PSK";
	case WIFI_AUTH_WPA_WPA2_PSK:
		return "WPA_WPA2_PSK";
	case WIFI_AUTH_WPA2_ENTERPRISE:
		return "WPA2_ENTERPRISE";
	case WIFI_AUTH_WPA3_PSK:
		return "WPA3_PSK";
	case WIFI_AUTH_WPA2_WPA3_PSK:
		return "WPA2_WPA3_PSK";
	case WIFI_AUTH_WAPI_PSK:
		return "WAPI_PSK";
	default:
		return "UNKNOWN";
	}
}

void debugPrintNearbyNetworks(const String& targetApName)
{
	DEBUG_PRINTLN("Scanning nearby WiFi networks...");
	int networksFound = WiFi.scanNetworks(false, true);

	if (networksFound <= 0)
	{
		DEBUG_PRINTLN("No WiFi networks found");
		return;
	}

	DEBUG_PRINT("Found ");
	DEBUG_PRINT(networksFound);
	DEBUG_PRINTLN(" network(s):");

	for (int i = 0; i < networksFound; i++)
	{
		String ssid = WiFi.SSID(i);
		int rssi = WiFi.RSSI(i);
		wifi_auth_mode_t authMode = WiFi.encryptionType(i);

		DEBUG_PRINT(" - ");
		DEBUG_PRINT((ssid == targetApName) ? "*" : " ");
		DEBUG_PRINT("SSID='");
		DEBUG_PRINT(ssid);
		DEBUG_PRINT("' RSSI=");
		DEBUG_PRINT(rssi);
		DEBUG_PRINT("dBm Auth=");
		DEBUG_PRINTLN(wifiAuthToString(authMode));
	}

	WiFi.scanDelete();
}

int isWiFiConnected(String deviceName, String apName, String apPassword)
{
	DEBUG_PRINTLN("Checking that the WiFi is connected");

	if (WiFi.status() == WL_CONNECTED)
	{
		DEBUG_PRINT("WiFi connected to '" + apName + "' with IP address: ");
		DEBUG_PRINTLN(WiFi.localIP());
		return true;
	}

	DEBUG_PRINTLN("\r\nConnecting to SSID='" + apName + "' (len=" + String(apName.length()) + ") password len=" + String(apPassword.length()));
	WiFi.disconnect(false, true); // eraseap=true clears saved credentials from NVS
	delay(1000); // allow driver to settle after disconnect/scan before begin
	WiFi.begin(apName.c_str(), apPassword.c_str());

	byte counter = 0;
	while (WiFi.status() != WL_CONNECTED)
	{
		wl_status_t s = WiFi.status();
		DEBUG_PRINTLN("  [" + String(counter + 1) + "/20] status=" + String((int)s) + " (" + String(wifiStatusToString(s)) + ")");
		counter++;

		if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL)
		{
			DEBUG_PRINTLN("WiFi connect failed early. Status=" + String(wifiStatusToString(s)));
			debugPrintNearbyNetworks(apName);
			return 0;
		}

		if (counter == 20)
		{
			DEBUG_PRINTLN("WiFi connect timed out. Status=" + String((int)WiFi.status()) + " (" + String(wifiStatusToString(WiFi.status())) + ")");
			debugPrintNearbyNetworks(apName);
			return 0;
		}
		delay(1000);
	}

	DEBUG_PRINTLN();
	DEBUG_PRINTLN("WiFi connected\r\n");
	DEBUG_PRINT("IP address: ");
	DEBUG_PRINTLN(WiFi.localIP());

	_rssi = WiFi.RSSI();
	DEBUG_PRINT(_rssi); // Signal strength in dBm
	DEBUG_PRINT(" dBm (");

	_rssiQualityPercentage = dBmtoPercentage(_rssi);

	DEBUG_PRINT(_rssiQualityPercentage); // Signal strength in %
	DEBUG_PRINTLN("%)");

	return 1;
}

int isWiFiConnected(String deviceName)
{
	String __apName = _networkConnection ? WIFI_ACCESSPOINT : WIFI_ACCESSPOINT1;
	String __apPassword = _networkConnection ? WIFI_ACCESSPOINT_PASSWORD : WIFI_ACCESSPOINT_PASSWORD1;

	return isWiFiConnected(deviceName, __apName, __apPassword);
}

int isWiFiConnected()
{
	return isWiFiConnected(_deviceClientName);
}

String getWiFIAPName()
{
	return (_networkConnection ? WIFI_ACCESSPOINT : WIFI_ACCESSPOINT1);
}

bool isLeapYear(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int zellersCongruence(int year, int month, int day) {
  if (month <= 2) {
    month += 12;
    year--;
  }
  
  int k = year % 100;
  int j = year / 100;
  
  int dayOfWeek = (day + 13*(month + 1)/5 + k + k/4 + j/4 + 5*j ) % 7 ;
  
  return dayOfWeek ; // Convert to Arduino's date format (Sat = 1, Sunday = 1, ...)
}

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ))     // from time-lib

int calcDayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
  uint16_t months[] = 
  {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 // days until 1st of month
  };   

  uint32_t days = year * 365;   // days until year 
  for (uint16_t i = 4; i < year; i+=4) 
  {
    if (LEAP_YEAR(i)) days++;  // adjust leap years
  }

  days += months[month-1] + day;
  if ((month > 2) && LEAP_YEAR(year)) days++;
  return days % 7;
}

int lastSunday(int month, int year)
{
	int lastDay = 31; // Initialize lastDay with a default value

	// Adjust lastDay for months with less than 31 days
	if (month == 4 || month == 6 || month == 9 || month == 11)
	{
		lastDay = 30;
	}
	else if (month == 2)
	{
		lastDay = (isLeapYear(year) ? 29 : 28);
	}
	
	
	for (int day = lastDay; day > 0; day--) {
    	//if (zellersCongruence(year, month, day) == 1) 
		if(calcDayOfWeek(year, month, day)==1)
		{
			return day;
		}
	}
	

	return lastDay;
}

bool isBST(struct tm *timeinfo)
{

	int year = 1900 + timeinfo->tm_year;
	int month = timeinfo->tm_mon + 1; // tm_mon is 0-indexed (0 for January, 11 for December)
	int dayOfMonth = timeinfo->tm_mday;

	  //if (month < 3 || month > 10) return false; // BST is not in effect
	/*
	DEBUG_PRINTLN("isBST()");
	DEBUG_PRINTLN("Year: "+String(year));
	DEBUG_PRINTLN("Month: "+String(month));
	DEBUG_PRINTLN("dayOfMonth: "+String(dayOfMonth));
	DEBUG_PRINTLN("  last sunday: 01-" + String(lastSunday(1,2024)));
	DEBUG_PRINTLN("  last sunday: 02-" + String(lastSunday(2,2024)));
	DEBUG_PRINTLN("  last sunday: 03-" + String(lastSunday(3,2024)));
	DEBUG_PRINTLN("  last sunday: 04-" + String(lastSunday(4,2024)));
	DEBUG_PRINTLN("  last sunday: 05-" + String(lastSunday(5,2024)));
	DEBUG_PRINTLN("  last sunday: 06-" + String(lastSunday(6,2024)));
	DEBUG_PRINTLN("  last sunday: 07-" + String(lastSunday(7,2024)));
	DEBUG_PRINTLN("  last sunday: 08-" + String(lastSunday(8,2024)));
	DEBUG_PRINTLN("  last sunday: 09-" + String(lastSunday(9,2024)));
	DEBUG_PRINTLN("  last sunday: 10-" + String(lastSunday(10,2024)));
	DEBUG_PRINTLN("  last sunday: 11-" + String(lastSunday(11,2024)));
	DEBUG_PRINTLN("  last sunday: 12-" + String(lastSunday(12,2024)));
	//DEBUG_PRINTLN("  last sunday: 2024 03-" + String(lastSunday(3,2024)));
	*/

	if (month > 3 && month < 10)
		return true; // BST is in effect
	// BST transition months (March and October)

	if (month == 3 )
	{
		return (dayOfMonth>= lastSunday(month, year));
	}
	if (month == 10)
	{
		return (dayOfMonth < lastSunday(month, year));
	}

	return false;
}

void checkBST()
{
	struct tm timeinfo;
#ifdef ESP32
	if (!getLocalTime(&timeinfo))
	{
		DEBUG_PRINTLN("Could not retieve local time from ESP");
		return;
	}
	bool __bst = isBST(&timeinfo);
	DEBUG_PRINTLN("BST is" + String(__bst ? "" : " not") + " applied");
#endif
}
void adjustBST(struct tm *timeinfo)
{
	if (isBST(timeinfo))
	{
		DEBUG_PRINTLN("adjustBST()");
		timeinfo->tm_hour += 1;
		if (timeinfo->tm_hour == 24)
			timeinfo->tm_hour = 0;
	}
}

void setupTimeClient()
{
	struct tm timeinfo;

#ifdef ESP32
	if (!getLocalTime(&timeinfo))
	{
		DEBUG_PRINTLN("Failed to obtain time");
	}
#endif
	bool __bst = isBST(&timeinfo);
	DEBUG_PRINTLN("Daylight Saving is " + String(__bst ? "" : "not ") + "applied");
	_timeClient.setTimeOffset(__bst ? 3600 : 0); // Set time offset based on BST
	_timeClient.begin();
	while (!_timeClient.forceUpdate())
	{
		DEBUG_PRINTLN(F("NTP delay"));
		delay(350);
	}
}

// PubSubClient::setServer() stores the raw char* it's given without copying it,
// so the backing string must outlive the client - a temporary String won't do.
static String _mqttServerIPStorage;

void setupMQTT(String deviceName, String mqttServerIP, uint16_t mqttServerPort)
{
	DEBUG_PRINT("MQTT client (" + deviceName + ") connecting to: ");
	DEBUG_PRINT(mqttServerIP.c_str());
	DEBUG_PRINT(":");
	DEBUG_PRINTLN(mqttServerPort);

	_mqttServerIPStorage = mqttServerIP;
	_mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
	_mqttClient.setServer(_mqttServerIPStorage.c_str(), mqttServerPort);
	delay(500);
	mqttReconnect(deviceName);
}

void setupMQTT(String mqttServerIP, uint16_t mqttServerPort)
{
	setupMQTT(_deviceClientName, mqttServerIP, mqttServerPort);
}

void setupMQTT()
{

	setupMQTT(_deviceClientName, MQTT_SERVER_IP, MQTT_SERVER_PORT);
}

void mqttSendInitStat()
{
	if (!_mqttIsConnected)
	{
		mqttReconnect();
	}
	if (_mqttIsConnected)
	{
		mqttTransmitInitStat();
	}
}