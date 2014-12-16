// *************************************************************************************************************
//                                          Mote Switch sample sketch
// *************************************************************************************************************
// Author: Felix Laevsky
// Original author: Felix Rusu (2014), felix@lowpowerlab.com http://lowpowerlab.com/
// *************************************************************************************************************
// License
// *************************************************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 2 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE.  See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program; if not, write 
// to the Free Software Foundation, Inc.,                
// 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//                                                        
// Licence can be viewed at                               
// http://www.fsf.org/licenses/gpl.txt                    
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// *************************************************************************************************************
// This sketch assumes the Moteino inside the SwitchMote has been configured using the one time SwitchMoteConfig sketch
// before it has been installed. See this link for details: http://github.com/lowpowerlab/SwitchMote
// *************************************************************************************************************
// SwitchMote is a highly integrated wireless AC switch controller based on Moteino, the wirelessly programmable Arduino clone
// http://lowpowerlab.com/switchmote
// http://moteino.com
// http://github.com/lowpowerlab
// Be sure to check back for code updates and patches
// *************************************************************************************************************
// This sketch will provide the essential features of SwitchMote
//   - wireless programming
//   - accept ON/OFF commands from a Moteino gateway
//   - control the optional solid state relay (SSR) to turn a small AC load ON/OFF (up to 100W)
//   - control 2 LEDs and 2 switch buttons and 1 Sync button
//   - SYNC feature allows out of box synchronization with other SwitchMotes
//     ie - when a button is pressed on this unit it can simulate the press of another button on a remote SwitchMote
//     so you can use a SwitchMote button to control a light/set of lights from a remote location
//     this sketch allows up to 5 SYNCs but could be extended
// This sketch may be extended to include integration with other LowPowerLab automation products, for instance to
//    control the GarageMote from a button on the SwitchMote, etc.
// *************************************************************************************************************

#include <EEPROMex.h>      //get it here: http://playground.arduino.cc/Code/EEPROMex
#include <RFM69.h>         //get it here: http://github.com/lowpowerlab/rfm69
#include <SPIFlash.h>      //get it here: http://github.com/lowpowerlab/spiflash
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming
#include <SPI.h>           //comes with Arduino

#define CHECK_VIRGIN_VALUE  0x55
#define IS_RFM69HW          //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define GATEWAYID           1  //assumed 1 in general
#define NETWORKID           100
#define ENCRYPTKEY          "sampleEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
#define FREQUENCY           RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)

#define LED_BTN1            5
#define LED_BTN2            6
#define LED_ONBOARD         9   //pin connected to onboard LED
#define LED_PULSE_PERIOD    5000   //5s seems good value for pulsing/blinking (not too fast/slow)
#define LED_SYNC_BLINK_DC   80  //Led blink duty cycle during sync mode


#define REALY_CNT           2
#define RELAY1              17
#define RELAY2              18

#define BTNCOUNT            2  //1 or 3 (2 also possible) Sync button is not in count
#define BTN_SYNC            16  //digital pin of sync button
#define BTN2                15  //digital pin of button no 1
#define BTN1                14  //digital pin of bottom no 2
//#define BTNINDEX_SSR        1  //index in btn[] array which is associated with the SolidStateRelay (SSR)

#define BUTTON_BOUNCE_MS  300  //timespan before another button change can occur
#define SYNC_ENTER       3000  //time required to hold a button before SwitchMote enters [SYNC mode]
#define SYNC_TIME        2000  //max time spent in SYNC mode before returning to normal operation (you got this much time to SYNC 2 SMs, increase if need more time to walk)
#define SYNC_MAX_COUNT     10  //max number of SYNC entries (increase for more interactions)
#define SYNC_EEPROM_ADDR   64  //SYNC_TO and SYNC_INFO data starts at this EEPROM address
#define ERASE_HOLD       6000  //time required to hold a button before SYNC data is erased

//in SYNC_INFO we're storing 4 pieces of information in each byte:
#define SYNC_DIGIT_THISBTN  0 //first digit is the button pressed on this unit which will trigger an action on another unit
#define SYNC_DIGIT_THISMODE 1 //second digit indicates the mode of this unit is in when triggering
#define SYNC_DIGIT_SYNCBTN  2 //third digit indicates the button that should be requested to be "pressed" on the target
#define SYNC_DIGIT_SYNCMODE 3 //fourth digit indicates the mode that should be requested on the target

