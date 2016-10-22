/*
 * boot.c
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

/***************************************************************************/
/**    INCLUDE FILES                                                      **/
/***************************************************************************/

#include "boot.h"
//#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "eeprom_config.h"
#include "DebugUtils.h"
#include "product_main.h"
#include "common_data.h"
#include <assert.h>
#include <string.h>



/***************************************************************************/
/**    DEFINITIONS                                                        **/
/***************************************************************************/
#define D_SYS_BOOT_WIFI_AP_SSID_DEFAULT              PRODUCT_NAME"_AP"

/***************************************************************************/
/**    MACROS                                                             **/
/***************************************************************************/

/***************************************************************************/
/**    TYPES                                                              **/
/***************************************************************************/

/***************************************************************************/
/**    LOCAL PROTOTYPES                                                   **/
/***************************************************************************/

int f_systemBootTestWifi(void);


/***************************************************************************/
/**    EXTERNAL PROTOTYPES                                                **/
/***************************************************************************/

void (*f_systemBootWifiConnected_p)(void);
void (*f_SystemBootLaunchApServer_p)(String wifiInRange);

/***************************************************************************/
/**    GLOBAL VARIABLES                                                   **/
/***************************************************************************/

/***************************************************************************/
/**    START IMPLEMENTATION                                               **/
/***************************************************************************/

/***************************************************************************/
/**                                                                       **/
/**   Function Name    : f_systemBootSetup                                **/
/**   Description      : This function start the boot setup               **/
/**   Input parameters : None.                                            **/
/**   Output parameters: None.                                            **/
/**   Return value     : None.                                            **/
/**   Remarks          : None.                                            **/
/***************************************************************************/
void f_systemBootSetup() {

  assert(f_systemBootWifiConnected_p != NULL);

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

  EEPROM_readAnything(EEPROM_ADDR_CONFIGURATION, *gProductConfig_p);
  if (gProductConfig_p->check_virgin!=CHECK_VIRGIN_VALUE) // virgin CONFIG, expected [0x55]
  {
    DEBUG_PRINTLN("No valid config found in EEPROM, loading defaults");
    memset(gProductConfig_p, 0, sizeof(*gProductConfig_p));
  }
  else
  {
	  DEBUG_PRINT("SSID: ");
	  DEBUG_PRINTLN(gProductConfig_p->router_ssid);

	  DEBUG_PRINT("PASS: ");
	  DEBUG_PRINTLN(gProductConfig_p->router_passwd);
  }


  if ( strlen(gProductConfig_p->router_ssid) > 1 ) {
	    // test esid
		WiFi.mode(WIFI_STA);
		WiFi.begin(gProductConfig_p->router_ssid, gProductConfig_p->router_passwd);
		if ( f_systemBootTestWifi() == 20 ) {
			f_systemBootWifiConnected_p();
			return;
		}

  }
  f_systemBootSetupAP();
}

int f_systemBootTestWifi(void) {
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

void f_systemBootSetupAP(void) {
  String st;
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
#ifdef D_SYS_BOOT_WIFI_AP_PASSWD
  WiFi.softAP(D_SYS_BOOT_WIFI_AP_SSID_DEFAULT, D_SYS_BOOT_WIFI_AP_PASSWD);
#else
  WiFi.softAP(D_SYS_BOOT_WIFI_AP_SSID_DEFAULT);
#endif
  DEBUG_PRINTLN("softap");
  DEBUG_PRINTLN("");

  if (f_SystemBootLaunchApServer_p != NULL)
	  f_SystemBootLaunchApServer_p(st);
  DEBUG_PRINTLN("over");
}

// handle diagnostic informations given by assertion and abort program execution:
void __assert(const char *__func, const char *__file, int __lineno, const char *__sexp) {
    // transmit diagnostic informations through serial link.
    Serial.println(__func);
    Serial.println(__file);
    Serial.println(__lineno, DEC);
    Serial.println(__sexp);
    Serial.flush();
    // abort program execution.
    abort();
}

