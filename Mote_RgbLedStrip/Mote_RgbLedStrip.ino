// **********************************************************************************************************
// Author: Felix Laevsky
// Original author: felix@lowpowerlab.com, http://www.LowPowerLab.com
// Mote_RgbLedStrip controller sketch that works with Moteinos equipped with HopeRF RFM69W/RFM69HW
// Allow to control RGB led strip using low cost led amplifier
// **********************************************************************************************************
// led's PWM value is received from mote_gateway and pwm outputs is updated accordingly  
// **********************************************************************************************************

/*****************************************************************************
 * Includes
 * **************************************************************************/

#include <RFM69.h>         //get it here: http://github.com/lowpowerlab/rfm69
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming
#include <SPI.h>           //comes with Arduino IDE (www.arduino.cc)
#include <SPIFlashA.h>      //get it here: http://github.com/lowpowerlab/spiflash
#include <avr/wdt.h>       //watchdog library
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "periph_cfg.h"


/*****************************************************************************
 * Defines
 * **************************************************************************/

//#define DEBUG_EN

#define DEVICE_TYPE_NAME        "RGB LED CONTROLLER"

#define CHECK_VIRGIN_VALUE      0x55
#define GATEWAYID               1
#define NETWORKID               100
#define FREQUENCY               RF69_433MHZ
//#define FREQUENCY             RF69_868MHZ
//#define FREQUENCY             RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define ENCRYPTKEY              ")nXLceHCQkaU{-5@" //"sampleEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW              //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME                30  // # of ms to wait for an ack
#define RETRY_NUM               0

#define BTNCOUNT                1

#define GPIO_BTNPWR             4  //digital pin for Power OFF/ON of the led strip
#define GPIO_LED_RED            3  //PWM pin for red led

#if (DEVICE_BOARD == BOARD_MINI_WIRELESS_MOTEINO_COMPATIBLE)
#define GPIO_LED_GREEN          7  //PWM pin for green led
#elif (DEVICE_BOARD == BOARD_MINI_WIRELESS_MOTEINO)
#define GPIO_LED_GREEN          5
#endif
#define GPIO_LED_BLUE           6  //PWM pin for blue led
#define GPIO_LED_ONBOARD        9  //pin connected to onboard LED

#define PWM_MAX_VALUE           255
#define PWM_MIN_VALUE           0

#define LED_PWM_VALUE_ON        PWM_MAX_VALUE
#define LED_PWM_VALUE_OFF       PWM_MIN_VALUE

#define LED_PULSE_PERIOD        5000   //5s seems good value for pulsing/blinking (not too fast/slow)

#define BTNPWR_INDEX            0

#define BUTTON_BOUNCE_MS            200  //timespan before another button change can occur
#define PRESSED                     0
#define RELEASED                    1
#define ON                          1
#define OFF                         0

#define FADE_INTERVAL_MS            30
#define FADE_IN_STEP_VALUE          5
#define FADE_OUT_STEP_VALUE         (-5)
#define COLOR_UPDATE_INTERVAL_MS    30
#define COLOR_UPDATE_INTERVAL_VAL   10
#define COLOR_SAVE_INTERVAL_MS      5000 // 5 sec

#define SERIAL_BAUD                 115200

#ifdef DEBUG_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

#define CONFIGURATION_ADDR              0
#define LEDS_PWM_VALUES_ADDR            CONFIGURATION_ADDR + sizeof(CONFIG)

/*****************************************************************************
 * Macros
 * **************************************************************************/
#if (LED_PWM_VALUE_OFF > LED_PWM_VALUE_ON)
#define IS_INTENSITY_GREATER_THEN(intensity, reference_intensity) ((intensity) < (reference_intensity))
#else
#define IS_INTENSITY_GREATER_THEN(intensity, reference_intensity) ((intensity) > (reference_intensity))
#endif

/*****************************************************************************
 * Types
 * **************************************************************************/
enum powerState_e { POWER_STATE_OFF, POWER_STATE_ON };  

enum fadeState_e
{
    FADE_STATE_OFF,
    FADE_STATE_FADE_IN,
    FADE_STATE_FADE_OUT
};

enum ledsColor_e
{
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_COUNT
};

struct configuration 
{
    byte check_virgin;
    byte nodeID;
    byte networkID;
    byte separator1;          //separators needed to keep strings from overlapping
    char description[10];
    byte separator2;
} CONFIG;