#define SERIAL_EN             //comment this out when deploying to an installed SM to save a few KB of sketch size
#define SERIAL_BAUD    115200
#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

#define LED_PERIOD_ERROR   50
#define LED_PERIOD_OK     200
#define ON                  1
#define OFF                 0
#define PRESSED             0
#define RELEASED            1

struct configuration {
  byte check_virgin;
  byte nodeID;
  byte networkID;
  char description[10];
  byte separator1;
} CONFIG;

void action(byte whichButtonIndex, byte whatMode, boolean notifyGateway=true); //compiler wants this prototype here because of the optional parameter
/////////////////////////////////////////////////////////////////////////////
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0xEF30 for windbond 4mbit flash (Moteino OEM)
/////////////////////////////////////////////////////////////////////////////
SPIFlash flash(8, 0xEF30);
//SYNC data is stored in 2 arrays:
byte SYNC_TO[SYNC_MAX_COUNT];  // stores the address of the remote Switch(s) that this Switch has to notify/send request to
int SYNC_INFO[SYNC_MAX_COUNT]; // stores the buttons and modes of this and the remote SM as last 4 digits:
                               //   - this SM button # (0,1) = least significant digit (SYNC_DIGIT_BTN=0)
                               //   - this button mode (0,1) = second digit ((SYNC_DIGIT_THISMODE=1)
                               //   - remote SM button # (0,1) = 3rd digit from right (SYNC_DIGIT_SYNCBTN=2)
                               //   - remote SM mode (0,1) = most significant digit (SYNC_DIGIT_SYNCMODE=3)
                               // the 4 pieces of information require an int (a byte only has up to 3 digits)
RFM69 radio;
long syncStart=0;
long now=0;
byte btnIndex=0; // as the sketch loops this index will loop through the available physical buttons
byte mode[] = {OFF,OFF}; //could use single bytes for efficiency but keeping it separate for clarity
byte btn[] = {BTN1, BTN2};
byte btnIndexRelay[] = {RELAY1, RELAY2};
byte btnLastState[]={RELEASED,RELEASED};
unsigned long btnLastPress[]={0,0};
byte btnLED[] = {LED_BTN1, LED_BTN2};
char * buff="justAnEmptyString";
int ledPulseValue=0;
boolean ledPulseDirection=false; //false=down, true=up
unsigned long ledPulseTimestamp=0;

void setup(void)
{
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
  #endif
  EEPROM.readBlock(0, CONFIG);
  if (CONFIG.check_virgin!=CHECK_VIRGIN_VALUE) // virgin CONFIG, expected [0x55]
  {
    DEBUGln("No valid config found in EEPROM, use the CONFIG sketch to load valid setting");
    while(1);
  }  
  //if SYNC_INFO[0] == 255 it means it's virgin EEPROM memory, needs initialization (one time ever)
  if (EEPROM.read(SYNC_EEPROM_ADDR+SYNC_MAX_COUNT)==255) eraseSYNC();  
  EEPROM.readBlock<byte>(SYNC_EEPROM_ADDR, SYNC_TO, SYNC_MAX_COUNT);
  EEPROM.readBlock<byte>(SYNC_EEPROM_ADDR+SYNC_MAX_COUNT, (byte *)SYNC_INFO, SYNC_MAX_COUNT*2); //int=2bytes so need to cast to byte array

  radio.initialize(FREQUENCY,CONFIG.nodeID,CONFIG.networkID);
  radio.sleep();
  //radio.setPowerLevel(10);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);

  DEBUG("\r\nNODEID:");DEBUGln(CONFIG.nodeID);
  DEBUG("SYNC_INFO: ");
  for(byte i = 0; i<SYNC_MAX_COUNT; i++)
  {
    DEBUG('[');
    DEBUG(SYNC_TO[i]);
    DEBUG("]=");
    DEBUG(SYNC_INFO[i]);
    DEBUG(" ");
  }

  pinMode(LED_BTN1, OUTPUT);
  pinMode(LED_BTN2, OUTPUT);
  pinMode(LED_ONBOARD, OUTPUT);
  // by writing HIGH while in INPUT mode, the internal pullup is activated
  // the button will read 1 when RELEASED (because of the pullup)
  // the button will read 0 when PRESSED (because it's shorted to GND)
  pinMode(BTN1, INPUT);digitalWrite(BTN1, HIGH); //activate pullup
  pinMode(BTN2, INPUT);digitalWrite(BTN2, HIGH); //activate pullup
  pinMode(BTN_SYNC, INPUT);digitalWrite(BTN_SYNC, HIGH); //activate pullup
  pinMode(RELAY1, OUTPUT);pinMode(RELAY2, OUTPUT);
  blinkLED(LED_BTN1,LED_PERIOD_ERROR,LED_PERIOD_ERROR,3);
  blinkLED(LED_BTN2,LED_PERIOD_ERROR,LED_PERIOD_ERROR,3);
  delay(500);
  DEBUGln("\r\nListening for ON/OFF commands...\n");

  //initialize LEDs according to default modes
  action(btnIndex, mode[btnIndex], false);btnIndex++;
  action(btnIndex, mode[btnIndex], false);
}

