/*
 * product_main.h
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

#ifndef PRODUCT_WATERHEATER_MAINAPP_INC_PRODUCT_MAIN_H_
#define PRODUCT_WATERHEATER_MAINAPP_INC_PRODUCT_MAIN_H_

#include <Arduino.h>

#if !defined(PRODUCT_NAME)
#define PRODUCT_NAME "POWER_SWITCH"
#endif

typedef struct TAG_S_PRODUCT_CONFIG {
  byte check_virgin;
  byte nodeID;
  char description[10];
  byte separator1;          //separators needed to keep strings from overlapping
  byte mqtt_server_ip[4];
  uint16_t mqtt_server_port;
  char mqtt_user[32];
  char mqtt_passwd[32];
  char router_ssid[32];
  char router_passwd[64];
  uint16_t timer_manual_time;
} S_PRODUCT_CONFIG, *S_PRODUCT_CONFIG_PTR;


extern S_PRODUCT_CONFIG_PTR gProductConfig_p;

#endif /* PRODUCT_WATERHEATER_MAINAPP_INC_PRODUCT_MAIN_H_ */
