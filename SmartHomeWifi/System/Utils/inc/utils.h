/*
 * utils.h
 *
 *  Created on: Oct 11, 2016
 *      Author: felix
 */

#ifndef SYSTEM_UTILS_INC_UTILS_H_
#define SYSTEM_UTILS_INC_UTILS_H_

#include <Arduino.h>  // for type definitions
#include <stddef.h>
#include <stdint.h>
#include <string.h>

class Utils {
public:
	/***************************************************************************//**
	* \fn atob
	* \brief This function converts HTML encoded to UTF8
	*
	* \param inputStr
	* \retval String in UTF8 format
	*******************************************************************************/
	static String htmlEncodedToUtf8(String inputStr);

	/***************************************************************************//**
	* \fn atob
	* \brief ascii value of hex digit -> real val
	* 		 the value of a hex char, 0=0,1=1,A=10,F=15
	* \param ch
	* \retval real val
	*******************************************************************************/
	static char atob (char ch);

	/***************************************************************************//**
	* \fn val2
	* \brief like atob for 2 chars representing nibbles of byte
	* \param None
	* \retval None
	*******************************************************************************/
	static char val2(String str);

};

#endif /* SYSTEM_UTILS_INC_UTILS_H_ */
