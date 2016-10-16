/*
 * wh_main.c
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

/***************************************************************************/
/**    INCLUDE FILES                                                      **/
/***************************************************************************/
#include "wh_main.h"
#include "boot.h"
#include "ESP8266WiFi.h"
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "utils.h"
#include <PubSubClient.h>
#include "esp12e_gpio.h"
#include "SimpleTimer.h"

/***************************************************************************/
/**    DEFINITIONS                                                        **/
/***************************************************************************/
#define D_WH_MAIN_WEB_FORM_INPUT_NUM                			10

/* GPIO */
#define D_WH_MAIN_PIN_WAHTER_HEATER_RELAY                       5
#define D_WH_MAIN_PIN_POWER_BUTTON                              4

/* Buttons */
#define D_WH_MAIN_BTNCOUNT                                      1
#define D_WH_MAIN_BUTTON_PRESSED                                0
#define D_WH_MAIN_RELEASED                                      1
#define D_WH_MAIN_BUTTON_BOUNCE_MS                              400  //timespan before another button change can occur

/* MQTT */
#define D_WH_MAIN_MQTT_TOPIC_PREFIX_IN	                        ("/CONTROLLERS/" PRODUCT_NAME "/H2D/")
#define D_WH_MAIN_MQTT_TOPIC_PREFIX_OUT                         ("/CONTROLLERS/" PRODUCT_NAME "/D2H/")
#define D_WH_MAIN_MQTT_TOPIC_MAX_SIZE							70

/* Timers */
#define D_WH_MAIN_SEND_STATUS_RATE                              (1000*60)

/***************************************************************************/
/**    MACROS                                                             **/
/***************************************************************************/

/***************************************************************************/
/**    TYPES                                                              **/
/***************************************************************************/

typedef enum
{
   D_WH_MAIN_WEB_TYPE_CONFIGURATION,
   D_WH_MAIN_WEB_TYPE_TEST
}E_WH_MAIN_WEB_TYPE;

typedef enum
{
	D_WH_MAIN_POWER_CONTROL_OFF,
	D_WH_MAIN_POWER_CONTROL_ON
}E_WH_MAIN_POWER_CONTROL_MODE;

/***************************************************************************/
/**    LOCAL PROTOTYPES                                                   **/
/***************************************************************************/
void f_systemBootWifiConnected_whMain(void);
void f_SystemBootLaunchApServer_whMain(String wifiInRange);
void f_whMainLaunchWeb(E_WH_MAIN_WEB_TYPE webtype, String param = String());
int f_whLoadWebPage(E_WH_MAIN_WEB_TYPE webtype, String param);
void f_whMainMqttCallback(char* topic, byte* payload, unsigned int length);
void f_whMainPowerControl(byte mode, int offTimeMin = 0, bool sendStatus = true);
void f_whMainGpioInit(void);
void f_whMainMqttInit(void);
String f_whMainAppendNumber(String in_str, byte num);

/***************************************************************************/
/**    EXTERNAL PROTOTYPES                                                **/
/***************************************************************************/

/***************************************************************************/
/**    GLOBAL VARIABLES                                                   **/
/***************************************************************************/

S_PRODUCT_CONFIG gProductConfig;
WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
String st;

/* Buttons */
byte btnIndex=0; // as the sketch loops this index will loop through the available physical buttons
byte btn[] = {D_WH_MAIN_PIN_POWER_BUTTON};
byte btnLastState[]={D_WH_MAIN_RELEASED};
unsigned long btnLastPress[]={0,0};
byte btnState=D_WH_MAIN_RELEASED;

/* RTC */
long now=0;
long countdownTimerSec = 0;
SimpleTimer timer;
int timerIdPower=0, timerIdStatus=0;

/* Power state*/
byte pwrState = D_WH_MAIN_POWER_CONTROL_OFF;

/* MQTT */
char mqttOutTopic[D_WH_MAIN_MQTT_TOPIC_MAX_SIZE];
char mqttInTopic[D_WH_MAIN_MQTT_TOPIC_MAX_SIZE];



/***************************************************************************/
/**    START IMPLEMENTATION                                               **/
/***************************************************************************/

void setup() {
	f_whMainGpioInit();
	gProductConfig_p = &gProductConfig;
	f_systemBootWifiConnected_p = f_systemBootWifiConnected_whMain;
	f_SystemBootLaunchApServer_p = f_SystemBootLaunchApServer_whMain;
	f_systemBootSetup();
	f_whMainMqttInit();
//	f_whMainTimerInit();
}

//void f_whMainTimerInit(void) {
//	timer.setInterval(1000, f_whMainTimerCallbackOneSec);
//}