byte btnState=RELEASED;
byte btnSyncState;
boolean isSyncMode=0;
boolean ignorePress=false;
void loop()
{
  //on each loop pass check the next button
  if (isSyncMode==false)
  {
    btnIndex++;
    if (btnIndex>BTNCOUNT-1) btnIndex=0;
  }
  btnState = digitalRead(btn[btnIndex]);
  btnSyncState = digitalRead(BTN_SYNC);
  now = millis();
  
  if (btnState != btnLastState[btnIndex] && now-btnLastPress[btnIndex] >= BUTTON_BOUNCE_MS) //button event happened
  {
    DEBUG("BTN_SYNC ");
    DEBUGln(btnSyncState == PRESSED ? "PRESSED" : "RELEASED");
    btnLastState[btnIndex] = btnState;
    btnLastPress[btnIndex] = now;
    if (btnState == PRESSED && !isSyncMode)
    {
      ignorePress=false;
      if (btnSyncState == RELEASED) action(btnIndex, mode[btnIndex]==ON ? OFF : ON);
      checkSYNC();	

    }
    else if (btnState == RELEASED && btnSyncState == PRESSED) //if normal button press, do the SSR/LED action and notify sync-ed SwitchMotes
    {
//      ignorePress=false;
      action(btnIndex, mode[btnIndex]==ON ? OFF : ON, false);
//      checkSYNC();
    }
  }

  //enter SYNC mode when a button pressed for more than SYNC_ENTER ms
  if (isSyncMode==false && btnSyncState==PRESSED && btnState == PRESSED && now-btnLastPress[btnIndex] >= SYNC_ENTER && !ignorePress)
  {
    // first broadcast SYNC token to sync with another SwitchMote that is in SYNC mode
    // "SYNC?" means "is there anyone wanting to Synchronize with me?"
    // response "SYNCx:0" (meaning "OK, SYNC with me and turn my button x OFF")
    // response "SYNCx:1" (meaning "OK, SYNC with me and turn my button x ON")
    // no response means no other SwMote in range is in SYNC mode, so enter SYNC and
    //    listen for another SwMote to broadcast its SYNC token
    if (radio.sendWithRetry(RF69_BROADCAST_ADDR,"SYNC?",5))
    {
      DEBUG("GOT SYNC? REPLY FROM [");
      DEBUG(radio.SENDERID);
      DEBUG(":");DEBUG(radio.DATALEN);DEBUG("]:[");
      for (byte i = 0; i < radio.DATALEN; i++)
        DEBUG((char)radio.DATA[i]);
      DEBUGln(']');

      //ACK received, check payload
      if (radio.DATALEN==7 && radio.DATA[0]=='S' && radio.DATA[1]=='Y' && radio.DATA[2]=='N' && radio.DATA[3]=='C' && radio.DATA[5]==':'
          && radio.DATA[4]>='0' && radio.DATA[4]<='1' && (radio.DATA[6]=='0' || radio.DATA[6]=='1'))
      {
        if (addSYNC(radio.SENDERID, radio.DATA[4]-'0', radio.DATA[6]-'0'))
          blinkLED(btnLED[btnIndex],LED_PERIOD_OK,LED_PERIOD_OK,3);
        else 
          blinkLED(btnLED[btnIndex],LED_PERIOD_ERROR,LED_PERIOD_ERROR,3);

        action(btnIndex, mode[btnIndex]);
        return; //exit SYNC
      }
      else
      {
        DEBUG("SYNC ACK mismatch: [");
        for (byte i = 0; i < radio.DATALEN; i++)
          DEBUG((char)radio.DATA[i]);
        DEBUGln(']');
      }
    }
    else { DEBUGln("NO SYNC REPLY ..");}

    isSyncMode = true;
    DEBUGln("SYNC MODE ON");
    displaySYNC();
    syncStart = now;
  }

  //if button held for more than ERASE_TRIGGER, erase SYNC table
  if (isSyncMode==true && btnSyncState==PRESSED && btnState == PRESSED && now-btnLastPress[btnIndex] >= ERASE_HOLD && !ignorePress)
  {
    DEBUG("ERASING SYNC TABLE ... ");
    eraseSYNC();
    isSyncMode = false;
    ignorePress = true;
    DEBUGln("... DONE");
    blinkLED(btnLED[btnIndex],LED_PERIOD_ERROR,LED_PERIOD_ERROR,3);
    action(btnIndex, mode[btnIndex], false);
  }


  if (isSyncMode)
  {
    syncBlink(btnLED[btnIndex], mode[btnIndex]==ON ? LED_SYNC_BLINK_DC : 100 - LED_SYNC_BLINK_DC);
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
      
      analogWrite(LED_ONBOARD, ledPulseValue);
      ledPulseTimestamp = millis();
    }
    //SYNC exit condition
    if (btnSyncState == RELEASED && now-syncStart >= SYNC_TIME)
    {
      isSyncMode = false;
      DEBUGln("SYNC MODE OFF");
      action(btnIndex, mode[btnIndex], false);
    }
  }
  else
  {
    if (millis()-(ledPulseTimestamp) > LED_PULSE_PERIOD/20)
    {
      ledPulseDirection = !ledPulseDirection;
      digitalWrite(LED_ONBOARD, ledPulseDirection ? HIGH : LOW);
      ledPulseTimestamp = millis();
    }
  }

  //check if any packet received
  if (radio.receiveDone())
  {
    DEBUGln();
    DEBUG("[");DEBUG(radio.SENDERID);DEBUG("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      DEBUG((char)radio.DATA[i]);
    DEBUG(" [RX_RSSI:");DEBUG(radio.RSSI);DEBUG("]");

    // wireless programming token check
    // DO NOT REMOVE, or SwitchMote will not be wirelessly programmable any more!
    CheckForWirelessHEX(radio, flash, true, LED_ONBOARD);

    //respond to SYNC request
    if (isSyncMode && radio.DATALEN == 5
        && radio.DATA[0]=='S' && radio.DATA[1]=='Y' && radio.DATA[2]=='N' && radio.DATA[3] == 'C' && radio.DATA[4]=='?')
    {
      sprintf(buff,"SYNC%d:%d",btnIndex, mode[btnIndex]); //respond to SYNC request with this Switch's button and mode information
      radio.sendACK(buff, strlen(buff));
      DEBUG(" - SYNC ACK sent : ");
      DEBUGln(buff);
      isSyncMode = false;
      action(btnIndex, mode[btnIndex], false);
      return; //continue loop
    }
    
#if 0
    //listen for relay requests: SSR0:0, SSR0:1. SSR1:0 or SSR1:1 commands
    if (radio.DATALEN == 6
        && radio.DATA[0]=='S' && radio.DATA[1]=='S' && radio.DATA[2]=='R' && (radio.DATA[3]=='0' || radio.DATA[3]=='1') && radio.DATA[4] == ':'
        && (radio.DATA[5]=='0' || radio.DATA[5]=='1'))
    {
      mode[radio.DATA[3]-'0'] = radio.DATA[5]=='1'?ON:OFF;
      if (radio.ACK_REQUESTED) radio.sendACK(); //send ACK sooner when a ON/OFF + ACK is requested
      action(radio.DATA[3]-'0', mode[radio.DATA[3]-'0'], radio.SENDERID!=GATEWAYID);
      //at this point we could call checkSYNC() again to notify the SYNC subscribed SwitchMotes but that could cause a chain reaction
      //so for simplicity sake we are only sending SYNC ON/OFF requests when the physical button is used
      //alternatively a special/aumented ON/OFF packet command could be used to indicate checkSYNC() should be called
    }
#endif
    
    //listen for BTNx:y commands where x={0,1}, y={0,1}
    if (radio.DATALEN == 6
        && radio.DATA[0]=='B' && radio.DATA[1]=='T' && radio.DATA[2]=='N' && radio.DATA[4] == ':'
        && (radio.DATA[3]>='0' && radio.DATA[3]<='1') && (radio.DATA[5]=='0' || radio.DATA[5]=='1'))
    {
      mode[radio.DATA[3]-'0'] = (radio.DATA[5]=='1'?ON:OFF);
      if (radio.ACKRequested()) radio.sendACK(); //send ACK sooner when a ON/OFF + ACK is requested
      action(radio.DATA[3]-'0', mode[radio.DATA[3]-'0'], radio.SENDERID!=GATEWAYID);
      //at this point we could call checkSYNC() again to notify the SYNC subscribed SwitchMotes but that could cause a chain reaction
      //so for simplicity sake we are only sending SYNC ON/OFF requests when the physical button is used
      //alternatively a special/aumented ON/OFF packet command could be used to indicate checkSYNC() should be called
    }

    if (radio.ACKRequested()) //dont ACK broadcasted messages except in special circumstances (like SYNCing)
    {
      radio.sendACK();
      DEBUG(" - ACK sent");
      delay(5);
    }
  }
}

