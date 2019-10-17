// **********************************************************************************************************
// Author: Felix Laevsky, Lina Laevsky Faingold
// IR sender for TadiranMulti Air Conditioner
// Allow to control devices using IR codes
// **********************************************************************************************************
// Air Conditioner modes are received from mote_gateway and IR codes sent to the Air Conditioner
// 


/*****************************************************************************
 * Includes
 * **************************************************************************/

#include <RFM69.h>         //get it here: http://github.com/lowpowerlab/rfm69
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming
#include <SPI.h>           //comes with Arduino IDE (www.arduino.cc)
#include <SPIFlashA.h>     //get it here: http://github.com/lowpowerlab/spiflash
#include <IRremote.h>
#include <avr/wdt.h>       //watchdog library
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "tadiranMulti.h"
#include "periph_cfg.h"
#include "config.h"

/*****************************************************************************
 * Defines
 * **************************************************************************/
//#define DEBUG_EN

#define DEVICE_TYPE_NAME        "IR Sender"

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

#define GPIO_LED_ONBOARD        9  //pin connected to onboard LED

#define SERIAL_BAUD             115200
#include "config.h"
#ifdef DEBUG_EN
  #define DNG_PRINT(input)   {Serial.print(input); delay(1);}
  #define DNG_PRINTln(input) {Serial.println(input); delay(1);}
#else
  #define DNG_PRINT(input);
  #define DNG_PRINTln(input);
#endif

#define CONFIGURATION_ADDR              0

/*****************************************************************************
 * Macros
 * **************************************************************************/

/*****************************************************************************
 * Types
 * **************************************************************************/
/* */
typedef struct s_ac_config_t
{
    byte mode        : 3;
    byte power       : 1;
    byte fan_speed   : 2;
    byte fan_angle   : 4;
    byte temperature : 4;
    byte light       : 1;
}s_ac_config;

typedef union u_ac_config_t
{
    s_ac_config config;
    byte        raw_data[2];
}u_ac_config;

/*****************************************************************************
 * Methods prototypes
 * **************************************************************************/

/*****************************************************************************
 * Variables
 * **************************************************************************/
IRsend irsend;
RFM69 radio;
SPIFlashA flash(SPI_CS, MANUFACTURER_ID); //EF40 for 16mbit windbond chip
tadiranMulti tmulti;
tadiranMultiCode1_tu sec1;
tadiranMultiCode2_tu sec2;
u_ac_config msg_rcv;
byte lastRequesterNodeID=GATEWAYID;

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
    Serial.begin(SERIAL_BAUD);
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
    }    

    GPIO_Init();

    Radio_Init();

#ifdef DEBUG_EN
    printDeviceInfo();
#endif
      
   if (flash.initialize())
   {
     DNG_PRINTln("SPI Flash Init OK!");
   }
   else
   {
     DNG_PRINTln("SPI Flash Init FAIL!");
   }
   
#ifdef DEBUG_EN
   spiFlashTest();
#endif
 
   wdt_enable (WDTO_1S);    
}

void loop()
{ 
    wdt_reset();
    
    Radio_Task();
    
    tmulti.worker();
    
    //irsend.sendTadiarMulti(unsigned long long *p_sec1, byte sec1_nbits, unsigned long *p_sec2, byte sec2_nbits);
//     tmulti._power           = tadiranMulti::ON;
//     tmulti._fan             = 3;
//     tmulti._temperature     = 16;
//     tmulti._mode            = tadiranMulti::MODE_HEAT;
//     tmulti._fan_angle       = tadiranMulti::ANGLE_AUTO;
//     tmulti._light           = 1;
//     
//     
//     tmulti.send();
//     
//     while(1);
}

