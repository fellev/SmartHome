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
#define D_WH_MAIN_WEB_FORM_INPUT_NUM                			11

/* Buttons */
#define D_WH_MAIN_BTNCOUNT                                      1
#define D_WH_MAIN_BUTTON_PRESSED                                0
#define D_WH_MAIN_BUTTON_RELEASED                               1
#define D_WH_MAIN_BUTTON_BOUNCE_MS                              200  //timespan before another button change can occur
#define D_WH_MAIN_BUTTON_HOLD_TIME								1000 //ms

/* MQTT */
#define D_WH_MAIN_MQTT_TOPIC_PREFIX	                            "/CONTROLLERS/" PRODUCT_NAME
#define D_WH_MAIN_MQTT_TOPIC_IN                                 D_WH_MAIN_MQTT_TOPIC_PREFIX "/XXX/H2D"
#define D_WH_MAIN_MQTT_TOPIC_OUT                                D_WH_MAIN_MQTT_TOPIC_PREFIX "/XXX/D2H/"
#define D_WH_MAIN_MQTT_TOPIC_TEMPLATE_POWER                     D_WH_MAIN_MQTT_TOPIC_OUT "PWR"
#define D_WH_MAIN_MQTT_TOPIC_TEMPLATE_TIMER                     D_WH_MAIN_MQTT_TOPIC_OUT "TMR"
#define D_WH_MAIN_MQTT_TOPIC_TEMPLATE_STATUS                    D_WH_MAIN_MQTT_TOPIC_OUT "STATUS"
#define D_WH_MAIN_MQTT_TOPIC_MAX_SIZE							70
#define D_WH_MAIN_MQTT_TOPIC_NODE_ID_OFFSET						sizeof(D_WH_MAIN_MQTT_TOPIC_PREFIX)
#define D_WH_MAIN_MQTT_TOPIC_NODE_ID_SIZE                       3
#define D_WH_MAIN_MQTT_CONNECTION_RETRY_TIME                    5000 //ms
#define D_WH_MAIN_MQTT_STATUS_SEND_PWR_BIT                      0
#define D_WH_MAIN_MQTT_STATUS_SEND_TIMER_BIT                    1

/* Timers */
#define D_WH_MAIN_SEND_STATUS_RATE                              (1000*60)
#define D_WH_MAIN_UNALLOCATED_TIMER                             SimpleTimer::MAX_TIMERS

/* Leds */
#define D_WH_MAIN_LED_PULSE_PERIOD                              5000   //5s
#if defined(D_WH_MAIN_LED_ACTIVE_MODE_HIGH)
#define D_WH_MAIN_LED_ON_VALUE                                  PWMRANGE
#define D_WH_MAIN_LED_OFF_VALUE                                 0
#else
#define D_WH_MAIN_LED_ON_VALUE                                  0
#define D_WH_MAIN_LED_OFF_VALUE                                 PWMRANGE
#endif

#define D_WH_MAIN_POWER_ON_TIME_DEFAULT                         30 //min

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
//String f_whMainAppendNumber(String in_str, byte num);
void f_whMainLedPulseStart( byte numOfPulses );
void f_whMainLedPulseStop();
char * f_whMainMqttGetTopic(const char* topicTemplate);

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
bool configServerEnable = false;
String configServerParam;

/* Buttons */
byte btnIndex=0; // as the sketch loops this index will loop through the available physical buttons
byte btn[] = {D_HW_CONFIG_GPIO_PIN_POWER_BUTTON};
byte btnLastState[]={D_WH_MAIN_BUTTON_RELEASED};
unsigned long btnLastPress[]={0,0};
byte btnState=D_WH_MAIN_BUTTON_RELEASED;
byte btnHoldEnabled = false;

/* RTC */
long now=0;
//long countdownTimerSec = 0;
SimpleTimer timer;
int timerIdPower=D_WH_MAIN_UNALLOCATED_TIMER, timerIdStatus=D_WH_MAIN_UNALLOCATED_TIMER;

/* Power state*/
byte pwrState = D_WH_MAIN_POWER_CONTROL_OFF;
byte pwrStateOld = D_WH_MAIN_POWER_CONTROL_OFF;