//sets the mode (ON/OFF) for the current button (btnIndex) and turns SSR ON if the btnIndex points to BTNSSR
void action(byte whichButtonIndex, byte whatMode, boolean notifyGateway)
{
  DEBUG("\r\nbtn[");
  DEBUG(whichButtonIndex);
  DEBUG("]:");
  DEBUG(btn[whichButtonIndex]);
  DEBUG(" - ");
  DEBUG(btn[whichButtonIndex]==BTN1?"BTN1":btn[whichButtonIndex]==BTN2?"BTN2":"UNKNOWN");
  DEBUG(whatMode==ON?"ON ":"OFF");
  mode[whichButtonIndex] = whatMode;
  digitalWrite(btnLED[whichButtonIndex], whatMode == ON ? HIGH : LOW);
  digitalWrite(btnIndexRelay[whichButtonIndex], whatMode == ON ? HIGH : LOW);
  if (notifyGateway)
  {
#if 0
    if (btnIndex==BTNINDEX_SSR)
      sprintf(buff, "SSR:%d",whatMode);
    else
#endif
    sprintf(buff, "BTN%d:%d", whichButtonIndex,whatMode);
    if (radio.sendWithRetry(GATEWAYID, buff, strlen(buff)))
      {DEBUGln("..OK");}
    else {DEBUGln("..NOK");}
  }
}