void Radio_Task(void)
{
    if (radio.receiveDone())
    {
        boolean reportStatusRequest=false;
        lastRequesterNodeID = radio.SENDERID;
        DNG_PRINT('[');DNG_PRINT(radio.SENDERID);DNG_PRINT("] ");
        for (byte i = 0; i < radio.DATALEN; i++)
            DNG_PRINT((char)radio.DATA[i]);
        
        if (radio.DATALEN==5 || radio.DATALEN==3) 
        {
            //parse received message
            if (radio.DATA[0]=='C' && radio.DATA[1]=='F' && radio.DATA[2]=='G' && radio.DATALEN==5)
            {
                msg_rcv.raw_data[0] = radio.DATA[3];
                msg_rcv.raw_data[1] = radio.DATA[4];
                DNG_PRINT("mode=");DNG_PRINTln(msg_rcv.config.mode);
                DNG_PRINT("power=");DNG_PRINTln(msg_rcv.config.power);
                DNG_PRINT("fan_speed=");DNG_PRINTln(msg_rcv.config.fan_speed);
                DNG_PRINT("fan_angle=");DNG_PRINTln(msg_rcv.config.fan_angle);
                DNG_PRINT("temperature=");DNG_PRINTln(msg_rcv.config.temperature);
                DNG_PRINT("light=");DNG_PRINTln(msg_rcv.config.light);
                
                tmulti._power           = (tadiranMulti::Power)msg_rcv.config.power;
                tmulti._fan             = msg_rcv.config.fan_speed;
                tmulti._temperature     = msg_rcv.config.temperature;
                tmulti._mode            = (tadiranMulti::Mode)msg_rcv.config.mode;
                tmulti._fan_angle       = (tadiranMulti::FanAngle)msg_rcv.config.fan_angle;
                tmulti._light           = msg_rcv.config.light;
                
                tmulti.startToSend();
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
        DNG_PRINT("   [RX_RSSI:");DNG_PRINT(radio.RSSI);DNG_PRINT("]");
        if (radio.ACKRequested())
        {
            radio.sendACK();
            DNG_PRINTln(" - ACK sent.");
        }        
        
        if (reportStatusRequest)
        {
            reportStatus();    
        }
        
        DNG_PRINTln();
    }
}

boolean reportStatus()
{
    if (lastRequesterNodeID == 0) return false;
    char buff[8];
    
    byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
    DNG_PRINT( "Radio Temp is ");
    DNG_PRINT(temperature);
    DNG_PRINTln("C");
    
    sprintf(buff, "TMP%04X", temperature);
    return radio.sendWithRetry(lastRequesterNodeID, buff, 7, RETRY_NUM, ACK_TIME);
}

void tadiranMulti::send(int crc)
{   
    DNG_PRINTln("Send() - started");
    
    if (crc >= 16)
    {
        return;
    }
    
    digitalWrite(GPIO_LED_ONBOARD, HIGH);
    
    sec1.raw_data = 0x250000000;
    sec2.raw_data = 0;
    
    sec1.code.mode           = _mode;
    sec1.code.power1         = _power;
    if (_fan > 3) _fan = 3;
    sec1.code.fan_speed      = _fan;
    if(_fan_angle == ANGLE_SWEEP || _fan_angle == ANGLE_SWEEP_CENTRAL || _fan_angle == ANGLE_SWEEP_UP) 
         sec1.code.sweep = 1; 
//     if ( _temperature < 16) _temperature = 16;
//     if ( _temperature > 30) _temperature = 30;
//     sec1.code.temperature    = _temperature-16;
    sec1.code.temperature    = _temperature;
    sec1.code.light          = _light;
    sec1.code.power2         = _power;
    
    sec2.code.air_flow_angle = _fan_angle;
    
//    for ( int i = 0 ; i < 16 ; i++ )
//    {
//        sec2.code.crc = i;
        sec2.code.crc = crc;    
        irsend.sendTadiarMulti(&sec1.raw_data, 35, &sec2.raw_data, 32);
//        wdt_reset();
//         delay(100);
//    }
    
    digitalWrite(GPIO_LED_ONBOARD, LOW);
}

void tadiranMulti::startToSend()
{
    _crc = 0;
}

void tadiranMulti::worker()
{
    if (_crc < 16)
    {
        send(_crc);
        _crc++;
    }    
}