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
	static String htmlEncodedToUtf8(String inputStr) {
		int inCnt = 0, outCnt = 0;
		String out = inputStr;

		for ( inCnt = 0 ; inCnt < inputStr.length() ; inCnt++, outCnt++) {

			if (inputStr[inCnt] == '%')
			{
				inCnt++;
				out.setCharAt(outCnt, val2(inputStr.substring(inCnt)));
				inCnt++;
			}
			else {
				out[outCnt] = inputStr[inCnt];
			}
		}
		return out.substring(0, outCnt);
	};

	/***************************************************************************//**
	* \fn atob
	* \brief ascii value of hex digit -> real val
	* 		 the value of a hex char, 0=0,1=1,A=10,F=15
	* \param ch
	* \retval real val
	*******************************************************************************/
	static char atob (char ch)
	{
		if (ch >= '0' && ch <= '9')
	    	return ch - '0';
		else{
			switch(ch)	{
	  		case 'A': return 10;
		   	case 'B': return 11;
			case 'C': return 12;
			case 'D': return 13;
			case 'E': return 14;
			case 'F': return 15;
			case 'a': return 10;
		   	case 'b': return 11;
			case 'c': return 12;
			case 'd': return 13;
			case 'e': return 14;
	  		case 'f': return 15;
	  }

		}
		return 0xFF; //error - char recieved not in range
	}

	/***************************************************************************//**
	* \fn val2
	* \brief like atob for 2 chars representing nibbles of byte
	* \param None
	* \retval None
	*******************************************************************************/
	static char val2(String str)
	{
		char tmp;
		tmp = atob(str[0]);
		tmp <<= 4;
		tmp |= atob(str[1]);
		return tmp;
	}

};

#endif /* SYSTEM_UTILS_INC_UTILS_H_ */
