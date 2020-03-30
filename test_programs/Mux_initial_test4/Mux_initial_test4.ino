/*
 * Mux_initial_test4.ino
 * Luke Miller 2020
 * 
 * Testing with MusselBedHeater RevC hardware
 * Tested working for thermistor channel 1,2
 * Incorporating NTC_Thermistor library
 * 
 * Analog input on pin A0 (PC0)
 * ADG725 MUX SYNC pin connected to D7 (PD7)
 */

#include <SPI.h>
#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib
#include "NTC_Thermistor.h"  // https://github.com/YuriiSalimov/NTC_Thermistor
#include "AverageThermistor.h" // https://github.com/YuriiSalimov/NTC_Thermistor

#define CS_MUX 7 // Arduino pin D7 connected to ADG725 SYNC pin
#define CS_SD 10 // Pin used for SD card
#define SPI_SPEED 4000000 // Desired SPI speed for talking to ADG725
// Thermistor settings
#define ANALOG_INPUT  A0 // Arduino analog input pin A0, reading thermistors from ADG725 mux
#define REFERENCE_RESISTANCE   4700   // fixed resistor value 
#define NOMINAL_RESISTANCE     6800  // thermistor value @ 25C
#define NOMINAL_TEMPERATURE    25    // thermistor reference temperature
#define B_VALUE                4250 // from manufacturer's data sheet
#define READINGS_NUMBER        4    // number of readings to average
#define DELAY_TIME             5    // milliseconds delay between multiple temp readings

ADG725 mux; // create ADG725 multiplexer object
uint8_t ADGchannel = 0x00; // initial value to activate ADG725 channel S1

Thermistor* thermistor = NULL; // create Thermistor object

void setup() {
  Serial.begin(57600);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(ANALOG_INPUT, INPUT); // Analog input from ADG725
  
  SPI.begin();
  mux.begin(CS_MUX, SPI_SPEED); // Set up multiplexer
  mux.setADG725channel(ADGchannel); // Activate channel stored in ADGchannel variable
  
  // Set thermistor object to produce an average of several readings
  thermistor = new AverageThermistor(
      new NTC_Thermistor(
      ANALOG_INPUT,
      REFERENCE_RESISTANCE,
      NOMINAL_RESISTANCE,
      NOMINAL_TEMPERATURE,
      B_VALUE
    ),
    READINGS_NUMBER,
    DELAY_TIME
  );
}

void loop() {
  delay(100);
  double thermAvg = thermistor->readCelsius();
  Serial.println(thermAvg);

}