/* MQTT */
//char mqttOutTopicTimerStatus[D_WH_MAIN_MQTT_TOPIC_MAX_SIZE];
//char mqttOutTopicPowerStatus[D_WH_MAIN_MQTT_TOPIC_MAX_SIZE];
//char mqttInTopic[D_WH_MAIN_MQTT_TOPIC_MAX_SIZE];
char mqttTopicBuff[D_WH_MAIN_MQTT_TOPIC_MAX_SIZE];
char mqttNodeIdStr[D_WH_MAIN_MQTT_TOPIC_NODE_ID_SIZE+1] = { '0','0','0',0 };
long mqttLastReconnectionTry=0;
byte mqttStatusSendMask = 0;

/* Leds */
bool isLedsNeedUpdate = false;
byte ledBlinksRequestedNum = 0;
byte ledBlinkCount = 0;
bool ledPulseDirection = true;
int ledPulseValue=0;
unsigned long ledPulseTimestamp=0;
bool ledPulseEnabled = false;


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
//	String topicOut = f_whMainAppendNumber(String(D_WH_MAIN_MQTT_TOPIC_PREFIX), gProductConfig.nodeID) + String(D_WH_MAIN_MQTT_TOPIC_OUT);
//    (topicOut + String(D_WH_MAIN_MQTT_TOPIC_TIMER)).toCharArray(mqttOutTopicTimerStatus, sizeof(mqttOutTopicTimerStatus)-1);
//    (topicOut + String(D_WH_MAIN_MQTT_TOPIC_POWER)).toCharArray(mqttOutTopicPowerStatus, sizeof(mqttOutTopicPowerStatus)-1);
//    (f_whMainAppendNumber(String(D_WH_MAIN_MQTT_TOPIC_PREFIX), gProductConfig.nodeID) + String(D_WH_MAIN_MQTT_TOPIC_IN)).toCharArray(mqttInTopic, sizeof(mqttInTopic)-1);
	String nodeIdStr;
	if (gProductConfig.nodeID < 100) nodeIdStr += "0";
	if (gProductConfig.nodeID < 10) nodeIdStr += "0";
	nodeIdStr += String(gProductConfig.nodeID, DEC);
	nodeIdStr.toCharArray(mqttNodeIdStr, sizeof(mqttNodeIdStr));
//    DEBUG_PRINT("MQTT out topic timer: "); DEBUG_PRINTLN(mqttOutTopicTimerStatus);
//    DEBUG_PRINT("MQTT out topic power: "); DEBUG_PRINTLN(mqttOutTopicPowerStatus);
//    DEBUG_PRINT("MQTT in topic: "); DEBUG_PRINTLN(mqttInTopic);
	client.setServer(gProductConfig.mqtt_server_ip, gProductConfig.mqtt_server_port);
	client.setCallback(f_whMainMqttCallback);

}

void f_whMainGpioInit(void) {
#if !defined(DEBUG)
	pinMode(D_HW_CONFIG_LED_ESP8266, OUTPUT);
	digitalWrite(D_HW_CONFIG_LED_ESP8266, HIGH);
#endif
	pinMode(D_HW_CONFIG_GPIO_PIN_WAHTER_HEATER_RELAY, OUTPUT);digitalWrite(D_HW_CONFIG_GPIO_PIN_WAHTER_HEATER_RELAY, LOW);
	pinMode(D_HW_CONFIG_GPIO_PIN_POWER_BUTTON, INPUT_PULLUP); digitalWrite(D_HW_CONFIG_GPIO_PIN_POWER_BUTTON, HIGH); //activate pullup
	pinMode(D_HW_CONFIG_GPIO_PIN_LED_POWER_STATUS, OUTPUT);   analogWrite(D_HW_CONFIG_GPIO_PIN_LED_POWER_STATUS, D_WH_MAIN_LED_OFF_VALUE);
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
          configServerEnable = true;
          configServerParam = param;
          // Start the server
          server.begin();
          DEBUG_PRINTLN("Server started");
//          int b = 20;
//          while(b == 20) {
//             b = f_whLoadWebPage(webtype, param);
//           }
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
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>" PRODUCT_NAME " IP:";
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
			 "<label>MQTT Server: </label>"
			 "<input name='mqtt_server_port' length=4 size=3><p>" /*Input 8*/
             "<label>MQTT User: </label>"
             "<input name='mqtt_user' length=32> " /*Input 8*/
             "<label>Password: </label>"
             "<input name='mqtt_passwd' type='password' length=32><p>" /*Input 10*/
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
        gProductConfig_p->mqtt_server_ip[0] = inputs[4].toInt();
        gProductConfig_p->mqtt_server_ip[1] = inputs[5].toInt();
        gProductConfig_p->mqtt_server_ip[2] = inputs[6].toInt();
        gProductConfig_p->mqtt_server_ip[3] = inputs[7].toInt();
        gProductConfig_p->mqtt_server_port  = inputs[8].toInt();
        inputs[9].toCharArray(gProductConfig_p->mqtt_user, sizeof(gProductConfig_p->mqtt_user));
        inputs[10] = Utils::htmlEncodedToUtf8(inputs[10]);
        inputs[10].toCharArray(gProductConfig_p->mqtt_passwd, sizeof(gProductConfig_p->mqtt_passwd));

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

void f_whMainPowerTimerStatusSetBit() {
	D_UTILS_MASK_SET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_TIMER_BIT);
}