struct ledsStrip_s
{
    int ledPwmValue[LED_COLOR_COUNT];
    powerState_e powerState;
    fadeState_e fadeState;
} ledsStrip;


/*****************************************************************************
 * Methods prototypes
 * **************************************************************************/
void PowerButton_Handle(void);
void Buttons_Task(void);
boolean reportStatus();
void Radio_Task(void);
void Leds_Task(void);
void Leds_SetPowerStateNew(powerState_e state);
void Leds_SetNewColor(byte red, byte green, byte blue);

/*****************************************************************************
 * Variables
 * **************************************************************************/

byte lastRequesterNodeID=GATEWAYID;
RFM69 radio;
long now=0;
byte btnIndex=0; // as the sketch loops this index will loop through the available physical buttons
byte btn[] = {GPIO_BTNPWR};
byte btnLastState[] = {RELEASED,RELEASED};
byte ledIndexPin[] = {GPIO_LED_RED, GPIO_LED_GREEN, GPIO_LED_BLUE};
byte ledIndexPwmValueNew[LED_COLOR_COUNT];
byte btnState=RELEASED;
boolean isLedsColorUpdated = false;
boolean isLedsColorStillChanging = false;
unsigned long btnLastPress[]={0};
unsigned long fadeLastUpdate = 0;
unsigned long colorLastUpdate = 0;
unsigned long colorLastSave = 0;
int fadeDeltaValue = 0;
fadeState_e fadeState, fadeStateNew;
powerState_e powerStateNew;
byte i;
char input;
unsigned long ledPulseTimestamp=0;
boolean ledPulseDirection=false;

/////////////////////////////////////////////////////////////////////////////
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0xEF30 for windbond 4mbit flash (Moteino OEM)
/////////////////////////////////////////////////////////////////////////////
SPIFlashA flash(SPI_CS, MANUFACTURER_ID);

/*****************************************************************************
 * Methods
 * **************************************************************************/
#ifdef DEBUG_EN
byte flashBuffer[90];                // Define a read buffer for readBytes() tests
void printDeviceInfo()
{
  Serial.println();
  Serial.println("-----------------------------------------------");
  Serial.println(DEVICE_TYPE_NAME);
  Serial.println("-----------------------------------------------");
  Serial.println( "Compiled: " __DATE__ ", " __TIME__ ", " __VERSION__);
  Serial.println("-----------------------------------------------");
  Serial.print("Node ID: ");Serial.println(CONFIG.nodeID);
  Serial.print("Network ID: ");Serial.println(CONFIG.networkID);
  Serial.print("Description: ");Serial.println(CONFIG.description);
  Serial.print("RF Frequency: ");Serial.print(FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);Serial.println(" Mhz");
}

void spiFlashTest()
{
    /* 3. Bulk Erase of the chip 
    *    ====================== */
    /*
    Serial.println ("Test 3: Bulk Erase of the chip - WARNING this may take about 45s");
    Serial.print ("Read address 16777215 (HEX): ");         
    Serial.println (flash.readByte(16777215),HEX);          // Read last location of the Flash Memory         
    Serial.println ("Write address 16777215 (HEX) AA ");
    flash.writeByte(16777215,0xAA);                         // Write to this location (should work if this location was previously erased)
    Serial.print ("Read address 16777215 (HEX): ");
    Serial.println (flash.readByte(16777215),HEX);          // Verify that the location was correctly written
    Serial.println ("Erasing 16MBytes ");
    long start = millis();                                  // Start time before starting the Erase
    flash.bulkErase();                                      // Initiate a the erase
        while(flash.busy());                              // Wait until finished
        Serial.print("DONE after (ms): ");Serial.println (millis()-start);  // Print the erasure time
    Serial.print ("Read address 16777215 (HEX): ");
    Serial.println (flash.readByte(16777215),HEX);          // Verify that the location is well erased
    */
    
    flash.blockErase32K(0);

    Serial.println ("Test 10: Read - Write - Read 44 Bytes into buffer");
    Serial.print ("Read Bulk: "); 
    flash.readBytes (0,flashBuffer,44);
    for (int i = 0; i <44; i++)
    {
    Serial.print((char) flashBuffer[i]);
    }
    Serial.println ();
    Serial.println ("Erasing 4K to write:");
    flash.blockErase4K(0);
    Serial.print ("Verify Erasure: ");
    flash.readBytes (0,flashBuffer,44);
    for (int i = 0; i <44; i++)
    {
    Serial.print((char) flashBuffer[i]);
    }
    Serial.println ();
    Serial.println ("Write Bulk: The quick brown fox jumps over the lazy dog");
    flash.writeBytes (0,"The quick brown fox jumps over the lazy dog",44);
    while (flash.busy());
    Serial.print ("Read Bulk:  ");
    flash.readBytes (0,flashBuffer,44);
    for (int i = 0; i <44; i++)
    {
    Serial.print((char) flashBuffer[i]);
    }
    Serial.println ();    
}
#endif //DEBUG_EN