long blinkLastCycle=0;
boolean blinkState=0;
// dutyCycle should be between 0 to 100
void syncBlink(byte LED1, byte dutyCycle)
{
  if (blinkState && now-blinkLastCycle>=dutyCycle)
  {
    blinkLastCycle=now;
    blinkState=0;
    digitalWrite(LED1,blinkState);
  }
  else if (!blinkState && now-blinkLastCycle>=(100 - dutyCycle))
  {
    blinkLastCycle=now;
    blinkState=1;
    digitalWrite(LED1,blinkState);
  }
}

//adds a new entry in the SYNC data
boolean addSYNC(byte targetAddr, byte targetButton, byte targetMode)
{
  byte emptySlot=255;
  if (targetAddr==0) return false;

  //traverse all SYNC data and look for an empty slot, or matching slot that should be overwritten
  for (byte i=0; i < SYNC_MAX_COUNT; i++)
  {
    if (SYNC_TO[i]==0 && emptySlot==255) //save first empty slot
      emptySlot=i; //remember first empty slot anyway
    else if (SYNC_TO[i]==targetAddr &&   //save first slot that matches the same button with the same mode in this unit
                                         //but different target unit mode (cant have 2 opposing conditions so just override it)
           getDigit(SYNC_INFO[i],SYNC_DIGIT_SYNCBTN)==targetButton &&
           getDigit(SYNC_INFO[i],SYNC_DIGIT_THISMODE)==mode[btnIndex]) //getDigit(SYNC_INFO[i],SYNC_DIGIT_SYNCNODE)!=targetMode)
    {
      emptySlot=i; //remember matching non-empty slot
      break; //stop as soon as we found a match
    }
  }

  if (emptySlot==255) //means SYNC data is full, do nothing and return
  {
    DEBUGln("SYNC data full, aborting...");
    return false;
  }
  else
  {
    SYNC_TO[emptySlot] = targetAddr;
    SYNC_INFO[emptySlot] = targetMode*1000 + targetButton*100 + mode[btnIndex]*10 + btnIndex;
    DEBUG("Saving SYNC_TO[");
    DEBUG(emptySlot);
    DEBUG("]=");
    DEBUG(SYNC_TO[emptySlot]);
    DEBUG(" SYNC_INFO = ");
    DEBUG(SYNC_INFO[emptySlot]);    
    saveSYNC();
    DEBUGln(" .. Saved!");
    return true;
  }
}

