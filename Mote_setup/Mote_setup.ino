// **********************************************************************************************************
// Author: Felix Laevsky
// Original author: felix@lowpowerlab.com, http://www.LowPowerLab.com
// **********************************************************************************************************
// **********************************************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this code/library but please abide with the CCSA license:
// http://creativecommons.org/licenses/by-sa/4.0/
// **********************************************************************************

#include <RFM69.h>         //get it here: http://github.com/lowpowerlab/rfm69
#include <SPIFlashA.h>      //get it here: http://github.com/lowpowerlab/spiflash
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming
#include <SPI.h>           //comes with Arduino IDE (www.arduino.cc)
#include <avr/wdt.h>       //watchdog library
//#include <EEPROMex.h>      //get it here: http://playground.arduino.cc/Code/EEPROMex
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "periph_cfg.h"

//*****************************************************************************************************************************
// ADJUST THE SETTINGS BELOW DEPENDING ON YOUR HARDWARE/SITUATION!
//*****************************************************************************************************************************
#define CHECK_VIRGIN_VALUE 0x55
#define GATEWAYID   1
#define NODEID      99
#define NETWORKID   100
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY       RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define ENCRYPTKEY     "sampleEncryptKey" //")nXLceHCQkaU{-5@" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30  // # of ms to wait for an ack
#define RETRY_NUM     0

#define HALLSENSOR1          A0
//#define HALLSENSOR1_EN       16
#define HALLSENSOR2          A1
//#define HALLSENSOR2_EN       17

#define LED_ONBOARD         9   //pin connected to onboard LED

#define RELAY_DIR_PIN         6  //direction relay - selecting the direction of the shutter (Open or Close)
#define RELAY_PWR_PIN         7  //power relay - connects the direction relay to the power line
#define RELAY_PULSE_MS      250  //just enough that the opener will pick it up

#define DOOR_MOVEMENT_TIME 20000 // this has to be at least as long as the max between [door opening time, door closing time]
                                 // my door opens and closes in about 12s
#define STATUS_CHANGE_MIN  1500  // this has to be at least as long as the delay 
                                 // between a opener button press and door movement start
                                 // most garage doors will start moving immediately (within half a second)
                                 
#define BTNCOUNT            2
#define BTNO                4  //digital pin of OPEN shutter button
#define BTNC                5  //digital pin of CLOSE shutter button

#define BTNO_INDEX          0
#define BTNC_INDEX          1

#define BUTTON_BOUNCE_MS  200  //timespan before another button change can occur
#define HOLD_TIME        1000
#define HOLD_MODE           1
#define NORMAL_MODE         0
#define PRESSED             0
#define RELEASED            1
#define PRESSED_HOLD        2

#define SHUTTER_STOP        0
#define SHUTTER_OPEN        1
#define SHUTTER_CLOSE       2
//*****************************************************************************************************************************
#define HALLSENSOR_OPENSIDE   0
#define HALLSENSOR_CLOSEDSIDE 1

#define STATUS_CLOSED        0
#define STATUS_CLOSING       1
#define STATUS_OPENING       2
#define STATUS_OPEN          3
#define STATUS_STOPPED_BY_USER 4
#define STATUS_UNKNOWN       5

#define LED                  9   //pin connected to onboard LED
#define LED_PULSE_PERIOD  5000   //5s seems good value for pulsing/blinking (not too fast/slow)
#define SERIAL_BAUD     115200
#define SERIAL_EN                //comment out if you don't want any serial output

#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

void setStatus(byte newSTATUS, boolean reportStatus=true);
byte STATUS;
unsigned long lastStatusTimestamp=0;
unsigned long ledPulseTimestamp=0;
byte lastRequesterNodeID=GATEWAYID;
int ledPulseValue=0;
boolean ledPulseDirection=false; //false=down, true=up
RFM69 radio;
long now=0;
byte btnIndex=0; // as the sketch loops this index will loop through the available physical buttons
//byte mode[] = {NORMAL_MODE,NORMAL_MODE};
byte btn[] = {BTNO, BTNC};
byte btnLastState[]={RELEASED,RELEASED};
//byte relay[] = {BTNO, BTNC};
unsigned long btnLastPress[]={0,0};
byte bMode = NORMAL_MODE;
struct configuration {
  byte check_virgin;
  byte nodeID;
  byte networkID;
  byte separator1;          //separators needed to keep strings from overlapping
  char description[10];
  byte separator2;
} CONFIG;
/////////////////////////////////////////////////////////////////////////////
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0xEF30 for windbond 4mbit flash (Moteino OEM)
/////////////////////////////////////////////////////////////////////////////
SPIFlashA flash(SPI_CS, MANUFACTURER_ID);

void displayMainMenu()
{
  Serial.println();
  Serial.println("-----------------------------------------------");
  Serial.println("   Moteino-RFM69 configuration setup utility");
  Serial.println("-----------------------------------------------");
  Serial.println( "Compiled: " __DATE__ ", " __TIME__ ", " __VERSION__);
  Serial.println("-----------------------------------------------");
  Serial.print(" i - set node ID (current: ");Serial.print(CONFIG.nodeID);Serial.println(")");
  Serial.print(" n - set network ID (current: ");Serial.print(CONFIG.networkID);Serial.println(")");
  Serial.print(" d - set description (current: ");Serial.print(CONFIG.description);Serial.println(")");
  Serial.println(" s - save CONFIG to EEPROM");
  Serial.println(" E - erase whole EEPROM - [0..1023]");
  Serial.println(" r - reboot");
  Serial.println(" ESC - return to main menu");
}

