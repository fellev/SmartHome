#include <IRremoteInt.h>
#include <IRremote.h>

int RECV_PIN = 4;
int ONBOARD_LED_PIN = 9;

IRrecv irrecv(RECV_PIN, ONBOARD_LED_PIN);
decode_results results;

void setup() {
  Serial.begin(115200);
  irrecv.enableIRIn(); // Start the receiver
}

void loop() {
  if (irrecv.decode(&results)) {
    Serial.println(results.value, HEX);
    irrecv.resume(); // Receive the next value
  }
}