void f_whMainPowerTimerStatusSend() {
	char msgBuf[8] = {0};
	int remainTime = 0;
	if (timer.isEnabled(timerIdPower)) {
		remainTime = timer.getTimeToNextCall(timerIdPower);
		remainTime = remainTime / (60*1000) + (((remainTime % 1000) > 500) ? 1 : 0);
	}
	String msg = String("TMR") + String(remainTime, DEC);
	msg.toCharArray(msgBuf, sizeof(msgBuf));
	client.publish(f_whMainMqttGetTopic(D_WH_MAIN_MQTT_TOPIC_TEMPLATE_TIMER), msgBuf);
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
	  int time = atol(buf);
	  f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_ON, time);
  }
#if defined(DEBUG)
  else if (strncmp((const char*)payload, "cleareeprom", 11) == 0)
  {
      DEBUG_PRINTLN("clearing eeprom");
//      f_whMainAppendNumber(String(D_WH_MAIN_MQTT_TOPIC_PREFIX_OUT), gProductConfig.nodeID).toCharArray(mqttTopic, sizeof(mqttTopic)-1);
      client.publish(f_whMainMqttGetTopic(D_WH_MAIN_MQTT_TOPIC_TEMPLATE_STATUS),"clearing eeprom");
      delay(1000);
      memset(gProductConfig_p, 0, sizeof(*gProductConfig_p));
      EEPROM_writeAnything(EEPROM_ADDR_CONFIGURATION, *gProductConfig_p);
      EEPROM.commit();
  }
#endif
}

//String f_whMainAppendNumber(String in_str, byte num) {
//	if (in_str.length() > 0) {
//		if (in_str.charAt(in_str.length()-1) != '/') in_str += "/";
//		if (gProductConfig.nodeID < 100) in_str += "0";
//		if (gProductConfig.nodeID < 10) in_str += "0";
//		in_str += String(gProductConfig.nodeID, DEC);
//	}
//	return(in_str);
//}

char * f_whMainMqttGetTopic(const char* topicTemplate) {
	strncpy(mqttTopicBuff, topicTemplate, sizeof(mqttTopicBuff));
	memcpy(&mqttTopicBuff[D_WH_MAIN_MQTT_TOPIC_NODE_ID_OFFSET], mqttNodeIdStr, D_WH_MAIN_MQTT_TOPIC_NODE_ID_SIZE);
	return mqttTopicBuff;
}

