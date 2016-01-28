/*
 * IRrecord: record and play back IR signals as a minimal 
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * An IR LED must be connected to the output PWM pin 3.
 * A button must be connected to the input BUTTON_PIN; this is the
 * send button.
 * A visible LED can be connected to STATUS_PIN to provide status.
 *
 * The logic is:
 * If the button is pressed, send the IR code.
 * If an IR code is received, record it.
 *
 * Version 0.11 September, 2009
 * Copyright 2009 Ken Shirriff
 * http://arcfn.com
 */

#include <IRremote.h>
#include <avr/wdt.h>       //watchdog library
#include <EEPROM.h>
#include "EEPROMAnything.h"

#define RAW_CODE_SIZE 250
#define IR_SUB_CMD_COUNT 1

int RECV_PIN = 4;
int BUTTON_PIN = 7;
int STATUS_PIN = 9;

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;
// Storage for the recorded code
int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
unsigned int rawCodes[IR_SUB_CMD_COUNT][RAW_CODE_SIZE]; // The durations if raw
int codeLen[IR_SUB_CMD_COUNT]; // The length of the code
int toggle = 0; // The RC5/6 toggle state
bool bAllowReceiceIr = false;
char menu = 0;
byte charsRead = 0;
boolean setup_done;



void setup()
{
  Serial.begin(115200);
  EEPROM_readAnything(sizeof(codeLen), rawCodes);
  EEPROM_readAnything(0, codeLen);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(BUTTON_PIN, INPUT); digitalWrite(BUTTON_PIN, HIGH); //activate pullup
  pinMode(STATUS_PIN, OUTPUT);
}



void handleMenuInput(char c)
{
  switch(menu)
  {
    case 0:
      switch(c)
      { 
        case 'r': Serial.print("\r\nRecord IR code: "); bAllowReceiceIr = true; delay(1); break;
//         case 's': Serial.print("\r\IR code saved to EEPROM!"); EEPROM_writeAnything(0, CONFIG); break;//EEPROM.writeBlock(0, CONFIG); break;
//         case 'E': Serial.print("\r\nErasing EEPROM ... "); menu=c; break;
//         case 'r': Serial.print("\r\nRebooting"); resetUsingWatchdog(1); break;
        case  27:  Serial.print("\r\nExiting the menu...\r\n");menu=0;setup_done=true;break;
      }
      break;
     
  }
}


// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results, unsigned int *_rawCodes, int * _pcodeLen) {
  codeType = results->decode_type;
  int count = results->rawlen;
  if (codeType == UNKNOWN) {
    Serial.println("Received unknown code, saving as raw");
    *_pcodeLen = results->rawlen - 1;
    // To store raw codes:
    // Drop first value (gap)
    // Convert from ticks to microseconds
    // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
    for (int i = 1; i <= *_pcodeLen; i++) {
      if (i % 2) {
        // Mark
        _rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK - MARK_EXCESS;
        if (_rawCodes[i - 1] > 500 && _rawCodes[i - 1] < 700) 
            _rawCodes[i - 1] = 650;
        else if (_rawCodes[i - 1] > 1500 && _rawCodes[i - 1] < 1800)
            _rawCodes[i - 1] = 1700;
        else
        {
            Serial.print("[m");
            Serial.print(_rawCodes[i - 1], DEC);
            Serial.print("]");
        }
      } 
      else {
        // Space
        _rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK + MARK_EXCESS;
        if (_rawCodes[i - 1] >= 500 && _rawCodes[i - 1] <= 700) {
            _rawCodes[i - 1] = 650;
             Serial.print("0");
        }
        else if (_rawCodes[i - 1] >= 1500 && _rawCodes[i - 1] <= 1800) {
            _rawCodes[i - 1] = 1700;        
            Serial.print("1");
        }
        else
        {
            Serial.print("[s");
            Serial.print(_rawCodes[i - 1], DEC);
            Serial.print("]");
        }
      }
//       Serial.print(_rawCodes[i - 1], DEC);
    }
    Serial.println("");
  }
  else {
    if (codeType == NEC) {
      Serial.print("Received NEC: ");
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        Serial.println("repeat; ignoring.");
        return;
      }
    } 
    else if (codeType == SONY) {
      Serial.print("Received SONY: ");
    } 
    else if (codeType == RC5) {
      Serial.print("Received RC5: ");
    } 
    else if (codeType == RC6) {
      Serial.print("Received RC6: ");
    } 
    else {
      Serial.print("Unexpected codeType ");
      Serial.print(codeType, DEC);
      Serial.println("");
    }
    Serial.println(results->value, HEX);
    codeValue = results->value;
    *_pcodeLen = results->bits;
  }
}



