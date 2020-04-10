// **********************************************************************************************************
// Author: Felix Laevsky
// IR sender for Samsung TV
// Date: 3/11/2019
// **********************************************************************************************************


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
#include "periph_cfg.h"
#include "config.h"

/*****************************************************************************
 * Defines
 * **************************************************************************/

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
#define GPIO_TV_POWER_SENSOR    4  //pin connected to TV USB port to detect power state of the TV

#define GPIO_TV_POWER_DEBOUNCE_VAL 3
#define D_TV_IR_CODE_POWER 0

#define SERIAL_BAUD             115200
#include "config.h"
#ifdef DEBUG_EN
  #define print_dbg(input)   {Serial.print(input); delay(1);}
  #define println_dbg(input) {Serial.println(input); delay(1);}
 #define println2_dbg(a,b) {Serial.println(a,b); delay(1);}
 #define print2_dbg(a,b)   {Serial.print(a,b); delay(1);}
#else
  #define print_dbg(input)
  #define println_dbg(input)
#endif

#define CONFIGURATION_ADDR              0

#define SAMSUNG_BITS  32

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
 * Exported Variables
 * **************************************************************************/

extern const unsigned long samsungTvCodes[];

/*****************************************************************************
 * Variables
 * **************************************************************************/
IRsend irsend;
RFM69 radio;
SPIFlashA flash(SPI_CS, MANUFACTURER_ID); //EF40 for 16mbit windbond chip
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
#endif //DEBUG_ENirCodes

void GPIO_Init(void)
{
    pinMode(GPIO_TV_POWER_SENSOR, INPUT);
    pinMode(GPIO_LED_ONBOARD, OUTPUT);
}

int GPIO_IsTvOn(void)
{
    byte cnt=0;
    int pin_state;

    do
    {
        pin_state = digitalRead(GPIO_TV_POWER_SENSOR);
        delay(1);
        if (pin_state == digitalRead(GPIO_TV_POWER_SENSOR))
        {
            cnt++;
        }
        else
        {
            cnt = 0;
        }
    }while(cnt < GPIO_TV_POWER_DEBOUNCE_VAL);

    return pin_state;
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
     println_dbg("SPI Flash Init OK!");
   }
   else
   {
     println_dbg("SPI Flash Init FAIL!");
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
}

void Radio_Task(void)
{
    unsigned long ircode;

    if (radio.receiveDone())
    {
        boolean reportStatusRequest=false;
        lastRequesterNodeID = radio.SENDERID;
#ifdef DEBUG_EN
        print_dbg('[');print_dbg(radio.SENDERID);print_dbg("] ");
        for (byte i = 0; i < radio.DATALEN; i++)
            print2_dbg(radio.DATA[i], HEX);
        println_dbg("");
#endif

        if (radio.DATALEN==5)
        {
            //parse received message
            if (radio.DATA[0]=='C')
            {
            	println_dbg("IR CODE");
                digitalWrite(GPIO_LED_ONBOARD, HIGH);
                ircode = radio.DATA[1];
                ircode |= ((unsigned long)radio.DATA[2] << 8);
                ircode |= ((unsigned long)radio.DATA[3] << 16);
                ircode |= ((unsigned long)radio.DATA[4] << 24);
                irsend.sendSAMSUNG(ircode, SAMSUNG_BITS);
                digitalWrite(GPIO_LED_ONBOARD, LOW);
            }
            else if (radio.DATA[0]=='P')
            {
            	println_dbg("PWR CMD");
                if (((radio.DATA[1] == 0x01) && (GPIO_IsTvOn() == 0)) ||
                    ((radio.DATA[1] == 0x00) && (GPIO_IsTvOn() == 1)))
                {
                	println_dbg("SEND PWR IR CODE");
                    digitalWrite(GPIO_LED_ONBOARD, HIGH);
                    irsend.sendSAMSUNG_code(D_TV_IR_CODE_POWER);
                    digitalWrite(GPIO_LED_ONBOARD, LOW);
                }
                else
                {
                	println_dbg("NONE");
                }
            }
        }

        // wireless programming token check
        // DO NOT REMOVE, or GarageMote will not be wirelessly programmable any more!
        CheckForWirelessHEX(radio, flash, true, GPIO_LED_ONBOARD);

        //first send any ACK to request
        print_dbg("   [RX_RSSI:");print_dbg(radio.RSSI);print_dbg("]");
        if (radio.ACKRequested())
        {
            radio.sendACK();
            println_dbg(" - ACK sent.");
        }

        if (reportStatusRequest)
        {
            reportStatus();
        }

        println_dbg();
    }
}

boolean reportStatus()
{
    if (lastRequesterNodeID == 0) return false;
    char buff[8];

    byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
    print_dbg( "Radio Temp is ");
    print_dbg(temperature);
    println_dbg("C");

    sprintf(buff, "TMP%04X", temperature);
    return radio.sendWithRetry(lastRequesterNodeID, buff, 7, RETRY_NUM, ACK_TIME);
}