void f_whMainMqttReconnect() {


  // Loop until we're reconnected
//  while (!client.connected()) {
	if (!client.connected() && ((now-mqttLastReconnectionTry) > D_WH_MAIN_MQTT_CONNECTION_RETRY_TIME)) {

		DEBUG_PRINT("Attempting MQTT connection...");
		// Attempt to connect
		// If you do not want to use a username and password, change next line to
		// if (client.connect("ESP8266Client")) {
		String clientName = String(PRODUCT_NAME) + String(gProductConfig.nodeID, DEC);
		clientName.toCharArray(mqttTopicBuff, sizeof(mqttTopicBuff));
		if (client.connect(mqttTopicBuff)) {
		  DEBUG_PRINTLN("connected");
		  client.publish(f_whMainMqttGetTopic(D_WH_MAIN_MQTT_TOPIC_TEMPLATE_STATUS),"CONNECTED");
		  client.subscribe(f_whMainMqttGetTopic(D_WH_MAIN_MQTT_TOPIC_IN));
		  D_UTILS_MASK_SET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_PWR_BIT);
		  D_UTILS_MASK_SET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_TIMER_BIT);
		  DEBUG_PRINT("MQTT in topic: "); DEBUG_PRINTLN(mqttTopicBuff);
		} else {
		  DEBUG_PRINT("failed, rc=");
		  DEBUG_PRINT(client.state());
//		  DEBUG_PRINTLN(" try again in 5 seconds");
		  // Wait 5 seconds before retrying
//		  delay(5000);
		  f_systemBootSetupAP();
		}
		mqttLastReconnectionTry = millis();
	}
}

void f_whMainPowerControl(byte mode, int offTimeMin, bool sendStatus) {
	timer.deleteTimer(timerIdPower);
	timer.deleteTimer(timerIdStatus);
	timerIdPower = timerIdStatus = D_WH_MAIN_UNALLOCATED_TIMER;

	if (mode == (uint8_t)D_WH_MAIN_POWER_CONTROL_OFF)
	{
		DEBUG_PRINTLN("Power OFF");
		digitalWrite(D_HW_CONFIG_GPIO_PIN_WAHTER_HEATER_RELAY, LOW);
		pwrState = mode;
		f_whMainLedPulseStop();

		if (sendStatus)
			D_UTILS_MASK_SET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_PWR_BIT);
	}
	else if (mode == (uint8_t)D_WH_MAIN_POWER_CONTROL_ON)
	{
		DEBUG_PRINTLN("Power ON");
		digitalWrite(D_HW_CONFIG_GPIO_PIN_WAHTER_HEATER_RELAY, HIGH);
		pwrState = mode;
		if (sendStatus)
			D_UTILS_MASK_SET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_PWR_BIT);

		if (offTimeMin > 0) {
			timerIdPower = timer.setTimeout(offTimeMin*1000*60, f_whMainPowerTimerTimeout);
			timerIdStatus = timer.setInterval(D_WH_MAIN_SEND_STATUS_RATE, f_whMainPowerTimerStatusSetBit);
			if (offTimeMin > 0)
				f_whMainLedPulseStart( (offTimeMin/10) ? (offTimeMin/10) : 1 );
			DEBUG_PRINT("Set timer to "); DEBUG_PRINT(offTimeMin, DEC); DEBUG_PRINTLN(" min");
		} else {
			f_whMainLedPulseStop();
			isLedsNeedUpdate = true;
		}
		D_UTILS_MASK_SET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_TIMER_BIT);
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
	    btnLastPress[btnIndex] = now;
	    if (btnState == D_WH_MAIN_BUTTON_PRESSED)
	    {
		  f_whMainPowerControl(pwrState == D_WH_MAIN_POWER_CONTROL_OFF ? D_WH_MAIN_POWER_CONTROL_ON : D_WH_MAIN_POWER_CONTROL_OFF, D_WH_MAIN_POWER_ON_TIME_DEFAULT);
	    }
	    if (btnHoldEnabled) {
	    	btnHoldEnabled = false;
	    }
	  }

	  if (!btnHoldEnabled && (btnState == D_WH_MAIN_BUTTON_PRESSED) && (now-btnLastPress[btnIndex] >= D_WH_MAIN_BUTTON_HOLD_TIME))
	  {
		  btnHoldEnabled = true;
		  if (pwrState == D_WH_MAIN_POWER_CONTROL_ON)
			  f_whMainPowerControl((byte)D_WH_MAIN_POWER_CONTROL_ON, 0);
	  }
}

inline void f_whMainLedPulseStart( byte numOfPulses ) {
#if defined(D_WH_MAIN_LED_ACTIVE_MODE_HIGH)
	ledPulseDirection=false;
    ledPulseValue = -(D_WH_MAIN_LED_PULSE_PERIOD/256);
#else
    ledPulseDirection=true;
    ledPulseValue = PWMRANGE+(D_WH_MAIN_LED_PULSE_PERIOD/256);
#endif
    ledBlinkCount = 0;
    ledBlinksRequestedNum = numOfPulses;
    ledPulseEnabled = true;
}