char menu = 0;
byte charsRead = 0;
boolean setup_done;
void handleMenuInput(char c)
{
  switch(menu)
  {
    case 0:
      switch(c)
      { 
        case 'i': Serial.print("\r\nEnter node ID (1-255 + <ENTER>): "); CONFIG.nodeID=0;menu=c; break;
        case 'n': Serial.print("\r\nEnter network ID (0-255 + <ENTER>): "); CONFIG.networkID=0; menu=c; break;
        case 'd': Serial.print("\r\nEnter description (10 chars max + <ENTER>): "); menu=c; break;
        case 's': Serial.print("\r\nCONFIG saved to EEPROM!"); EEPROM_writeAnything(0, CONFIG); break;//EEPROM.writeBlock(0, CONFIG); break;
        case 'E': Serial.print("\r\nErasing EEPROM ... "); menu=c; break;
        case 'r': Serial.print("\r\nRebooting"); resetUsingWatchdog(1); break;
        case  27:  Serial.print("\r\nExiting the menu...\r\n");menu=0;setup_done=true;break;
      }
      break;
      
    case 'i':
      if (c >= '0' && c <= '9')
      {
        if (CONFIG.nodeID * 10 + c - 48 <= 255)
        {
          CONFIG.nodeID = CONFIG.nodeID * 10 + c - 48;
          Serial.print(c);
        }
        else
        {
          Serial.print(" - Set to ");Serial.println(CONFIG.nodeID);
          menu=0;
        }
      }
      else if (c == 13 || c == 27)
      {
        Serial.print(" - Set to ");Serial.println(CONFIG.nodeID);
        displayMainMenu();
        menu=0;
      }
      break;

    case 'n':
      if (c >= '0' && c <= '9')
      {
        if (CONFIG.networkID * 10 + c - 48 <= 255)
        {
          CONFIG.networkID = CONFIG.networkID * 10 + c - 48;
          Serial.print(c);
        }
        else
        {
          Serial.print(" - Set to ");Serial.println(CONFIG.networkID);
          menu=0;
        }
      }
      if (c == 13 || c == 27)
      {
        Serial.print(" - Set to ");Serial.println(CONFIG.networkID);
        displayMainMenu();
        menu=0;
      }
      break;

    case 'd':
      if (c >= ' ' && c <= '~') //human readable chars (32 - 126)
      {
        if (++charsRead<=10)
        {
          CONFIG.description[charsRead-1] = c;
          CONFIG.description[charsRead] = 0;
          Serial.print(c);
        }
      }
      if (charsRead>=10 || c == 13 || c == 27)
      {
        //Serial.print(" - Set to [");Serial.print(CONFIG.description);Serial.println(']');
        Serial.println(" - DONE");
        displayMainMenu();menu=0;charsRead=0;
      }
      break;

    case 'E':
      for (int i=0;i<1024;i++) EEPROM.write(i,255); //eeprom_write_byte((unsigned char *) i, 255);
      Serial.println("DONE");
      //resetUsingWatchdog(1);
      menu=0;
      break;
  }
}


void setup(void)
{
  Serial.begin(SERIAL_BAUD);
  EEPROM_readAnything(0, CONFIG);
  if (CONFIG.check_virgin!=CHECK_VIRGIN_VALUE) // virgin CONFIG, expected [0x55]
  {
    Serial.println("No valid config found in EEPROM, writing defaults");
    CONFIG.separator1=CONFIG.separator2=0;
    CONFIG.check_virgin = CHECK_VIRGIN_VALUE;
    CONFIG.description[0]=0;
    CONFIG.nodeID=0;
    CONFIG.networkID=NETWORKID;
  }
  
  radio.initialize(FREQUENCY,CONFIG.nodeID,CONFIG.networkID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);

  char buff[50];
  sprintf(buff, "Mote Shutter : %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGln(buff);

  wdt_enable (WDTO_1S);
}

unsigned long doorPulseCount = 0;
char input;

void loop()
{  
  if (Serial.available())
    input = Serial.read();
    
  wdt_disable();
  displayMainMenu();
  setup_done = false;
  while(setup_done!=true)
  {
    if(Serial.available())
      handleMenuInput(Serial.read());
    
    if (millis()-(ledPulseTimestamp) > LED_PULSE_PERIOD/256)
    {
      ledPulseValue = ledPulseDirection ? ledPulseValue + LED_PULSE_PERIOD/256 : ledPulseValue - LED_PULSE_PERIOD/256;

      if (ledPulseDirection && ledPulseValue > 255)
      {
        ledPulseDirection=false;
        ledPulseValue = 255;
      }
      else if (!ledPulseDirection && ledPulseValue < 0)
      {
        ledPulseDirection=true;
        ledPulseValue = 0;
      }
      
      analogWrite(LED, ledPulseValue);
      ledPulseTimestamp = millis();
    }
    
        
    // wireless programming token check
    // DO NOT REMOVE, or GarageMote will not be wirelessly programmable any more!
    CheckForWirelessHEX(radio, flash, true, LED_ONBOARD);
  }
  wdt_enable (WDTO_1S);
  
  wdt_reset();
  
  
}

void Blink(byte PIN, byte DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
