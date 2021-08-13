#include <Arduino.h>
#include "main.h"
/*
 * Semi-Bruteforce-Key-Finder. Reads out any key from an Attiny.
 */

//Clock pin
#define CLK 2
//Data Pin
#define DATA 3
//Analog input pin
#define A_INPUT A0
//Length of key
#define MESSAGE_LENGTH 9
//Cycle of CLK
#define PULSE_WIDTH 300

//Analog threshold that defines if the tiny is sleeping or not. Value is 150 and 630 with tiny@9.6Mhz and 150 Ohm
//This might different a bit -- so test it out!
#define CURRENT_THRESHOLD 150

// The key to be tested
uint8_t key[MESSAGE_LENGTH];

void setup() {
  //Setup pins
  pinMode(CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  //Pullup for analog input
  pinMode(A_INPUT, INPUT_PULLUP);
  //Set reference voltage to 1.1V, better resolution. With an Attiny13@9.6Mhz and 22Ohm we measure 100mv in Active Mode
  analogReference(INTERNAL);
  //Read out one value, as first value is always garbage
  analogRead(A_INPUT);
  //Start serial
  Serial.begin(115200);
  delay(200);
}

void loop() {
  // Initialize the key to all 0s
  for(short i = 0; i < MESSAGE_LENGTH; i++){
    key[i] = 0x00;
  }

  bool keyFound = false;
  do {
    // test key combinations and check at which character the chip went to sleep
    int untilChar = testKey(key);

    //Serial.print("Returned at ");
    //Serial.println(untilChar);

    if (untilChar < MESSAGE_LENGTH) {
      // went to sleep before we could finish - didn't accept it
      // let's wait a bit until ATtiny gave up reading
      delay(250);

      // TODO: decide what to do next
      key[untilChar]++;
    } else {
      // ATtiny seems to have processed all our key bytes
      keyFound = true;
    }
  } while (!keyFound);

  Serial.println("Found Key:");
  printKey(key);
}

int testKey(uint8_t key[]){
    // set the CLK to HIGH manually (even though shiftOutByte does so), because
    // the ATtiny takes a while to wake up from sleep
    digitalWrite(CLK,HIGH);
    //Wait 15ms to ensure attiny did wake up
    delay(20);

    // take a reading of the resistor voltage (i.e. current of the ATtiny)
    int powerWhenAwake = analogRead(A_INPUT);
    //Serial.print("awake: ");
    //Serial.println(powerWhenAwake);

    //Send out every byte
    short cameUntilByte = 0;
    for(cameUntilByte = 0; cameUntilByte < MESSAGE_LENGTH; cameUntilByte++){
      //Serial.print("Trying ");
      //Serial.print(key[cameUntilByte]);
      //Serial.print(" at index ");
      //Serial.print(cameUntilByte);

      shiftOutByte(key[cameUntilByte]);
      // the ATtiny might take a while to process our sent byte
      delay(3);

      // take another current reading of this byte
      int powerAfter = analogRead(A_INPUT);
      //Serial.print("after: ");
      //Serial.println(powerAfter);

      if (powerAfter < powerWhenAwake - CURRENT_THRESHOLD) {
        // TODO: decide how to react if power was lower (probably sleep mode of ATtiny)
        //Serial.println(" - Rejected");
        Serial.println();
        break;
      }
      else {
        //Serial.println(" - Accepted");
        Serial.print(".");
      }
    }

/*
    digitalWrite(CLK,LOW);
    digitalWrite(DATA,LOW);
*/
  return cameUntilByte;
}

/*
 * Print out a key to the serial buffer
 * key: the key to print
 */
void printKey(uint8_t key[])
{
  for(short i = 0; i < MESSAGE_LENGTH;i++){
    Serial.print(" 0x");
    Serial.print(key[i],HEX);
  }
  Serial.print ("  --  ");
  for(short i = 0; i < MESSAGE_LENGTH;i++){
    Serial.print((char)key[i]);
  }
}

/*
 * Send a byte. Uses CLK as a clock line and DATA as data line
 * bt: Byte that is to be send
 * pulsewidth: Duty cycle of CLK line in microseconds
 */
void shiftOutByte(uint8_t bt)
{
  uint8_t bits = 0;
  while(bits!=8){
    digitalWrite(CLK,HIGH);
    digitalWrite(DATA,bt&1);
    delayMicroseconds(PULSE_WIDTH);
    digitalWrite(CLK,LOW);
    delayMicroseconds(PULSE_WIDTH);
    bits++;
    bt >>= 1;
  }
}