void f_whMainMqttInit(void) {
    f_whMainAppendNumber(String(D_WH_MAIN_MQTT_TOPIC_PREFIX_OUT), gProductConfig.nodeID).toCharArray(mqttOutTopic, sizeof(mqttOutTopic)-1);
    f_whMainAppendNumber(String(D_WH_MAIN_MQTT_TOPIC_PREFIX_IN), gProductConfig.nodeID).toCharArray(mqttInTopic, sizeof(mqttInTopic)-1);
    DEBUG_PRINT("MQTT out topic: "); DEBUG_PRINTLN(mqttOutTopic);
    DEBUG_PRINT("MQTT in topic: "); DEBUG_PRINTLN(mqttInTopic);
	client.setServer(gProductConfig.mqtt_server, 1883);
	client.setCallback(f_whMainMqttCallback);

}

void f_whMainGpioInit(void) {
#if !defined(DEBUG)
	pinMode(D_HW_CONFIG_LED_ESP8266, OUTPUT);
	digitalWrite(D_HW_CONFIG_LED_ESP8266, HIGH);
#endif
	pinMode(D_WH_MAIN_PIN_WAHTER_HEATER_RELAY, OUTPUT);
	digitalWrite(D_WH_MAIN_PIN_WAHTER_HEATER_RELAY, LOW);
	pinMode(D_WH_MAIN_PIN_POWER_BUTTON, INPUT_PULLUP);digitalWrite(D_WH_MAIN_PIN_POWER_BUTTON, HIGH); //activate pullup
}

void f_systemBootWifiConnected_whMain(void) {
//	f_whMainLaunchWeb(D_WH_MAIN_WEB_TYPE_TEST);
}

void f_SystemBootLaunchApServer_whMain(String wifiInRange) {
	f_whMainLaunchWeb(D_WH_MAIN_WEB_TYPE_CONFIGURATION, wifiInRange);
}


void f_whMainLaunchWeb(E_WH_MAIN_WEB_TYPE webtype, String param) {
          DEBUG_PRINT("launchWeb:");DEBUG_PRINTLN(webtype);
          DEBUG_PRINTLN("WiFi connected");
          DEBUG_PRINTLN(WiFi.localIP());
          DEBUG_PRINTLN(WiFi.softAPIP());
          // Start the server
          server.begin();
          DEBUG_PRINTLN("Server started");
          int b = 20;
          while(b == 20) {
             b = f_whLoadWebPage(webtype, param);
           }
}

//void f_whMainLaunchWeb(E_WH_MAIN_WEB_TYPE webtype ) {
//	f_whMainLaunchWeb(webtype, NULL);
//}