void GPIO_Init(void)
{
    for ( i = 0 ; i < LED_COLOR_COUNT; i++)
    {
        pinMode(ledIndexPin[i], OUTPUT);
        analogWrite(ledIndexPin[i], LED_PWM_VALUE_OFF);
        ledsStrip.ledPwmValue[i] = LED_PWM_VALUE_OFF;
    }
    pinMode(GPIO_BTNPWR, INPUT);digitalWrite(GPIO_BTNPWR, HIGH); //activate pullup
    pinMode(GPIO_LED_ONBOARD, OUTPUT);
}

void Radio_Init(void)
{
    radio.initialize(FREQUENCY,CONFIG.nodeID,CONFIG.networkID);
#ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
#endif
    radio.encrypt(ENCRYPTKEY);
}

void setup(void)
{
    ledsStrip.powerState = POWER_STATE_OFF;
    powerStateNew = POWER_STATE_OFF;
    ledsStrip.fadeState = FADE_STATE_OFF;
    fadeStateNew = FADE_STATE_OFF;
    
    Serial.begin(SERIAL_BAUD);
    
    //Read leds pwm values from eeprom
    EEPROM_readAnything(LEDS_PWM_VALUES_ADDR, ledIndexPwmValueNew);

    // Read configuration from eeprom
    EEPROM_readAnything(CONFIGURATION_ADDR, CONFIG);
    if (CONFIG.check_virgin!=CHECK_VIRGIN_VALUE) // virgin CONFIG, expected [0x55]
    {
        Serial.println("No valid config found in EEPROM, writing defaults");
        CONFIG.separator1=CONFIG.separator2=0;
        CONFIG.check_virgin = CHECK_VIRGIN_VALUE;
        CONFIG.description[0]=0;
        CONFIG.nodeID=0;
        CONFIG.networkID=NETWORKID;
        for ( i = 0 ; i < LED_COLOR_COUNT; i++)
        {
            ledIndexPwmValueNew[i] = LED_PWM_VALUE_OFF;
        }
    }    

    GPIO_Init();

    Radio_Init();
    
#ifdef DEBUG_EN
    printDeviceInfo();
#endif
      
   if (flash.initialize())
   {
     DEBUGln("SPI Flash Init OK!");
   }
   else
   {
     DEBUGln("SPI Flash Init FAIL!");
   }
   
#ifdef DEBUG_EN
   spiFlashTest();
#endif
 
   wdt_enable (WDTO_1S);
    
   Leds_SetPowerStateNew(POWER_STATE_ON);
}


void loop()
{
    now = millis();
    
    wdt_reset();
    
    if (Serial.available())
    {
        input = Serial.read();
        DEBUG(input);
        #ifdef DEBUG_EN
        switch(input)
        {
            case 'y': // Power On
                Leds_SetPowerStateNew(POWER_STATE_ON);
                break;
            case 'n': // Power Off
                Leds_SetPowerStateNew(POWER_STATE_OFF);
                break;
            case 'r':
                Leds_SetNewColor(255,0,0);
                break;
            case 'g':
                Leds_SetNewColor(0,255,0);
                break;
            case 'b':
                Leds_SetNewColor(0,0,255);
                break;
            case 'w':
                Leds_SetNewColor(255,255,255);
                break;
            case 'o':
                Leds_SetNewColor(0xff,0x77,0x23); //white
                break;                
            case '0':
                Leds_SetNewColor(0,0,0);
                break;
            default:
                DEBUGln("Unknown");
                
        }
        #endif        
    }

    wdt_reset();
    
    Buttons_Task();
 
    Radio_Task();
    
    Leds_Task(); 
}

