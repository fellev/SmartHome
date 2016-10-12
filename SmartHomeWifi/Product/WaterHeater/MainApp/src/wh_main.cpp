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

/***************************************************************************/
/**    DEFINITIONS                                                        **/
/***************************************************************************/
#define D_WH_MAIN_WEB_FORM_INPUT_NUM                			12

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

/***************************************************************************/
/**    LOCAL PROTOTYPES                                                   **/
/***************************************************************************/
void f_systemBootWifiConnected_whMain(void);
void f_SystemBootLaunchApServer_whMain(String wifiInRange);
void f_whMainLaunchWeb(E_WH_MAIN_WEB_TYPE webtype, String param = String());
int f_whLoadWebPage(E_WH_MAIN_WEB_TYPE webtype, String param);

/***************************************************************************/
/**    EXTERNAL PROTOTYPES                                                **/
/***************************************************************************/

/***************************************************************************/
/**    GLOBAL VARIABLES                                                   **/
/***************************************************************************/

S_PRODUCT_CONFIG gProductConfig;
WiFiServer server(80);
//char ap_ssid[32];
//const char* ap_passwd = "U4nnA%p*:Q=m4#cJ";
String st;

/***************************************************************************/
/**    START IMPLEMENTATION                                               **/
/***************************************************************************/

void setup() {

	gProductConfig_p = &gProductConfig;
	f_systemBootWifiConnected_p = f_systemBootWifiConnected_whMain;
	f_SystemBootLaunchApServer_p = f_SystemBootLaunchApServer_whMain;
	f_systemBootSetup();
}

void f_systemBootWifiConnected_whMain(void) {
	f_whMainLaunchWeb(D_WH_MAIN_WEB_TYPE_TEST);
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
             "<label>MQTT IN Topic:  </label>"
             "<input name='in_topic' length=64 size=64><p>" /*Input 10*/
             "<label>MQTT OUT Topic: </label>"
             "<input name='out_topic' length=64 size=64><p>" /*Input 11*/
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
        Utils::htmlEncodedToUtf8(inputs[10]).toCharArray(gProductConfig_p->in_topic, sizeof(gProductConfig_p->in_topic));
        Utils::htmlEncodedToUtf8(inputs[11]).toCharArray(gProductConfig_p->out_topic, sizeof(gProductConfig_p->out_topic));

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
#if defined(DEBUG)
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

void loop() {
  // put your main code here, to run repeatedly:

}
