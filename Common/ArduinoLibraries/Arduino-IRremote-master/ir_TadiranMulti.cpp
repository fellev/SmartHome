/*
Assuming the protocol we are adding is for the (imaginary) manufacturer:  Shuzu

Our fantasy protocol is a standard protocol, so we can use this standard
template without too much work. Some protocols are quite unique and will require
considerably more work in this file! It is way beyond the scope of this text to
explain how to reverse engineer "unusual" IR protocols. But, unless you own an
oscilloscope, the starting point is probably to use the rawDump.ino sketch and
try to spot the pattern!

Before you start, make sure the IR library is working OK:
  # Open up the Arduino IDE
  # Load up the rawDump.ino example sketch
  # Run it

Now we can start to add our new protocol...

1. Copy this file to : ir_Shuzu.cpp

2. Replace all occurrences of "Shuzu" with the name of your protocol.

3. Tweak the #defines to suit your protocol.

4. If you're lucky, tweaking the #defines will make the default send() function
   work.

5. Again, if you're lucky, tweaking the #defines will have made the default
   decode() function work.

You have written the code to support your new protocol!

Now you must do a few things to add it to the IRremote system:

1. Open IRremote.h and make the following changes:
   REMEMEBER to change occurences of "SHUZU" with the name of your protocol

   A. At the top, in the section "Supported Protocols", add:
      #define DECODE_SHUZU  1
      #define SEND_SHUZU    1

   B. In the section "enumerated list of all supported formats", add:
      SHUZU,
      to the end of the list (notice there is a comma after the protocol name)

   C. Further down in "Main class for receiving IR", add:
      //......................................................................
      #if DECODE_SHUZU
          bool  decodeShuzu (decode_results *results) ;
      #endif

   D. Further down in "Main class for sending IR", add:
      //......................................................................
      #if SEND_SHUZU
          void  sendShuzu (unsigned long data,  int nbits) ;
      #endif

   E. Save your changes and close the file

2. Now open irRecv.cpp and make the following change:

   A. In the function IRrecv::decode(), add:
      #ifdef DECODE_NEC
          DBG_PRINTLN("Attempting Shuzu decode");
          if (decodeShuzu(results))  return true ;
      #endif

   B. Save your changes and close the file

You will probably want to add your new protocol to the example sketch

3. Open MyDocuments\Arduino\libraries\IRremote\examples\IRrecvDumpV2.ino

   A. In the encoding() function, add:
      case SHUZU:    Serial.print("SHUZU");     break ;

Now open the Arduino IDE, load up the rawDump.ino sketch, and run it.
Hopefully it will compile and upload.
If it doesn't, you've done something wrong. Check your work.
If you can't get it to work - seek help from somewhere.

If you get this far, I will assume you have successfully added your new protocol
There is one last thing to do.

1. Delete this giant instructional comment.

2. Send a copy of your work to us so we can include it in the library and
   others may benefit from your hard work and maybe even write a song about how
   great you are for helping them! :)

Regards,
  BlueChip
*/

#include "IRremote.h"
#include "IRremoteInt.h"
// #include "ir_TadiranMulti.h"

//#define DEBUG_EN

#ifdef DEBUG_EN
  #define DEBUG(input)   Serial.print(input)
  #define DEBUGln(input) Serial.println(input)
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

//==============================================================================
//
//
//                              T A D I R A N   M U L T I
//
//
//==============================================================================

#define BITS         250  // The number of bits in the command

#define HDR1_MARK   8900  // The length of the Header:Mark
#define HDR1_SPACE  4450  // The lenght of the Header:Space

#define HDR2_MARK   650
#define HDR2_SPACE  19850 

#define HDR3_MARK1  650
#define HDR3_SPACE1 39650 
#define HDR3_MARK2  8850
#define HDR3_SPACE2 4500

#define HDR4_MARK   650
#define HDR4_SPACE  19850

#define SEC4_NBITS  18

#define BIT_MARK    550  // The length of a Bit:Mark
#define ONE_SPACE   1700 // The length of a Bit:Space for 1's
#define ZERO_SPACE  650  // The length of a Bit:Space for 0's

#define OTHER       1234  // Other things you may need to define

//+=============================================================================
//

#if SEND_TADIRAN_MULTI

void IRsend::sendSection(unsigned long long data, byte nbits)
{
    byte i;
    for (unsigned long long mask = 1UL, i = 0;  i < nbits ;  mask <<= 1, i++) {
        if (data & mask) {
            mark (BIT_MARK);
            space(ONE_SPACE);
            DEBUG("1");
        } else {
            mark (BIT_MARK);
            space(ZERO_SPACE);
            DEBUG("0");
        }
    }
}

void  IRsend::sendTadiarMulti(unsigned long long *p_sec1, byte sec1_nbits, unsigned long *p_sec2, byte sec2_nbits)
{
	// Set IR carrier frequency
	enableIROut(38);

    DEBUG("[HDR1]");
	// Header 1
	mark (HDR1_MARK);
	space(HDR1_SPACE);

	// Send Data of section 1
    sendSection(*p_sec1, sec1_nbits);
	
    DEBUG("[HDR2]");
     
    // Header 2
    mark (HDR2_MARK);
    space(HDR2_SPACE);
    
    // Send Data of section 2
    sendSection(*p_sec2, sec2_nbits);

    DEBUG("[HDR3]");
    // Header 3
    mark (HDR3_MARK1);
    space(HDR3_SPACE1);    
    mark (HDR3_MARK2);
    space(HDR3_SPACE2);
    
    // Send Data of section 3
    sendSection(*p_sec1 ^ (1UL << 29), sec1_nbits); // Invert bit 29
    
    DEBUG("[HDR4]");
    // Header 4
    mark (HDR4_MARK);
    space(HDR4_SPACE);    
    
    // Send Data of section 4
    sendSection(0, SEC4_NBITS);
    
    DEBUGln();
    space(0);  // Always end with the LED off
}
#endif

//+=============================================================================
//

