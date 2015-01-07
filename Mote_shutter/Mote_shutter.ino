// **********************************************************************************************************
// Author: Felix Laevsky
// Original author: felix@lowpowerlab.com, http://www.LowPowerLab.com
// GarageMote garage door controller sketch that works with Moteinos equipped with HopeRF RFM69W/RFM69HW
// Can be adapted to use Moteinos using RFM12B
// 2014-07-14 (C) felix@lowpowerlab.com, http://www.LowPowerLab.com
// **********************************************************************************************************
// It uses 2 hall effect sensors (and magnets mounted on the garage belt/chain) to detect the position of the
// door, and a small signal relay to be able to toggle the garage opener.
// Implementation details are posted at the LowPowerLab blog
// Door status is reported via RFM69 to a base Moteino, and visually on the onboard Moteino LED:
//    - solid ON - door is in open position
//    - solid OFF - door is in closed position
//    - blinking - door is not in either open/close position
//    - pulsing - door is in motion
// **********************************************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this code/library but please abide with the CCSA license:
// http://creativecommons.org/licenses/by-sa/4.0/
// **********************************************************************************

#include <RFM69.h>         //get it here: http://github.com/lowpowerlab/rfm69
#include <SPIFlash.h>      //get it here: http://github.com/lowpowerlab/spiflash
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming
#include <SPI.h>           //comes with Arduino IDE (www.arduino.cc)
#include <avr/wdt.h>       //watchdog library
#include <EEPROMex.h>      //get it here: http://playground.arduino.cc/Code/EEPROMex

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
#define ENCRYPTKEY      "sampleEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30  // # of ms to wait for an ack
#define RETRY_NUM     0

#define HALLSENSOR1          A0
//#define HALLSENSOR1_EN       16
#define HALLSENSOR2          A1
//#define HALLSENSOR2_EN       17

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
#define STATUS_UNKNOWN       4 

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
SPIFlash flash(8, 0xEF30);

void shutter_motor_control(int cmd)
{
  switch(cmd)
  {
    case SHUTTER_STOP:
      digitalWrite(RELAY_PWR_PIN, LOW);
      digitalWrite(RELAY_DIR_PIN, LOW);
      break;
    case SHUTTER_OPEN:
      digitalWrite(RELAY_DIR_PIN, LOW);
      digitalWrite(RELAY_PWR_PIN, HIGH);
      break;
    case SHUTTER_CLOSE:
      digitalWrite(RELAY_DIR_PIN, HIGH);
      digitalWrite(RELAY_PWR_PIN, HIGH);
      break;
    default: 
      digitalWrite(RELAY_PWR_PIN, LOW);
      digitalWrite(RELAY_DIR_PIN, LOW);
      break;
  }
}

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
        case 's': Serial.print("\r\nCONFIG saved to EEPROM!"); EEPROM.writeBlock(0, CONFIG); break;
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
  EEPROM.readBlock(0, CONFIG);
  if (CONFIG.check_virgin!=CHECK_VIRGIN_VALUE) // virgin CONFIG, expected [0x55]
  {
    Serial.println("No valid config found in EEPROM, writing defaults");
    CONFIG.separator1=CONFIG.separator2=0;
    CONFIG.check_virgin = CHECK_VIRGIN_VALUE;
    CONFIG.description[0]=0;
    CONFIG.nodeID=0;
    CONFIG.networkID=NETWORKID;
  }
  
  pinMode(RELAY_DIR_PIN, OUTPUT);digitalWrite(RELAY_DIR_PIN, LOW);
  pinMode(RELAY_PWR_PIN, OUTPUT);digitalWrite(RELAY_PWR_PIN, LOW);
  pinMode(HALLSENSOR1, INPUT);digitalWrite(HALLSENSOR1, HIGH); //activate pullup
  pinMode(HALLSENSOR2, INPUT);digitalWrite(HALLSENSOR2, HIGH); //activate pullup
  pinMode(HALLSENSOR1_EN, OUTPUT);
  pinMode(HALLSENSOR2_EN, OUTPUT);
  pinMode(BTNO, INPUT);digitalWrite(BTNO, HIGH); //activate pullup
  pinMode(BTNC, INPUT);digitalWrite(BTNC, HIGH); //activate pullup

  pinMode(LED, OUTPUT);
  
  radio.initialize(FREQUENCY,CONFIG.nodeID,CONFIG.networkID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);

  char buff[50];
  sprintf(buff, "GarageMote : %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGln(buff);

  if (hallSensorRead(HALLSENSOR_OPENSIDE)==true)
    setStatus(STATUS_OPEN);
  if (hallSensorRead(HALLSENSOR_CLOSEDSIDE)==true)
    setStatus(STATUS_CLOSED);
  else setStatus(STATUS_UNKNOWN);
  
  wdt_enable (WDTO_1S);
}

unsigned long doorPulseCount = 0;
char input;
byte btnState=RELEASED;

