/*
 * gpio.h
 *
 *  Created on: Oct 15, 2016
 *      Author: felix
 */

#ifndef SYSTEM_ESP12E_GPIO_ESP12E_GPIO_H_
#define SYSTEM_ESP12E_GPIO_ESP12E_GPIO_H_


/***************************************************************************/
/**    INCLUDE FILES                                                      **/
/***************************************************************************/
//extern "C" {
//#include "gpio.h"
//}

/***************************************************************************/
/**    DEFINITIONS                                                        **/
/***************************************************************************/
#define D_HW_CONFIG_LED_ESP8266         2 //GPIO2

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

/*
template<uint8_t PIN>
class GpioPin {
public:
    static void SetMode(uint32_t mode) {
    	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO1); //use pin as GPIO1 instead of UART TXD
    	gpio_output_set(0, 0, 1 << pin, 0); // enable pin as output
    }

    inline static void SetHigh() {
        PORT->BSRR = 1 << PIN;
    }

    inline static void SetLow() {
        PORT->BRR = 1 << PIN;
    }

    inline static void Toggle() {
        PORT->ODR ^= 1 << PIN;  // Probably not interrupt safe.
    }

    inline static int GetValue() {
        return ((PORT->IDR & 1 << PIN) ? 1 : 0);
    }

    inline static int GetOutputValue() {
        return ((PORT->ODR & 1 << PIN) ? 1 : 0);
    }

    inline static void SetPullup() {
        PORT->PUPDR &= ~(0x3 << (PIN * 2));
        PORT->PUPDR |= 0x1 << (PIN * 2);
    }

    inline static void SetPulldown() {
        PORT->PUPDR &= ~(0x3 << (PIN * 2));
        PORT->PUPDR |= 0x2 << (PIN * 2);
    }

    inline static void SetNoPull() {
        PORT->PUPDR &= ~(0x3 << (PIN * 2));
    }
private:
    enum { AFR_INDEX = PIN >= 8 };
    GpioPin();
    GpioPin(const GpioPin&);
    GpioPin& operator=(const GpioPin&);
};
*/


#endif /* SYSTEM_ESP12E_GPIO_ESP12E_GPIO_H_ */
