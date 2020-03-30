/*
 * Mux_initial_test3.ino
 * Luke Miller 2020
 * 
 * Testing with MusselBedHeater RevC hardware
 * Tested working for thermistor channel 1,2
 * 
 * Analog input on pin A0 (PC0)
 * ADG725 MUX SYNC pin connected to D7 (PD7)
 */

#include <SPI.h>
#include "MusselBedHeaterlib.h"

#define CS_MUX 7 // Arduino pin D7 connected to ADG725 SYNC pin
#define CS_SD 10 // Pin used for SD card
#define THERM A0 // Arduino analog input pin A0, reading thermistors from ADG725 mux
#define SPI_SPEED 4000000 // Desired SPI speed for talking to ADG725

ADG725 mux; // create ADG725 multiplexer object
uint8_t ADGchannel = 0x00; // initial value to activate ADG725 channel S1

void setup() {
  Serial.begin(57600);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(CS_MUX, OUTPUT); // Set CS_MUX pin as output for ADG725 SYNC
  pinMode(THERM, INPUT); // Analog input from ADG725
  
  SPI.begin();
  mux.begin(CS_MUX, SPI_SPEED); // Set up multiplexer
  mux.setADG725channel(ADGchannel); // Activate channel stored in ADGchannel variable
}

void loop() {
  delay(100);
  unsigned int thermRaw = analogRead(THERM);
  Serial.println(thermRaw);

}