int f_whLoadWebPage(E_WH_MAIN_WEB_TYPE webtype, String param) {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return(20);
  }
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("New client");

  // Wait for data from client to become available
  while(client.connected() && !client.available()){
    delay(1);
   }

  // Read the first line of HTTP request
  String req = client.readStringUntil('\r');

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    DEBUG_PRINT("Invalid request: ");
    DEBUG_PRINTLN(req);
    return(20);
   }
  req = req.substring(addr_start + 1, addr_end);
  DEBUG_PRINT("Request: ");
  DEBUG_PRINTLN(req);
  client.flush();
  String s;
  if ( webtype == D_WH_MAIN_WEB_TYPE_CONFIGURATION ) {
      if (req == "/")
      {
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
        s += ipStr;
        s += "<p>";
        s += (String)param;
        s += "<form method='get' action='a'><label>SSID: </label>"
             "<input name='ssid' length=32>" /*Input 0*/
             "<input name='pass' type='password' length=64 ><p>" /*Input 1*/
             "<label>Node ID: </label>"
             "<input name='nodeid' length=3 size=2><p>" /*Input 2*/
             "<label>Description: </label>"
             "<input name='description' length=10><p>" /*Input 3*/
             "<label>MQTT Server: </label>"
             "<input name='mqtt_server_ip1' length=3 size=2>." /*Input 4*/
             "<input name='mqtt_server_ip2' length=3 size=2>." /*Input 5*/
             "<input name='mqtt_server_ip3' length=3 size=2>." /*Input 6*/
             "<input name='mqtt_server_ip4' length=3 size=2><p>" /*Input 7*/
             "<label>MQTT User: </label>"
             "<input name='mqtt_user' length=32> " /*Input 8*/
             "<label>Password: </label>"
             "<input name='mqtt_passwd' type='password' length=32><p>" /*Input 9*/
             "<input type='submit'></form>";
        s += "</html>\r\n\r\n";
        DEBUG_PRINTLN("Sending 200");
      }
      else if ( req.startsWith("/a?ssid=") ) {
        // /a?ssid=blahhhh&pass=poooo&nodeid=1&description=booo&ap_ssid=sssss&mqtt_server_ip1=192&mqtt_server_ip2=168&mqtt_server_ip3=0&mqtt_server_ip4=52&
        DEBUG_PRINTLN("clearing eeprom");
        for (unsigned int i = 0; i < EEPROM_ADDR_END; ++i) { EEPROM.write(i, 0); }
        String inputs[D_WH_MAIN_WEB_FORM_INPUT_NUM];
        String t = req.substring(req.indexOf('=')+1);
        for ( int i = 0 ; i < D_WH_MAIN_WEB_FORM_INPUT_NUM ; i++)
        {
            inputs[i] = t.substring(0, t.indexOf('&'));
            DEBUG_PRINT("Input "); DEBUG_PRINTDEC(i) ; DEBUG_PRINT(":");
            DEBUG_PRINTLN(inputs[i]);
            t = t.substring(t.indexOf('=')+1);
        }

        gProductConfig_p->check_virgin = CHECK_VIRGIN_VALUE;
        Utils::htmlEncodedToUtf8(inputs[0]).toCharArray(gProductConfig_p->router_ssid, sizeof(gProductConfig_p->router_ssid));
        Utils::htmlEncodedToUtf8(inputs[1]).toCharArray(gProductConfig_p->router_passwd, sizeof(gProductConfig_p->router_passwd));
        gProductConfig_p->nodeID = inputs[2].toInt();
        Utils::htmlEncodedToUtf8(inputs[3]).toCharArray(gProductConfig_p->description, sizeof(gProductConfig_p->description));
        gProductConfig_p->mqtt_server[0] = inputs[4].toInt();
        gProductConfig_p->mqtt_server[1] = inputs[5].toInt();
        gProductConfig_p->mqtt_server[2] = inputs[6].toInt();
        gProductConfig_p->mqtt_server[3] = inputs[7].toInt();
        inputs[8].toCharArray(gProductConfig_p->mqtt_user, sizeof(gProductConfig_p->mqtt_user));
        inputs[9] = Utils::htmlEncodedToUtf8(inputs[9]);
        inputs[9].toCharArray(gProductConfig_p->mqtt_passwd, sizeof(gProductConfig_p->mqtt_passwd));

        EEPROM_writeAnything(EEPROM_ADDR_CONFIGURATION, *gProductConfig_p);

        EEPROM.commit();

        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 ";
        s += "Found ";
        s += req;
        s += "<p> saved to eeprom... reset to boot into new wifi</html>\r\n\r\n";

        //DEBUG_PRINTLN("Restarting...");
        //ESP.restart();
      }
      else
      {
        s = "HTTP/1.1 404 Not Found\r\n\r\n";
        DEBUG_PRINTLN("Sending 404");
      }
  }
#if 0 //defined(DEBUG)
  else
  {
      if (req == "/")
      {
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266";
        s += "<p>";
        s += "</html>\r\n\r\n";
        DEBUG_PRINTLN("Sending 200");
      }
      else if ( req.startsWith("/cleareeprom") ) {
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266";
        s += "<p>Clearing the EEPROM<p>";
        s += "</html>\r\n\r\n";
        DEBUG_PRINTLN("Sending 200");
        DEBUG_PRINTLN("clearing eeprom");
        memset(gProductConfig_p, 0, sizeof(*gProductConfig_p));
        EEPROM_writeAnything(EEPROM_ADDR_CONFIGURATION, *gProductConfig_p);
        EEPROM.commit();
      }
      else
      {
        s = "HTTP/1.1 404 Not Found\r\n\r\n";
        DEBUG_PRINTLN("Sending 404");
      }
  }
#endif

  client.print(s);
  DEBUG_PRINTLN("Done with client");
  return(20);
}

//void f_whMainTimerCallbackOneSec() {
//
//	  if (countdownTimerSec) {
//		  countdownTimerSec--;
//	  } else {
//		  if (pwrSt
//		  f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_OFF);
//	  }
//}

void f_whMainPowerTimerTimeout() {
	f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_OFF);
}

void f_whMainPowerTimerStatusSend() {
	char msgBuf[8] = {0};
	int remainTime = timer.getTimeToNextCall(timerIdPower);
	remainTime = remainTime / (60*1000) + (((remainTime % 1000) > 500) ? 1 : 0);
	String msg = String("TMR") + String(remainTime, DEC);
	msg.toCharArray(msgBuf, sizeof(msgBuf));
	client.publish(mqttOutTopic, msgBuf);
	DEBUG_PRINT("Timer status: "); DEBUG_PRINTLN(msgBuf);
}

