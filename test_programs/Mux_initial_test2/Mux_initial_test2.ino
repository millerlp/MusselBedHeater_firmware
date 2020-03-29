/*
 * Mux_initial_test2.ino
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
#define SPI_SPEED 4000000 // Desired SPI speed for talking to ADG725

uint8_t ADGchannel = 0x00; // initial value to activate ADG725 channel S1

void setup() {
  Serial.begin(57600);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(CS_MUX, OUTPUT); // Set CS_MUX pin as output for ADG725 SYNC
  pinMode(THERM, INPUT); // Analog input from ADG725
  
  SPI.begin();
  setADG725channel(ADGchannel); // Activate channel stored in ADGchannel variable
}

void loop() {
  delay(100);
  unsigned int thermRaw = analogRead(THERM);
  Serial.println(thermRaw);

}

//************ Function to set input/output channel pair on ADG725 multiplexer
void setADG725channel(uint8_t ADGchannel) {
#ifndef CS_MUX
#define CS_MUX 7 // Appropriate for MusselBedHeater RevC hardware
#endif   

#ifndef SPI_SPEED
#define SPI_SPEED 4000000 // 4 MHz
#endif
  // Reset the SPI bus settings and mode
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
  // Activate the ADG725 SYNC line (CS_MUX)
  digitalWrite(CS_MUX, LOW); // activate ADG725 SYNC line by pulling low
  // Transfer the new channel address for the ADG725 as an 8-bit value
  SPI.transfer(ADGchannel); // Send 8-bit byte to set addresses and EN line
  // Deactivate the ADG725 SYNC line
  digitalWrite(CS_MUX, HIGH); // pull high to stop sending data to ADG725
  // End the transaction, release the SPI bus
  SPI.endTransaction();
}
//*************************************************************************