void Buttons_Task(void)
{
    btnIndex++;
    if (btnIndex>BTNCOUNT-1) btnIndex=0;  
    
    btnState = digitalRead(btn[btnIndex]);
    
    if (btnState != btnLastState[btnIndex] && now-btnLastPress[btnIndex] >= BUTTON_BOUNCE_MS) //button event happened
    {
        DEBUG(" Button");DEBUGln((btnState == PRESSED) ? " pressed" : " released");
        btnLastState[btnIndex] = btnState;
        if (btnState == PRESSED)
        {
            btnLastPress[btnIndex] = now;
            PowerButton_Handle();
        }
    }    
}

void PowerButton_Handle(void)
{
    if (ledsStrip.powerState == POWER_STATE_OFF)
    {
        Leds_SetPowerStateNew(POWER_STATE_ON);
    }
    else
    {
        Leds_SetPowerStateNew(POWER_STATE_OFF);
    }
}

void Radio_Task(void)
{
    if (radio.receiveDone())
    {
        boolean reportStatusRequest=false;
        lastRequesterNodeID = radio.SENDERID;
        DEBUG('[');DEBUG(radio.SENDERID);DEBUG("] ");
        for (byte i = 0; i < radio.DATALEN; i++)
            DEBUG((char)radio.DATA[i]);
        
        if (radio.DATALEN==3 || radio.DATALEN==6)
        {
            //check for POWER ON/POWER OFF/SET COLOR
            if (radio.DATA[0]=='P' && radio.DATA[1]=='O' && radio.DATA[2]=='N')
            {
                Leds_SetPowerStateNew(POWER_STATE_ON);
            }
            else if (radio.DATA[0]=='P' && radio.DATA[1]=='O' && radio.DATA[2]=='F')
            {
                Leds_SetPowerStateNew(POWER_STATE_OFF);
            }
            else if (radio.DATA[0]=='C' && radio.DATA[1]=='L' && radio.DATA[2]=='R')
            {
                Leds_SetNewColor(radio.DATA[3], radio.DATA[4], radio.DATA[5]);
            }
            else if (radio.DATA[0]=='S' && radio.DATA[1]=='T' && radio.DATA[2]=='S')
            {
                reportStatusRequest = true;
            }
        }
        
        // wireless programming token check
        // DO NOT REMOVE, or GarageMote will not be wirelessly programmable any more!
       CheckForWirelessHEX(radio, flash, true, GPIO_LED_ONBOARD);

        //first send any ACK to request
        DEBUG("   [RX_RSSI:");DEBUG(radio.RSSI);DEBUG("]");
        if (radio.ACKRequested())
        {
            radio.sendACK();
            DEBUGln(" - ACK sent.");
        }        
        
        if (reportStatusRequest)
        {
            reportStatus();    
        }
        
        DEBUGln();
    }
}

boolean reportStatus()
{
    if (lastRequesterNodeID == 0) return false;
    char buff[5];
    sprintf(buff, ledsStrip.powerState == POWER_STATE_ON ? "ON" : "OFF");
    byte len = strlen(buff);
    return radio.sendWithRetry(lastRequesterNodeID, buff, len, RETRY_NUM, ACK_TIME);
}

