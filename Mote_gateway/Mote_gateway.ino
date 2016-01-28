// **********************************************************************************************************
// GarageMote garage door controller base receiver sketch that works with Moteinos equipped with HopeRF RFM69W/RFM69HW
// Can be adapted to use Moteinos using RFM12B
// This is the sketch for the base, not the controller itself, and meant as another example on how to use a
// Moteino as a gateway/base/receiver
// 2013-09-13 (C) felix@lowpowerlab.com, http://www.LowPowerLab.com
// **********************************************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this code/library but please abide with the CCSA license:
// http://creativecommons.org/licenses/by-sa/3.0/
// **********************************************************************************************************

#include <RFM69.h>
#include <SPI.h>
#include <SPIFlashA.h>
#include "periph_cfg.h"

//*****************************************************************************************************************************
// ADJUST THE SETTINGS BELOW DEPENDING ON YOUR HARDWARE/SITUATION!
//*****************************************************************************************************************************
#define NODEID        1
#define GARAGENODEID  99
#define NETWORKID     100
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    ")nXLceHCQkaU{-5@" //"sampleEncryptKey" has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW  //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define LED           9
#define SERIAL_BAUD   115200
#define ACK_TIME      30  // # of ms to wait for an ack
#define AC_ACK_TIME   255
#define RETRY_NUM     8

//*****************************************************************************************************************************

RFM69 radio;
SPIFlashA flash(SPI_CS, MANUFACTURER_ID); //EF40 for 16mbit windbond chip
byte readSerialLine(char* input, char endOfLineChar=10, byte maxLength=64, uint16_t timeout=50);

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //must include only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  if (flash.initialize())
    Serial.println("SPI Flash Init OK!");
  else
    Serial.println("SPI Flash Init FAIL! (is chip present?)");
}

byte ackCount=0;
byte inputLen=0;
byte nodeid;
char input[64];
void loop() {
  //process any serial input
  inputLen = readSerialLine(input);
      
  if (inputLen >= 8)
  {
    if (input[0]=='S' && input[1]=='H' && input[2]=='T' && input[3]=='O' && input[4]=='P' && input[5]=='N')
    {
      nodeid = val2(&input[6]);
      Serial.print("ID:");Serial.print(nodeid, DEC);
      Serial.print("OPN ... ");      
      if (radio.sendWithRetry(nodeid, "OPN", 3, RETRY_NUM, ACK_TIME))
        Serial.println("ok ... ");
      else Serial.println("nothing ... ");
    }
    else if (input[0]=='S' && input[1]=='H' && input[2]=='T' && input[3]=='C' && input[4]=='L' && input[5]=='S')
    {
      nodeid = val2(&input[6]);
      Serial.print("ID:");Serial.print(nodeid, DEC);
      Serial.print("CLS ... ");
      if (radio.sendWithRetry(nodeid, "CLS", 3, RETRY_NUM, ACK_TIME))
        Serial.println("ok ... ");
      else Serial.println("nothing ... ");
    }
    else if (input[0]=='S' && input[1]=='H' && input[2]=='T' && input[3]=='S' && input[4]=='T' && input[5]=='O')
    {
      nodeid = val2(&input[6]);
      Serial.print("ID:");Serial.print(nodeid, DEC);
      Serial.print("STO ... ");
      if (radio.sendWithRetry(nodeid, "STO", 3, RETRY_NUM, ACK_TIME))
        Serial.println("ok ... ");
      else Serial.println("nothing ... ");
    }
    else if (input[0]=='S' && input[1]=='H' && input[2]=='T' && input[3]=='S' && input[4]=='T' && input[5]=='S')
    {
      nodeid = val2(&input[6]);
      Serial.print("ID:");Serial.print(nodeid, DEC);
      Serial.print("STS ... ");
      if (radio.sendWithRetry(nodeid, "STS", 3, RETRY_NUM, ACK_TIME))
        Serial.println("ok ... ");
      else Serial.println("nothing ... ");
    }
    else if (input[0]=='R' && input[1]=='G' && input[2]=='B' && input[5]=='C' && input[6]=='L' && input[7]=='R')
    {
      nodeid = val2(&input[3]);
      Serial.print("ID:");Serial.print(nodeid, DEC);
      Serial.print("RGB CLR ... ");
      if (radio.sendWithRetry(nodeid, &input[5], 6, RETRY_NUM, ACK_TIME))
        Serial.println("ok ... ");
      else Serial.println("nothing ... ");      
    }
    else if (input[0]=='A' && input[1]=='C' && input[4]=='C' && input[5]=='F' && input[6]=='G')
    {
      int result;
      nodeid = val2(&input[2]);
      result = radio.sendWithRetry(nodeid, &input[4], 5, RETRY_NUM, AC_ACK_TIME);
      Serial.print("ID:");Serial.print(nodeid, DEC);
      Serial.print(" AC CFG ... ");
      if (result)
        Serial.println("ok ... ");
      else Serial.println("nothing ... ");      
    }

    //if (input == 'i')
    //{
    //  Serial.print("DeviceID: ");
    //  word jedecid = flash.readDeviceId();
    //  Serial.println(jedecid, HEX);
    //}
  }

  if (radio.receiveDone())
  {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    if (radio.ACKRequested())
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print("[ACK-sent]");
    }
    Serial.println();
    Blink(LED,3);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

// reads a line feed (\n) terminated line from the serial stream
// returns # of bytes read, up to 255
// timeout in ms, will timeout and return after so long
byte readSerialLine(char* input, char endOfLineChar, byte maxLength, uint16_t timeout)
{
  byte inputLen = 0;
  Serial.setTimeout(timeout);
  inputLen = Serial.readBytesUntil(endOfLineChar, input, maxLength);
  input[inputLen]=0;//null-terminate it
  Serial.setTimeout(0);
  //Serial.println();
  return inputLen;
}

/***************************************************************************//**
* \fn atob
* \brief ascii value of hex digit -> real val
* 		 the value of a hex char, 0=0,1=1,A=10,F=15
* \param ch
* \retval real val
*******************************************************************************/
byte atob (byte ch)
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
byte val2(const char *str)
{
	byte tmp;
	tmp = atob(str[0]);
	tmp <<= 4;
	tmp |= atob(str[1]);
	return tmp;

}
