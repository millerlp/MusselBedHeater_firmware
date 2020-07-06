/*
 * MAX31820_sensor_check.ino
 * 
 * Use this to verify visually with the onboard RGB LED
 * that a MAX31820 temperature sensor was detected. If sensors
 * are found, the green LED will flash the same number of times
 * as the number of sensors found. If no MAX31820s are found, 
 * the LED will flash red once. In order to register as a "found"
 * sensor, the temperature that is read must be within sensible
 * bounds (-10C to +60C). 
 * 
 */

#include <OneWire.h>  // For MAX31820 temperature sensor
#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib

//--------- RGB LED setup --------------------------
// Create object for red green blue LED
RGBLED rgb;

//----------------------------------------------------------
// OneWire MAX31820 temperature sensor macros and variables
// These are for the reference (unheated) mussels
#define ONE_WIRE_BUS 8  // Data pin for MAX31820s is D8 on MusselBedHeater RevC
#define TEMPERATURE_PRECISION 11 // 11-bit = 0.125C resolution
// Create a OneWire object called max31820 that we will use to communicate
OneWire max31820(ONE_WIRE_BUS); 
// Pass our OneWire reference to DallasTemperature library object.
DallasTemperature refSensors(&max31820);
uint8_t numRefSensors; // store number of available MAX31820 temperature sensors
// Array to store MAX31820 addresses, sized for max possible number of sensors, 
// each 8 bytes long
uint8_t sensorAddr[4][8] = {}; // Up to 4 MAX31820 sensors on MusselBedHeater RevC boards
uint8_t addr[8] = {};  // Address array for MAX31820, 8 bytes long
int MAXSampleTime = 1000; // units milliseconds, time between MAX31820 readings
byte goodReadings = 0; // Keep count of good readings in each loop


void setup() {
  
  Serial.begin(57600);
  rgb.begin(); // Setup RGB with default pins (9,6,5)
  //---------- MAX31820 setup ---------------------------------
  // Start up the DallasTemperature library object
  refSensors.begin();
  numRefSensors = refSensors.getDeviceCount();
}

void loop() {
  // put your main code here, to run repeatedly:
  double MAXTemps[numRefSensors] = {}; // MAX31820 temperature array
  goodReadings = 0;              // Number of good MAX31820 readings in a cycle
  numRefSensors = refSensors.getDeviceCount();
    // Call function from MusselBedHeaterlib library
  getRefSensorAddresses(max31820, refSensors, numRefSensors, sensorAddr); 
  Serial.print(F("Number of MAX31820 sensors detected: "));
  Serial.println(numRefSensors);

  // Set library to not wait for a conversion to complete after requesting temperatures
  // (Allows the program to do other things while MAX31820s are taking ~400ms to 
  // generate new temperature values)
  refSensors.setWaitForConversion(false);
  // Tell all MAX31820s to begin taking the first temperature readings
  refSensors.requestTemperatures();
  delay(750);
  // Cycle through all MAX31820s and read their temps
  for (byte i = 0; i < numRefSensors; i++){
    //********** Faster version, using known addresses ********
    // Extract current device's 8-byte address from sensorAddr array
    for (int n = 0; n < 8; n++){
      addr[n] = sensorAddr[i][n];
    }
    MAXTemps[i] = refSensors.getTempC(addr); // query using device address
    // Sanity check the recorded temperature
    if ( (MAXTemps[i] > -10.0) & (MAXTemps[i] < 60.0) ){
      goodReadings = goodReadings + 1;   
    }   
  }
  Serial.println(F("Addresses:"));
  for (byte i = 0; i < numRefSensors; i++){
    for (byte j = 0; j < 8; j++){
        Serial.write(' ');
        Serial.print(sensorAddr[i][j], HEX);
    }
    Serial.print(F("\t TempC: "));
    Serial.print(MAXTemps[i],3);
    Serial.println();
  }
  // Flash LED to indicate how many good readings were received
  if (goodReadings > 0){
    for (byte i = 0; i < goodReadings; i++){
      // Flash green for number of good sensors detected
      rgb.setColor(0,127,0); // green
      delay(150);
      rgb.setColor(0,0,0);  // off
      delay(150);
    }
  } else if (goodReadings == 0){
    // Flash red once if no sensors detected
      rgb.setColor(127,0,0);  // red
      delay(150);
      rgb.setColor(0,0,0);  // off
      delay(150);
  }
  

}