void f_whMainMqttCallback(char* topic, byte* payload, unsigned int length) {
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (unsigned int i=0;i<length;i++) {
    DEBUG_PRINT((char)payload[i]);
  }
  DEBUG_PRINTLN();
  if (length == 2 && payload[0] == 'O' && payload[1] == 'N') { /* ON */
	  //Power ON
	  DEBUG_PRINTLN("Power ON command received");
	  f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_ON);

  }
  else if (length == 3 && payload[0] == 'O' && payload[1] == 'F' && payload[2] == 'F') { /* OFF */
	  //Power OFF
	  DEBUG_PRINTLN("Power OFF command received");
	  f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_OFF);
  }
  else if (length > 3 && length < 8 && payload[0] == 'T' && payload[1] == 'M' && payload[2] == 'R') { /*TMPxxxx: xxxx Timer number of second before turn off*/
	  char buf[5] = {0};
	  memcpy(buf, &payload[3], length - 3);
	  int timeout = atol(buf);
	  f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_ON, timeout);
  }
#if defined(DEBUG)
  else if (strncmp((const char*)payload, "cleareeprom", 11) == 0)
  {
      DEBUG_PRINTLN("clearing eeprom");
      client.publish(mqttOutTopic,"clearing eeprom");
      delay(1000);
      memset(gProductConfig_p, 0, sizeof(*gProductConfig_p));
      EEPROM_writeAnything(EEPROM_ADDR_CONFIGURATION, *gProductConfig_p);
      EEPROM.commit();
  }
#endif
}

String f_whMainAppendNumber(String in_str, byte num) {
	if (in_str.length() > 0) {
		if (in_str.charAt(in_str.length()-1) != '/') in_str += "/";
		if (gProductConfig.nodeID < 100) in_str += "0";
		if (gProductConfig.nodeID < 10) in_str += "0";
		in_str += String(gProductConfig.nodeID, DEC);
	}
	return(in_str);
}

void f_whMainMqttReconnect() {


  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client")) {
      DEBUG_PRINTLN("connected");
      client.publish(mqttOutTopic,"CONNECTED");
      f_whMainPowerControl(pwrState); // Send to host the current power state
      client.subscribe(mqttInTopic);
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void f_whMainPowerControl(byte mode, int offTimeMin, bool sendStatus) {
	timer.deleteTimer(timerIdPower);
	timer.deleteTimer(timerIdStatus);

	if (mode == (uint8_t)D_WH_MAIN_POWER_CONTROL_OFF)
	{
		DEBUG_PRINTLN("Power OFF");
		digitalWrite(D_WH_MAIN_PIN_WAHTER_HEATER_RELAY, LOW);
		digitalWrite(D_HW_CONFIG_LED_ESP8266, HIGH);
		pwrState = mode;
		if (sendStatus)
			client.publish(mqttOutTopic,"OFF");
	}
	else if (mode == (uint8_t)D_WH_MAIN_POWER_CONTROL_ON)
	{
		DEBUG_PRINTLN("Power ON");
		digitalWrite(D_WH_MAIN_PIN_WAHTER_HEATER_RELAY, HIGH);
		digitalWrite(D_HW_CONFIG_LED_ESP8266, LOW);
		pwrState = mode;
		if (sendStatus)
			client.publish(mqttOutTopic,"ON");

		if (offTimeMin > 0) {
			timerIdPower = timer.setTimeout(offTimeMin*1000*60, f_whMainPowerTimerTimeout);
			timerIdStatus = timer.setInterval(D_WH_MAIN_SEND_STATUS_RATE, f_whMainPowerTimerStatusSend);
			f_whMainPowerTimerStatusSend();
			DEBUG_PRINT("Set timer to "); DEBUG_PRINT(offTimeMin, DEC); DEBUG_PRINTLN(" min");
		}
	}

}

void f_whMainHandleButtons() {
	  btnIndex++;
	  if (btnIndex>D_WH_MAIN_BTNCOUNT-1) btnIndex=0;

	  btnState = digitalRead(btn[btnIndex]);

	  if (btnState != btnLastState[btnIndex] && now-btnLastPress[btnIndex] >= D_WH_MAIN_BUTTON_BOUNCE_MS) //button event happened
	  {
	    DEBUG_PRINT("Button ");DEBUG_PRINTDEC(btnIndex);DEBUG_PRINTLN((btnState == D_WH_MAIN_BUTTON_PRESSED) ? " pressed" : " released");
	    btnLastState[btnIndex] = btnState;
	    if (btnState == D_WH_MAIN_BUTTON_PRESSED)
	    {
	      btnLastPress[btnIndex] = now;
	      f_whMainPowerControl((byte)(pwrState == D_WH_MAIN_POWER_CONTROL_OFF ? D_WH_MAIN_POWER_CONTROL_ON : D_WH_MAIN_POWER_CONTROL_OFF));
	    }
	  }
}

void loop() {
	  if (!client.connected()) {
		  f_whMainMqttReconnect();
	  }
	  client.loop();

	  now = millis();

	  timer.run();

	  f_whMainHandleButtons();
}