//checks the SYNC table for any necessary requests to other SwitchMotes
void checkSYNC()
{
  for (byte i=0; i < SYNC_MAX_COUNT; i++)
  {
    if (SYNC_TO[i]!=0 && getDigit(SYNC_INFO[i],SYNC_DIGIT_THISBTN)==btnIndex && getDigit(SYNC_INFO[i],SYNC_DIGIT_THISMODE)==mode[btnIndex])
    {
      DEBUGln();
      DEBUG(" SYNC[");
      DEBUG(SYNC_TO[i]);
      DEBUG(":");
      DEBUG(getDigit(SYNC_INFO[i],SYNC_DIGIT_SYNCMODE));
      DEBUG("]:");
      sprintf(buff, "BTN%d:%d", getDigit(SYNC_INFO[i],SYNC_DIGIT_SYNCBTN), getDigit(SYNC_INFO[i],SYNC_DIGIT_SYNCMODE));
      if (radio.sendWithRetry(SYNC_TO[i], buff,6))
      {DEBUG("OK");}
      else {DEBUG("NOK");}
    }
  }
}

void eraseSYNC()
{
  for(byte i = 0; i<SYNC_MAX_COUNT; i++)
  {
    SYNC_TO[i]=0;
    SYNC_INFO[i]=0;
  }
  saveSYNC();
}

//saves SYNC_TO and SYNC_INFO arrays to EEPROM
void saveSYNC()
{
  EEPROM.writeBlock<byte>(SYNC_EEPROM_ADDR, SYNC_TO, SYNC_MAX_COUNT);
  EEPROM.writeBlock<byte>(SYNC_EEPROM_ADDR+SYNC_MAX_COUNT, (byte*)SYNC_INFO, SYNC_MAX_COUNT*2);
}

void displaySYNC()
{
  DEBUG("SYNC TABLE: ");
  for (byte i=0; i < SYNC_MAX_COUNT; i++)
  {
    DEBUG(SYNC_INFO[i]);
    if (i!=SYNC_MAX_COUNT-1) DEBUG(',');
  }
  DEBUGln();
}

//returns the Nth digit in an integer
byte getDigit(int n, byte pos){return (n/(pos==0?1:pos==1?10:pos==2?100:1000))%10;}

void blinkLED(byte LEDpin, byte periodON, byte periodOFF, byte repeats)
{
  while(repeats-->0)
  {
    digitalWrite(LEDpin, HIGH);
    delay(periodON);
    digitalWrite(LEDpin, LOW);
    delay(periodOFF);
  }
}