void loop()
{  
  if (Serial.available())
    input = Serial.read();
    
  if (input=='r')
  {
    DEBUGln("Relay test...");
//    pulseRelay();
    input = 0;
  }
  else if (input=='s')
  {
    input = 0;
    MCUSR = 0;
    wdt_disable();
    displayMainMenu();
    setup_done = false;
    while(setup_done!=true)
    {
      if(Serial.available())
        handleMenuInput(Serial.read());
    }
    wdt_enable (WDTO_1S);
  }
  
  wdt_reset();
  
  btnIndex++;
  if (btnIndex>BTNCOUNT-1) btnIndex=0;  
  
  btnState = digitalRead(btn[btnIndex]);
  now = millis();
  
  if (btnState != btnLastState[btnIndex] && now-btnLastPress[btnIndex] >= BUTTON_BOUNCE_MS) //button event happened
  {
    DEBUG((btnIndex == BTNO_INDEX) ? "OPEN" : "CLOSE");DEBUG(" Button");DEBUGln((btnState == PRESSED) ? " pressed" : " released");
    btnLastState[btnIndex] = btnState;
    if (btnState == PRESSED)
    {
      btnLastPress[btnIndex] = now;
      bMode = NORMAL_MODE;
      if (btnIndex == BTNO_INDEX && (STATUS == STATUS_CLOSED || STATUS == STATUS_CLOSING || STATUS == STATUS_UNKNOWN))
      {
        // Manual open
        setStatus(STATUS_OPENING);
      }
      else if (btnIndex == BTNC_INDEX && (STATUS == STATUS_OPEN || STATUS == STATUS_OPENING || STATUS == STATUS_UNKNOWN))
      {
        // Manual close
        setStatus(STATUS_CLOSING);
      }
    }

//    if (btnState == RELEASED)
//    {
//      ignorePress=false;
//    }
  }
  
  //enter SYNC mode when a button pressed for more than SYNC_ENTER ms
  if (now-(lastStatusTimestamp) > STATUS_CHANGE_MIN && btnState == PRESSED && now-btnLastPress[btnIndex] >= HOLD_TIME)
  {
    //Keep closing or opening the shutter ofter button is released
    bMode = HOLD_MODE;
    if (!(STATUS == STATUS_OPENING || STATUS == STATUS_CLOSING))
      setStatus((btnIndex == BTNO_INDEX) ? STATUS_OPENING : STATUS_CLOSING);
  }
 

  if ((STATUS == STATUS_OPENING || STATUS == STATUS_CLOSING) && btnLastState[BTNO_INDEX] == RELEASED && btnLastState[BTNC_INDEX] == RELEASED && bMode == NORMAL_MODE)
  {
    // Stop manual open/close
    setStatus(STATUS_UNKNOWN);
  }

    
  // UNKNOWN => OPEN/CLOSED
  if (STATUS == STATUS_UNKNOWN && millis()-(lastStatusTimestamp)>STATUS_CHANGE_MIN)
  {
    if (hallSensorRead(HALLSENSOR_OPENSIDE)==true)
      setStatus(STATUS_OPEN);
    if (hallSensorRead(HALLSENSOR_CLOSEDSIDE)==true)
      setStatus(STATUS_CLOSED);
  }


  // OPEN => CLOSING
  if (STATUS == STATUS_OPEN && millis()-(lastStatusTimestamp)>STATUS_CHANGE_MIN)
  {
    if (hallSensorRead(HALLSENSOR_OPENSIDE)==false)
      setStatus(STATUS_UNKNOWN);
  }

  // CLOSED => OPENING  
  if (STATUS == STATUS_CLOSED && millis()-(lastStatusTimestamp)>STATUS_CHANGE_MIN)
  {
    if (hallSensorRead(HALLSENSOR_CLOSEDSIDE)==false)
      setStatus(STATUS_UNKNOWN);
  }

  // OPENING/CLOSING => OPEN (when door returns to open due to obstacle or toggle action)
  //                 => CLOSED (when door closes normally from OPEN)
  //                 => UNKNOWN (when more time passes than normally would for a door up/down movement)
  if ((STATUS == STATUS_OPENING || STATUS == STATUS_CLOSING) && millis()-(lastStatusTimestamp)>STATUS_CHANGE_MIN)
  {
    if (hallSensorRead(HALLSENSOR_OPENSIDE)==true)
      setStatus(STATUS_OPEN);
    else if (hallSensorRead(HALLSENSOR_CLOSEDSIDE)==true)
      setStatus(STATUS_CLOSED);
    else if (millis()-(lastStatusTimestamp)>DOOR_MOVEMENT_TIME)
      setStatus(STATUS_UNKNOWN);
  }
  
  if (radio.receiveDone())
  {
    byte newStatus=STATUS;
    boolean reportStatusRequest=false;
    lastRequesterNodeID = radio.SENDERID;
    DEBUG('[');DEBUG(radio.SENDERID);DEBUG("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      DEBUG((char)radio.DATA[i]);

    if (radio.DATALEN==3)
    {
      //check for an OPEN/CLOSE/STATUS request
      if (radio.DATA[0]=='O' && radio.DATA[1]=='P' && radio.DATA[2]=='N')
      {
        if (millis()-(lastStatusTimestamp) > STATUS_CHANGE_MIN && (STATUS == STATUS_CLOSED || STATUS == STATUS_CLOSING || STATUS == STATUS_UNKNOWN))
        {
          newStatus = STATUS_OPENING;
          bMode = HOLD_MODE;
        }
        //else radio.Send(requester, "INVALID", 7);
      }
      if (radio.DATA[0]=='C' && radio.DATA[1]=='L' && radio.DATA[2]=='S')
      {
        if (millis()-(lastStatusTimestamp) > STATUS_CHANGE_MIN && (STATUS == STATUS_OPEN || STATUS == STATUS_OPENING || STATUS == STATUS_UNKNOWN)){
          newStatus = STATUS_CLOSING;
          bMode = HOLD_MODE;
        }
        //else radio.Send(requester, "INVALID", 7);
      }
      if (radio.DATA[0]=='S' && radio.DATA[1]=='T' && radio.DATA[2]=='S')
      {
        reportStatusRequest = true;
      }
    }
    
    // wireless programming token check
    // DO NOT REMOVE, or GarageMote will not be wirelessly programmable any more!
    CheckForWirelessHEX(radio, flash, true);

    //first send any ACK to request
    DEBUG("   [RX_RSSI:");DEBUG(radio.RSSI);DEBUG("]");
    if (radio.ACKRequested())
    {
      radio.sendACK();
      DEBUGln(" - ACK sent.");
    }
    
    if (STATUS != newStatus)
    {
      setStatus(newStatus);
    }
    if (reportStatusRequest)
    {
      reportStatus();    
    }
    
    DEBUGln();
  }
  

  
  //use LED to visually indicate STATUS
  if (STATUS == STATUS_OPEN || STATUS == STATUS_CLOSED) //solid ON/OFF
  {
    digitalWrite(LED, STATUS == STATUS_OPEN ? LOW : HIGH);
  }
  if (STATUS == STATUS_OPENING || STATUS == STATUS_CLOSING) //pulse
  {
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
  }
  if (STATUS == STATUS_UNKNOWN) //blink
  {
    if (millis()-(ledPulseTimestamp) > LED_PULSE_PERIOD/20)
    {
      ledPulseDirection = !ledPulseDirection;
      digitalWrite(LED, ledPulseDirection ? HIGH : LOW);
      ledPulseTimestamp = millis();
    }
  }
}

//returns TRUE if magnet is next to sensor, FALSE if magnet is away
boolean hallSensorRead(byte which)
{
  //while(millis()-lastStatusTimestamp<STATUS_CHANGE_MIN);
//  digitalWrite(which ? HALLSENSOR2_EN : HALLSENSOR1_EN, HIGH); //turn sensor ON
//  delay(1); //wait a little
  byte reading = digitalRead(which ? HALLSENSOR2 : HALLSENSOR1);
//  digitalWrite(which ? HALLSENSOR2_EN : HALLSENSOR1_EN, LOW); //turn sensor OFF
  return reading==0;
}

void setStatus(byte newSTATUS, boolean reportStatusRequest)
{
  if (STATUS != newSTATUS) lastStatusTimestamp = millis();
  STATUS = newSTATUS;
  DEBUGln(STATUS==STATUS_CLOSED ? "CLOSED" : STATUS==STATUS_CLOSING ? "CLOSING" : STATUS==STATUS_OPENING ? "OPENING" : STATUS==STATUS_OPEN ? "OPEN" : "UNKNOWN");
  if (STATUS == STATUS_CLOSING) shutter_motor_control(SHUTTER_CLOSE);
  else if (STATUS == STATUS_OPENING) shutter_motor_control(SHUTTER_OPEN);
  else shutter_motor_control(SHUTTER_STOP);
  if (reportStatusRequest)
    reportStatus();
}

boolean reportStatus()
{
  if (lastRequesterNodeID == 0) return false;
  char buff[10];
  sprintf(buff, STATUS==STATUS_CLOSED ? "CLOSED" : STATUS==STATUS_CLOSING ? "CLOSING" : STATUS==STATUS_OPENING ? "OPENING" : STATUS==STATUS_OPEN ? "OPEN" : "UNKNOWN");
  byte len = strlen(buff);
  return radio.sendWithRetry(lastRequesterNodeID, buff, len, RETRY_NUM, ACK_TIME);
}

/*
void pulseRelay()
{
  digitalWrite(RELAYPIN1, HIGH);
  digitalWrite(RELAYPIN2, HIGH);
  delay(RELAY_PULSE_MS);
  digitalWrite(RELAYPIN1, LOW);
  digitalWrite(RELAYPIN2, LOW);
}
*/

void Blink(byte PIN, byte DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
