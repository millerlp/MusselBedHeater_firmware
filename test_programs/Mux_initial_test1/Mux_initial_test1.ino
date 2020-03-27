/*
 * Mux_initial_test1.ino
 * Luke Miller 2020
 * 
 * Testing with MusselBedHeater RevC hardware
 * Tested working for thermistor channel 1,2
 * 
 * Analog input on pin A0 (PC0)
 * ADG725 MUX SYNC pin connected to D7 (PD7)
 */

#include <SPI.h>

#define CS_MUX 7 // Arduino pin D7 connected to ADG725 SYNC pin
#define CS_SD 10 // Pin used for SD card
#define THERM A0 // Arduino analog input pin A0, reading thermistors from ADG725 mux

uint8_t ADGchannel = 0x00; // base command for setting active ADG725 channel

void setup() {
  Serial.begin(57600);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(CS_MUX, OUTPUT);
  pinMode(THERM, INPUT); // Analog input from ADG725
  
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000,MSBFIRST, SPI_MODE1));
  // Set ADG725 channel S1 as active by sending 0x00
  digitalWrite(CS_MUX, LOW); // activate ADG726 SYNC line by pulling low
  SPI.transfer(ADGchannel); // Send 8-bit byte to set addresses and EN line
  digitalWrite(CS_MUX, HIGH); // pull high to stop sending data to ADG725
  SPI.endTransaction();
}

void loop() {
  delay(100);
  unsigned int thermRaw = analogRead(THERM);
  Serial.println(thermRaw);

}