inline void f_whMainLedPulseStop() {
    ledPulseEnabled = false;
}

void f_whMainHandleLeds() {

	if (isLedsNeedUpdate) {
		if (pwrState == D_WH_MAIN_POWER_CONTROL_ON) {
			digitalWrite(D_HW_CONFIG_LED_ESP8266, LOW);
		} else {
			digitalWrite(D_HW_CONFIG_LED_ESP8266, HIGH);
		}
	}

	if (ledPulseEnabled && (ledBlinksRequestedNum > ledBlinkCount)) {
	    if (millis()-(ledPulseTimestamp) > D_WH_MAIN_LED_PULSE_PERIOD/350)
	    {
	      ledPulseValue = ledPulseDirection ? ledPulseValue + D_WH_MAIN_LED_PULSE_PERIOD/256 : ledPulseValue - D_WH_MAIN_LED_PULSE_PERIOD/256;

	      if (ledPulseDirection && ledPulseValue > PWMRANGE)
	      {
	        ledPulseDirection=false;
	        ledPulseValue = PWMRANGE;
#if defined(D_WH_MAIN_LED_ACTIVE_MODE_HIGH)
	        ledBlinkCount++;
#endif
	      }
	      else if (!ledPulseDirection && ledPulseValue < 0)
	      {
	        ledPulseDirection=true;
	        ledPulseValue = 0;
#if !defined(D_WH_MAIN_LED_ACTIVE_MODE_HIGH)
	        ledBlinkCount++;
#endif
	      }

	      analogWrite(D_HW_CONFIG_GPIO_PIN_LED_POWER_STATUS, ledPulseValue);
	      ledPulseTimestamp = millis();
//	      DEBUG_PRINT("LED:");DEBUG_PRINTLN(ledPulseValue, DEC);
	    }
	} else {
		if (ledPulseEnabled) {
			ledPulseEnabled = false;
			isLedsNeedUpdate = true;
		}
		if (isLedsNeedUpdate) {
			if (pwrState == D_WH_MAIN_POWER_CONTROL_ON) {
				analogWrite(D_HW_CONFIG_GPIO_PIN_LED_POWER_STATUS, D_WH_MAIN_LED_ON_VALUE);
			} else {
				analogWrite(D_HW_CONFIG_GPIO_PIN_LED_POWER_STATUS, D_WH_MAIN_LED_OFF_VALUE);
			}
		}
	}

	if (isLedsNeedUpdate) {
		isLedsNeedUpdate = false;
	}
}

void f_whMainHandleMqttStatus() {
	if (!client.connected())
		return;

	if (D_UTILS_MASK_GET_BIT(mqttStatusSendMask, D_WH_MAIN_MQTT_STATUS_SEND_PWR_BIT)) {
		D_UTILS_MASK_CLR_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_PWR_BIT);
		client.publish(f_whMainMqttGetTopic(D_WH_MAIN_MQTT_TOPIC_TEMPLATE_POWER),pwrState == D_WH_MAIN_POWER_CONTROL_OFF ? "OFF" : "ON");
	} else if (D_UTILS_MASK_GET_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_TIMER_BIT)) {
		D_UTILS_MASK_CLR_BIT(mqttStatusSendMask,D_WH_MAIN_MQTT_STATUS_SEND_TIMER_BIT);
		f_whMainPowerTimerStatusSend();
	}
}


void loop() {
	now = millis();
	if(configServerEnable) {
		f_whLoadWebPage(D_WH_MAIN_WEB_TYPE_CONFIGURATION, configServerParam);
	}else {
		if (!client.connected()) {
		  f_whMainMqttReconnect();
		}
		client.loop();
	}


	timer.run();

	f_whMainHandleButtons();

	if (pwrStateOld != pwrState) {
	  pwrStateOld = pwrState;
	  isLedsNeedUpdate = true;
	}

	f_whMainHandleLeds();

    f_whMainHandleMqttStatus();

}
