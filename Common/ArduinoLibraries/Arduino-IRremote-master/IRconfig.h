#ifndef IRCONFIG_H
#define IRCONFIG_H

//------------------------------------------------------------------------------
// Supported IR protocols
// Each protocol you include costs memory and, during decode, costs time
// Disable (set to 0) all the protocols you do not need/want!
//

//#define IR_RECEIVE            // Uncomment this line in case you want to use IR receive functionality

#define DECODE_RC5           0
#define SEND_RC5             0

#define DECODE_RC6           0
#define SEND_RC6             0

//#define DECODE_NEC           0
//#define SEND_NEC             0

#define DECODE_SONY          0
#define SEND_SONY            0

#define DECODE_PANASONIC     0
#define SEND_PANASONIC       0

#define DECODE_JVC           0
#define SEND_JVC             0

#define DECODE_SAMSUNG       0
#ifndef SEND_SAMSUNG
#define SEND_SAMSUNG         0
#endif

#define DECODE_WHYNTER       0
#define SEND_WHYNTER         0

#define DECODE_AIWA_RC_T501  0
#define SEND_AIWA_RC_T501    0

#define DECODE_LG            0
#define SEND_LG              0 

#define DECODE_SANYO         0
#define SEND_SANYO           0 // NOT WRITTEN

#define DECODE_MITSUBISHI    0
#define SEND_MITSUBISHI      0 // NOT WRITTEN

#define DECODE_DISH          0 // NOT WRITTEN
#define SEND_DISH            0

#define DECODE_SHARP         0 // NOT WRITTEN
#define SEND_SHARP           0

#define DECODE_DENON         0
#define SEND_DENON           0

#define DECODE_PRONTO        0 // This function doe not logically make sense
#define SEND_PRONTO          0

#define DECODE_TADIRAN_MULTI 0
#define SEND_TADIRAN_MULTI   0

#endif /* IRCONFIG_H */
