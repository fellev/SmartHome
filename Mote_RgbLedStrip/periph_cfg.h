#ifndef MOTE_RGB_LED_STRIP
#define MOTE_RGB_LED_STRIP

#define BOARD_MINI_WIRELESS_MOTEINO_COMPATIBLE  0
#define BOARD_MINI_WIRELESS_MOTEINO             1

/***************************************************************************************/
/******************** Board Model Selection ********************************************/
/* Currently supported board models:
 * 1. BOARD_MINI_WIRELESS_MOTEINO_COMPATIBLE
 * 2. BOARD_MINI_WIRELESS_MOTEINO
 */
#define DEVICE_BOARD            BOARD_MINI_WIRELESS_MOTEINO_COMPATIBLE
/***************************************************************************************/

#define BOARD_REV_0             0
#define BOARD_REV_1             1

/***************************************************************************************/
/******************** Board Revision Selection ********************************************/
/* Currently supported revisions:
 * 1. BOARD_REV_0
 * 2. BOARD_REV_1
 */
#define BOARD_REV               BOARD_REV_1
/***************************************************************************************/


#if (DEVICE_BOARD == BOARD_MINI_WIRELESS_MOTEINO_COMPATIBLE)
#define __S125FL127S__
#define SPI_CS                  5
#elif (DEVICE_BOARD == BOARD_MINI_WIRELESS_MOTEINO)
#define __W25X40CL__
#define SPI_CS                  8
#endif

#if defined(__W25X40CL__)
#define MANUFACTURER_ID 0xEF30
#elif defined (__S125FL127S__)
#define MANUFACTURER_ID 0x12018 //0x0120 
#else
#error "You must define flash memory"
#endif

#endif //MOTE_RGB_LED_STRIP
