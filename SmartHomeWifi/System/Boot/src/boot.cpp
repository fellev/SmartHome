/*
 * boot.c
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

#include "boot.h"
#include "ESP8266WiFi.h"
//#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "eeprom_config.h"
#include "DebugUtils.h"
#include "product_main.h"
#include "common_data.h"
#include "utils.h"

int testWifi(void);
void launchWeb(int webtype);
void setupAP(void);
int mdns1(int webtype);

#define WIFI_AP_SSID_DEFAULT              PRODUCT_NAME"_AP"

#define WEB_FORM_INPUT_NUM                12

//MDNSResponder mdns;
WiFiServer server(80);

//char ap_ssid[32];
//const char* ap_passwd = "U4nnA%p*:Q=m4#cJ";
String st;

void setup_boot() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  EEPROM.begin(512);
  delay(10);
  DEBUG_PRINTLN();
  DEBUG_PRINTLN();
  DEBUG_PRINTLN("Startup");
  // read eeprom for ssid and pass
  DEBUG_PRINTLN("Reading EEPROM ssid");
  String esid = "";
  String epass = "";

  EEPROM_readAnything(EEPROM_ADDR_CONFIGURATION, *gProductConfig_p);
  if (gProductConfig_p->check_virgin!=CHECK_VIRGIN_VALUE) // virgin CONFIG, expected [0x55]
  {
    DEBUG_PRINTLN("No valid config found in EEPROM, writing defaults");
    memset(gProductConfig_p, 0, sizeof(*gProductConfig_p));
    gProductConfig_p->description[0]=0;
    gProductConfig_p->nodeID=0;
  }
  else
  {
    for (int i = 0; i < sizeof(gProductConfig_p->router_ssid); ++i)
    {
      esid += char(gProductConfig_p->router_ssid[i]);
    }
    for (int i = 0; i < sizeof(gProductConfig_p->router_passwd); ++i)
    {
      epass += char(gProductConfig_p->router_passwd[i]);
    }
  }


  DEBUG_PRINT("SSID: ");
  DEBUG_PRINTLN(esid);
  DEBUG_PRINTLN("Reading EEPROM pass");


  DEBUG_PRINT("PASS: ");
  DEBUG_PRINTLN(epass);
  if ( esid.length() > 1 ) {
      // test esid
	  WiFi.mode(WIFI_STA);
      WiFi.begin(esid.c_str(), epass.c_str());
      if ( testWifi() == 20 ) {
          launchWeb(0);
          return;
      }
  }
  setupAP();

}

int testWifi(void) {
  int c = 0;
  DEBUG_PRINTLN("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED) { return(20); }
    delay(500);
    DEBUG_PRINT(WiFi.status());
    c++;
  }
  DEBUG_PRINTLN("Connect timed out, opening AP");
  return(10);
}

void launchWeb(int webtype) {
          DEBUG_PRINT("launchWeb:");DEBUG_PRINTLN(webtype);
          DEBUG_PRINTLN("WiFi connected");
          DEBUG_PRINTLN(WiFi.localIP());
          DEBUG_PRINTLN(WiFi.softAPIP());
//          if (!mdns.begin("esp8266", WiFi.localIP())) {
//            DEBUG_PRINTLN("Error setting up MDNS responder!");
//            while(1) {
//              delay(1000);
//            }
//          }
//          DEBUG_PRINTLN("mDNS responder started");
          // Start the server
          server.begin();
          DEBUG_PRINTLN("Server started");
          int b = 20;
          int c = 0;
          while(b == 20) {
             b = mdns1(webtype);
           }
}

void setupAP(void) {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  DEBUG_PRINTLN("scan done");
  if (n == 0)
    DEBUG_PRINTLN("no networks found");
  else
  {
    DEBUG_PRINT(n);
    DEBUG_PRINTLN(" networks found");
    for (int i = 0; i < n; ++i)
     {
      // Print SSID and RSSI for each network found
      DEBUG_PRINT(i + 1);
      DEBUG_PRINT(": ");
      DEBUG_PRINT(WiFi.SSID(i));
      DEBUG_PRINT(" (");
      DEBUG_PRINT(WiFi.RSSI(i));
      DEBUG_PRINT(")");
      DEBUG_PRINTLN((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
     }
  }
  DEBUG_PRINTLN("");
  st = "<ul>";
  for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      st += "<li>";
      st +=i + 1;
      st += ": ";
      st += WiFi.SSID(i);
      st += " (";
      st += WiFi.RSSI(i);
      st += ")";
      st += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      st += "</li>";
    }
  st += "</ul>";
  delay(100);
//  WiFi.softAP(ap_ssid, ap_passwd);
  WiFi.softAP(WIFI_AP_SSID_DEFAULT); //TODO: Add password
  DEBUG_PRINTLN("softap");
  DEBUG_PRINTLN("");
  launchWeb(1);
  DEBUG_PRINTLN("over");
}

int mdns1(int webtype)
{
  // Check for any mDNS queries and send responses
//  mdns.update();

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
  if ( webtype == 1 ) {
      if (req == "/")
      {
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
        s += ipStr;
        s += "<p>";
        s += st;
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
        for (int i = 0; i < EEPROM_ADDR_END; ++i) { EEPROM.write(i, 0); }
        String inputs[WEB_FORM_INPUT_NUM];
        String t = req.substring(req.indexOf('=')+1);
        for ( int i = 0 ; i < WEB_FORM_INPUT_NUM ; i++)
        {
            inputs[i] = t.substring(0, t.indexOf('&'));
            DEBUG_PRINT("Input "); DEBUG_PRINTDEC(i) ; DEBUG_PRINT(":");
            DEBUG_PRINTLN(inputs[i]);
            t = t.substring(t.indexOf('=')+1);
        }

//         String qsid;
//         qsid = req.substring(8,req.indexOf('&'));
//         DEBUG_PRINTLN(qsid);
//         DEBUG_PRINTLN("");
//         String qpass;
//         qpass = req.substring(req.lastIndexOf('=')+1);
//         DEBUG_PRINTLN(qpass);
//         DEBUG_PRINTLN("");
//         DEBUG_PRINT("Passwd Afer decoding: ");
//         inputs[1] = Utils::htmlEncodedToUtf8(inputs[1]);
//         DEBUG_PRINTLN(inputs[1]);
//         DEBUG_PRINTLN("");
//
//         DEBUG_PRINTLN("writing eeprom ssid:");
//         for (int i = EEPROM_ADDR_AP_SSID; i < inputs[0].length(); ++i)
//         {
//             EEPROM.write(i, inputs[0][i]);
//             DEBUG_PRINT("Wrote: ");
//             DEBUG_PRINTLN(inputs[0][i]);
//         }
//         DEBUG_PRINTLN("writing eeprom pass:");
//         for (int i = 0; i < inputs[1].length(); ++i)
//         {
//             EEPROM.write(EEPROM_ADDR_WIFI_PASSWD+i, inputs[1][i]);
//             DEBUG_PRINT("Wrote: ");
//             DEBUG_PRINTLN(inputs[1][i]);
//         }
//         EEPROM.commit();

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
  client.print(s);
  DEBUG_PRINTLN("Done with client");
  return(20);
}

