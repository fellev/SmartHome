/*
 * wh_main.c
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

#include "wh_main.h"
#include "boot.h"

S_PRODUCT_CONFIG gProductConfig;

void setup() {

	gProductConfig_p = &gProductConfig;
	setup_boot();

}



void loop() {
  // put your main code here, to run repeatedly:

}
