/*
 * product_main.h
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

#ifndef PRODUCT_PWR_SW_MAINAPP_INC_PRODUCT_MAIN_H_
#define PRODUCT_PWR_SW_MAINAPP_INC_PRODUCT_MAIN_H_

#include <Arduino.h>

#if !defined(PRODUCT_NAME)
#define PRODUCT_NAME "POWER_SWITCH"
#endif

#define D_PROD_MAX_PARED_SWITCHES			4

typedef struct TAG_SW_ID {
  byte sw_id;
  byte controller_id;
}S_SW_ID, *S_SW_ID_PTR;

typedef struct TAG_S_SWITCH_CONFIG {
  byte led_gpio;
  byte relay_gpio;
  int timer_id_pwr;
  int timer_id_status;
  byte nc; /* 1 - Normally closed, 0 - Normally Open */
  S_SW_ID pared_sw[D_PROD_MAX_PARED_SWITCHES];

}S_SWITCH_CONFIG, *S_SWITCH_CONFIG_PTR;

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

#endif /* PRODUCT_PWR_SW_MAINAPP_INC_PRODUCT_MAIN_H_ */