void Leds_Task(void)
{
    boolean isStillFadingCnt  = 0;
    int delta;
    #ifdef DEBUG_EN
    char buff[15];    
    #endif
    
    //Handle fade in
    if (fadeStateNew != ledsStrip.fadeState)
    {
        fadeLastUpdate = 0;
        ledsStrip.fadeState = fadeStateNew;
    }
    
    if ( (now - fadeLastUpdate) > FADE_INTERVAL_MS)
    {
        fadeLastUpdate = now;
        if (ledsStrip.fadeState == FADE_STATE_FADE_IN)
            fadeDeltaValue = FADE_IN_STEP_VALUE;
        else if (ledsStrip.fadeState == FADE_STATE_FADE_OUT)
            fadeDeltaValue = FADE_OUT_STEP_VALUE;
        else if (ledsStrip.fadeState == FADE_STATE_OFF)
            fadeDeltaValue = 0;
        
        if (fadeDeltaValue != 0)
        {
            for ( i = 0 ; i < LED_COLOR_COUNT ; i++)
            {
                ledsStrip.ledPwmValue[i] += fadeDeltaValue;
                if (IS_INTENSITY_GREATER_THEN(ledsStrip.ledPwmValue[i], ledIndexPwmValueNew[i]))
                {
                    ledsStrip.ledPwmValue[i] = ledIndexPwmValueNew[i];
                }
                if (ledsStrip.ledPwmValue[i] > PWM_MAX_VALUE)
                    ledsStrip.ledPwmValue[i] = PWM_MAX_VALUE;
                else if (ledsStrip.ledPwmValue[i] < PWM_MIN_VALUE)
                    ledsStrip.ledPwmValue[i] = PWM_MIN_VALUE;
                
                analogWrite(ledIndexPin[i], ledsStrip.ledPwmValue[i]);
                
                #ifdef DEBUG_EN
                sprintf(buff, "F[%d] %d", i, ledsStrip.ledPwmValue[i]);
                DEBUGln(buff);
                #endif
                
//                if (!(ledsStrip.ledPwmValue[i] == PWM_MAX_VALUE || 
//                    ledsStrip.ledPwmValue[i] == PWM_MIN_VALUE))
                if (ledsStrip.ledPwmValue[i] != ledIndexPwmValueNew[i])
                {
                    isStillFadingCnt++;
                }
            }
            fadeDeltaValue = 0;
            if (isStillFadingCnt == 0)
            {
                fadeStateNew = FADE_STATE_OFF;
            }
        }        
    }

    //Handle color update
    if (isLedsColorStillChanging && 
        ((now - colorLastUpdate) > COLOR_UPDATE_INTERVAL_MS))
    {
        colorLastUpdate = now;
        isLedsColorStillChanging = false;
        for ( i = 0 ; i < LED_COLOR_COUNT ; i++)
        {
            if (ledsStrip.ledPwmValue[i] != ledIndexPwmValueNew[i])
            {
                delta = ledIndexPwmValueNew[i];
                delta -= ledsStrip.ledPwmValue[i];
                if (delta < 0)
                {
                    if (delta < -COLOR_UPDATE_INTERVAL_VAL)
                        delta = -COLOR_UPDATE_INTERVAL_VAL;
                }
                else
                {
                    if (delta > COLOR_UPDATE_INTERVAL_VAL)
                        delta = COLOR_UPDATE_INTERVAL_VAL;                    
                }
                ledsStrip.ledPwmValue[i] += delta;
                isLedsColorStillChanging = true;
                #ifdef DEBUG_EN
                sprintf(buff, "U[%d] %d", i, ledsStrip.ledPwmValue[i]);
                DEBUGln(buff);
                #endif
                analogWrite(ledIndexPin[i], ledsStrip.ledPwmValue[i]);
            }
        }     
    }
    
    if (isLedsColorUpdated &&
        ((now - colorLastSave) > COLOR_SAVE_INTERVAL_MS))
    {
        colorLastSave = now;
        isLedsColorUpdated = false;
        EEPROM_writeAnything(LEDS_PWM_VALUES_ADDR, ledIndexPwmValueNew);
        DEBUGln("Leds PWM values written to eeprom");
    }
    
    
    // On-board led blink
    if ((now-ledPulseTimestamp) > LED_PULSE_PERIOD/20)
    {
        ledPulseDirection = !ledPulseDirection;
        digitalWrite(GPIO_LED_ONBOARD, ledPulseDirection ? HIGH : LOW);
        ledPulseTimestamp = now;
    }
            
}

void Leds_SetPowerStateNew(powerState_e state)
{
    powerStateNew = state;
    
    if (ledsStrip.powerState != powerStateNew)
    {
        isLedsColorUpdated = false;
        if (powerStateNew == POWER_STATE_ON)
        {
            fadeStateNew = FADE_STATE_FADE_IN;
        }
        else
        {
            fadeStateNew = FADE_STATE_FADE_OUT;
        }
        ledsStrip.powerState = powerStateNew;
    }
    
}

void Leds_SetNewColor(byte red, byte green, byte blue)
{
    if (isLedsColorStillChanging == false)
        colorLastUpdate = 0;
    isLedsColorStillChanging = true;
    
    if (ledIndexPwmValueNew[LED_COLOR_RED] != red || 
        ledIndexPwmValueNew[LED_COLOR_BLUE] != blue ||
        ledIndexPwmValueNew[LED_COLOR_GREEN] != green)
    {
        ledIndexPwmValueNew[LED_COLOR_RED] = red;
        ledIndexPwmValueNew[LED_COLOR_BLUE] = blue;
        ledIndexPwmValueNew[LED_COLOR_GREEN] = green;
        isLedsColorUpdated = true;
        colorLastSave = now; // We don't want to write epprom right after color update
        #ifdef DEBUG_EN
        char buff[50];
        sprintf(buff, "R:%d,G:%d,B:%d", red, green, blue);
        DEBUGln(buff);
        #endif
        
    }

}