void sendCode() {
    int low = 650;
    int high = 1700;
    for ( int j = 0 ; j < 16 ; j++) {
//         for ( int i = 0 ; i < codeLen[0] ; i++) {
//             if (i % 2) {
//                 if (rawCodes[0][i] >= 500 && rawCodes[0][i] <= 700) {
//                     Serial.print("0");
//                 }
//                 else if (rawCodes[0][i] >= 1500 && rawCodes[0][i] <= 1800) {
//                     Serial.print("1");
//                 }
//                 else
//                 {
//                     Serial.print("[s");
//                     Serial.print(rawCodes[0][i], DEC);
//                     Serial.print("]");
//                 }                
//             }
// //              Serial.print(rawCodes[0][i], DEC); Serial.print(",");
//         }
        Serial.println();
        
        for ( int i = 0 ; i < IR_SUB_CMD_COUNT ; i++) {
            irsend.sendRaw(rawCodes[i], codeLen[i], 38);
    //         delay(1);
        }
        rawCodes[0][131] = (j & 0x01) ? high : low ;
        rawCodes[0][133] = (j & 0x02) ? high : low ;
        rawCodes[0][135] = (j & 0x04) ? high : low ;
        rawCodes[0][137] = (j & 0x08) ? high : low ;
    }
}

#if 0
void sendCode(int repeat) {
  if (codeType == NEC) {
    if (repeat) {
//       irsend.sendNEC(REPEAT, codeLen);
      Serial.println("Sent NEC repeat");
    } 
    else {
//       irsend.sendNEC(codeValue, codeLen);
      Serial.print("Sent NEC ");
      Serial.println(codeValue, HEX);
    }
  } 
  else if (codeType == SONY) {
//     irsend.sendSony(codeValue, codeLen);
    Serial.print("Sent Sony ");
    Serial.println(codeValue, HEX);
  } 
  else if (codeType == SAMSUNG) {
  }
  else if (codeType == RC5 || codeType == RC6) {
    if (!repeat) {
      // Flip the toggle bit for a new button press
      toggle = 1 - toggle;
    }
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5) {
      Serial.print("Sent RC5 ");
      Serial.println(codeValue, HEX);
      irsend.sendRC5(codeValue, codeLen);
    } 
    else {
      irsend.sendRC6(codeValue, codeLen);
      Serial.print("Sent RC6 ");
      Serial.println(codeValue, HEX);
    }
  } 
  else if (codeType == UNKNOWN /* i.e. raw */) {
    // Assume 38 KHz
     irsend.sendRaw(rawCodes, codeLen, 38);
//     irsend.sendRaw(rawCodes, codeLen, 40);//34//56      
    Serial.println("Sent raw");
  }
}
#endif

int lastButtonState;

void loop() {
  // If button pressed, send the code.
  int buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == LOW  && buttonState == HIGH) {
    Serial.println("Released");
    irrecv.enableIRIn(); // Re-enable receiver
  }

  if(Serial.available()) {
      handleMenuInput(Serial.read());
  }
  else if (!buttonState) {
    Serial.println("Pressed, sending");
    digitalWrite(STATUS_PIN, HIGH);
//     sendCode(lastButtonState == buttonState);
    sendCode();
    digitalWrite(STATUS_PIN, LOW);
    delay(50); // Wait a bit between retransmissions
  } 
  else if (irrecv.decode(&results) && bAllowReceiceIr) {
    bAllowReceiceIr = false;
    digitalWrite(STATUS_PIN, HIGH);
    int t = (650 - MARK_EXCESS)/ USECPERTICK;
    for ( int i = 0 ; i < IR_SUB_CMD_COUNT ; i++) {
        storeCode(&results, rawCodes[i], &codeLen[i]);
        
//         results.rawbuf[130] = t;
//         results.rawbuf[132] = t;
//         results.rawbuf[134] = t;
//         results.rawbuf[136] = t;
//         results.rawbuf[138] = t;
//         Serial.println("Apply patch");
//         storeCode(&results, rawCodes[i], &codeLen[i]);
        Serial.print("Code length:");Serial.println(codeLen[i]);
        Serial.print("Hash: "); Serial.println(results.value, HEX);
        irrecv.resume(); // resume receiver
//         while(!irrecv.decode(&results));
    }
    
//     while(!irrecv.decode(&results));
//     storeCode(&results, rawCodes[1], &codeLen[1]);
//     Serial.print("Code length:");Serial.println(codeLen[1]);
//     irrecv.resume(); // resume receiver    
    
    digitalWrite(STATUS_PIN, LOW);
    
    EEPROM_writeAnything(sizeof(codeLen), rawCodes);
    EEPROM_writeAnything(0, codeLen);
  }
  lastButtonState = buttonState;
}

