#include "Arduino.h"
#include <IRremoteInt.h>
#include <IRremote.h>
#include "Mote_irTranslator.h"

#ifdef DEBUG_EN
#define DNG_PRINT(...)   (Serial.print(__VA_ARGS__))
#define DNG_PRINTln(...) (Serial.println(__VA_ARGS__))
#else
  #define DNG_PRINT(...);
  #define DNG_PRINTln(...);
#endif

int RECV_PIN = 4;
int ONBOARD_LED_PIN = LED_BUILTIN;

IRrecv irrecv(RECV_PIN, ONBOARD_LED_PIN);
IRsend irsend;
decode_results results;

typedef struct s_ir_translation_t
{
    unsigned long yes;
    unsigned long visionnet;
} s_ir_translation;

s_ir_translation G_irTranslation[] =
{
{ D_STB_YES_POWER, D_STB_VISIONNET_POWER, },
{ D_STB_YES_VOLUME_UP, D_STB_VISIONNET_VOLUME_UP, },
{ D_STB_YES_VOLUME_DOWN, D_STB_VISIONNET_VOLUME_DOWN, },
{ D_STB_YES_CHANNEL_UP, D_STB_VISIONNET_UP /* D_STB_VISIONNET_PAGE_UP */, },
{ D_STB_YES_CHANNEL_DOWN, D_STB_VISIONNET_DOWN /*D_STB_VISIONNET_PAGE_DOWN*/, },
{ D_STB_YES_OK, D_STB_VISIONNET_OK, },
{ D_STB_YES_UP, D_STB_VISIONNET_UP, },
{ D_STB_YES_DOWN, D_STB_VISIONNET_DOWN, },
{ D_STB_YES_LEFT, D_STB_VISIONNET_LEFT, },
{ D_STB_YES_RIGHT, D_STB_VISIONNET_RIGHT, },
{ D_STB_YES_GUIDE, D_STB_VISIONNET_MENU /*D_STB_VISIONNET_EPG*/, },
{ D_STB_YES_MOSAIC, D_STB_VISIONNET_EXIT, },
{ D_STB_YES_SYNO, D_STB_VISIONNET_INFO, },
{ D_STB_YES_HELP, D_STB_VISIONNET_MENU, },
{ D_STB_YES_MUTE, D_STB_VISIONNET_MUTE, },
{ D_STB_YES_1, D_STB_VISIONNET_1, },
{ D_STB_YES_2, D_STB_VISIONNET_2, },
{ D_STB_YES_3, D_STB_VISIONNET_3, },
{ D_STB_YES_4, D_STB_VISIONNET_4, },
{ D_STB_YES_5, D_STB_VISIONNET_5, },
{ D_STB_YES_6, D_STB_VISIONNET_6, },
{ D_STB_YES_7, D_STB_VISIONNET_7, },
{ D_STB_YES_8, D_STB_VISIONNET_8, },
{ D_STB_YES_9, D_STB_VISIONNET_9, },
{ D_STB_YES_0, D_STB_VISIONNET_0, },
{ D_STB_YES_AV, D_STB_VISIONNET_EXIT, },
{ D_STB_YES_MUSIC, D_STB_VISIONNET_RECALL, },
{ D_STB_YES_RED, D_STB_VISIONNET_RED, },
{ D_STB_YES_GREEN, D_STB_VISIONNET_GREEN, },
{ D_STB_YES_YELLOW, D_STB_VISIONNET_YELLOW, },
{ D_STB_YES_BLUE, D_STB_VISIONNET_BLUE, },
{ D_STB_YES_BACK, D_STB_VISIONNET_EXIT, } };

//The setup function is called once at startup of the sketch
void setup()
{
    Serial.begin(115200);
    irrecv.enableIRIn(); // Start the receiver
    DNG_PRINTln("Start");
}

//On success return 0
int translateYesToVisionnet(unsigned long ir_code_yes,
        unsigned long *p_ir_code_visionnet)
{
    unsigned int i;
    for (i = 0; i < sizeof(G_irTranslation) / sizeof(s_ir_translation); i++)
    {
        if (G_irTranslation[i].yes == ir_code_yes)
        {
            *p_ir_code_visionnet = G_irTranslation[i].visionnet;
            return 0;
        }
    }
    return 1;
}

// The loop function is called in an endless loop
void loop()
{
    unsigned long irCode;

    if (irrecv.decode(&results))
    {
        if (results.decode_type == NEC)
        {
            DNG_PRINT("Received NEC: ");

            if (!translateYesToVisionnet(results.value, &irCode))
            {
                irsend.sendNEC(irCode, 32);
                DNG_PRINT("Send to visionnet: ");
                DNG_PRINTln(irCode, HEX);
            }
        }
        else
        {
            DNG_PRINT("Received Unknown: ");
//            /* Test code */
//            irCode = D_STB_VISIONNET_MUTE;
//            irsend.sendNEC(irCode, 32);
//            DNG_PRINT("Send to visionnet: ");
//            DNG_PRINTln(irCode, HEX);
//            /*************/
        }
        DNG_PRINTln(results.value, HEX);
        irrecv.resume(); // Receive the next value
        irrecv.enableIRIn();
    }

//    /* Test code */
//    irCode = D_STB_VISIONNET_MUTE;
//    irsend.sendNEC(irCode, 32);
//    DNG_PRINT("Send to visionnet: ");
//    DNG_PRINTln(irCode, HEX);
//    irrecv.resume(); // Receive the next value
//    irrecv.enableIRIn();
//    delay(1000);
//    /*************/
}
