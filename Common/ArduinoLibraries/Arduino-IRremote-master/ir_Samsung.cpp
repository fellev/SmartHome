#include "IRremote.h"
#include "IRremoteInt.h"

//==============================================================================
//              SSSS   AAA    MMM    SSSS  U   U  N   N   GGGG
//             S      A   A  M M M  S      U   U  NN  N  G
//              SSS   AAAAA  M M M   SSS   U   U  N N N  G  GG
//                 S  A   A  M   M      S  U   U  N  NN  G   G
//             SSSS   A   A  M   M  SSSS    UUU   N   N   GGG
//==============================================================================

#define SAMSUNG_BITS          32
#define SAMSUNG_HDR_MARK    5000
#define SAMSUNG_HDR_SPACE   5000
#define SAMSUNG_BIT_MARK     560
#define SAMSUNG_ONE_SPACE   1600
#define SAMSUNG_ZERO_SPACE   560
#define SAMSUNG_RPT_SPACE   2250

//+=============================================================================
#if SEND_SAMSUNG
const unsigned  long IRsend::ir_codes[] =
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
    0xE0E06897  /* d */
};

void  IRsend::sendSAMSUNG (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(38);

	// Header
	mark(SAMSUNG_HDR_MARK);
	space(SAMSUNG_HDR_SPACE);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(SAMSUNG_BIT_MARK);
			space(SAMSUNG_ONE_SPACE);
		} else {
			mark(SAMSUNG_BIT_MARK);
			space(SAMSUNG_ZERO_SPACE);
		}
	}

	// Footer
	mark(SAMSUNG_BIT_MARK);
    space(0);  // Always end with the LED off
}

void  IRsend::sendSAMSUNG_code (unsigned int index)
{
	this->sendSAMSUNG(this->ir_codes[index], SAMSUNG_BITS);
}
#endif

//+=============================================================================
// SAMSUNGs have a repeat only 4 items long
//
#if DECODE_SAMSUNG
bool  IRrecv::decodeSAMSUNG (decode_results *results)
{
	long  data   = 0;
	int   offset = 1;  // Skip first space

	// Initial mark
	if (!MATCH_MARK(results->rawbuf[offset], SAMSUNG_HDR_MARK))   return false ;
	offset++;

	// Check for repeat
	if (    (irparams.rawlen == 4)
	     && MATCH_SPACE(results->rawbuf[offset], SAMSUNG_RPT_SPACE)
	     && MATCH_MARK(results->rawbuf[offset+1], SAMSUNG_BIT_MARK)
	   ) {
		results->bits        = 0;
		results->value       = REPEAT;
		results->decode_type = SAMSUNG;
		return true;
	}
	if (irparams.rawlen < (2 * SAMSUNG_BITS) + 4)  return false ;

	// Initial space
	if (!MATCH_SPACE(results->rawbuf[offset++], SAMSUNG_HDR_SPACE))  return false ;

	for (int i = 0;  i < SAMSUNG_BITS;   i++) {
		if (!MATCH_MARK(results->rawbuf[offset++], SAMSUNG_BIT_MARK))  return false ;

		if      (MATCH_SPACE(results->rawbuf[offset], SAMSUNG_ONE_SPACE))   data = (data << 1) | 1 ;
		else if (MATCH_SPACE(results->rawbuf[offset], SAMSUNG_ZERO_SPACE))  data = (data << 1) | 0 ;
		else                                                                return false ;
		offset++;
	}

	// Success
	results->bits        = SAMSUNG_BITS;
	results->value       = data;
	results->decode_type = SAMSUNG;
	return true;
}
#endif

