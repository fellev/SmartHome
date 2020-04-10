/*
 * ir_codes.h
 *
 *  Created on: 9 Nov 2019
 *      Author: felix
 */

#ifndef SYSTEM_INC_IR_CODES_H_
#define SYSTEM_INC_IR_CODES_H_

class irCodes
{
private:
    static const unsigned long ir_codes[];
public:
    unsigned long getIrCode(unsigned int index);
};

const unsigned  long irCodes::ir_codes =
{
    0xE0E040BF, /* on/off */
    0xE0E0807F, /* source */
    0xE0E0F00F, /* mute */
    0xE0E020DF, /* 1 */
    0xE0E0A05F, /* 2 */
    0xE0E0609F, /* 3 */
    0xE0E010EF, /* 4 */
    0xE0E0906F, /* 5 */
    0xE0E050AF, /* 6 */
    0xE0E030CF, /* 7 */
    0xE0E0B04F, /* 8 */
    0xE0E0708F, /* 9 */
    0xE0E08877, /* 0 */
    0xE0E034CB, /* txt/mix */
    0xE0E0C837, /* pre-ch */
    0xE0E0E01F, /* volume + */
    0xE0E0D02F, /* volume - */
    0xE0E048B7, /* channel + */
    0xE0E008F7, /* channel - */
    0xE0E0D629, /* ch list */
    0xE0E058A7, /* menu */
    0xE0E031CE, /* media p */
    0xE0E0F20D, /* guide */
    0xE0E0F807, /* info */
    0xE0E0D22D, /* tools */
    0xE0E006F9, /* up */
    0xE0E0A659, /* left */
    0xE0E046B9, /* right */
    0xE0E08679, /* down */
    0xE0E016E9, /* enter */
    0xE0E01AE5, /* return */
    0xE0E0B44B, /* exit */
    0xE0E0FC03, /* e-manual */
    0xE0E07C83, /* pic size */
    0xE0E0A45B, /* ad/subt */
    0xE0E0629D, /* stop */
    0xE0E0E21D, /* play */
    0xE0E052AD, /* pause */
    0xE0E0A25D, /* rewind */
    0xE0E012ED, /* forward */
    0xE0E036C9, /* a */
    0xE0E028D7, /* b */
    0xE0E0A857, /* c */
    0xE0E06897, /* d */
    0xE0E09E61, /* smart button */
    0xE0E0D12E, /* HDMI */
    0xE0E0D827  /* TV */
};

#endif /* SYSTEM_INC_IR_CODES_H_ */
