/*
 * boot.h
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

#ifndef SYSTEM_BOOT_INC_BOOT_H_
#define SYSTEM_BOOT_INC_BOOT_H_


/***************************************************************************/
/**    INCLUDE FILES                                                      **/
/***************************************************************************/
#include "ESP8266WiFi.h"

/***************************************************************************/
/**    DEFINITIONS                                                        **/
/***************************************************************************/

/***************************************************************************/
/**    MACROS                                                             **/
/***************************************************************************/

/***************************************************************************/
/**    TYPES                                                              **/
/***************************************************************************/

/***************************************************************************/
/**    EXPORTED GLOBALS                                                   **/
/***************************************************************************/

/***************************************************************************/
/**    GLOBAL VARIABLES                                                   **/
/***************************************************************************/

/***************************************************************************/
/**    FUNCTION PROTOTYPES                                                **/
/***************************************************************************/

void f_systemBootSetup();
extern void (*f_systemBootWifiConnected_p)(void);
extern void (*f_SystemBootLaunchApServer_p)(String wifiInRange);

#endif /* SYSTEM_BOOT_INC_BOOT_H_ */