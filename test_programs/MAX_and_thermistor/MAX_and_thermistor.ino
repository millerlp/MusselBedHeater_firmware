/*
 * Basic working example, initializing multiple MAX31820s
 * and reading them back using MusselBedHeaterlib library
 * 
 * TODO: Implement thermistor reading via multiplexer
 * TODO: Implement PID routine
 * TODO: Implement PWM chip control
 */


#include <OneWire.h>  // For MAX31820 temperature sensor https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // For MAX31820 sensors https://github.com/milesburton/Arduino-Temperature-Control-Library
#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib

#define ONE_WIRE_BUS 8  // Data pin for MAX31820s is D8 on MusselBedHeater RevC
#define TEMPERATURE_PRECISION 11 // 11-bit = 0.125C resolution
// Shut off alarm functions on MAX31820 sensors in the DallasTemperature library
#define REQUIRESALARMS false 
// Create a OneWire object called max31820 that we will use to communicate
OneWire max31820(ONE_WIRE_BUS); 
// Pass our OneWire reference to DallasTemperature library object.
DallasTemperature refSensors(&max31820);

uint8_t numRefSensors; // store number of available MAX31820 temperature sensors
// Array to store MAX31820 addresses, sized for max possible number of sensors, 
// each 8 bytes long
uint8_t sensorAddr[4][8] = {}; // Up to 4 MAX31820 sensors on MusselBedHeater RevC boards
uint8_t addr[8] = {};  // Address array for MAX31820

void setup() {  

  Serial.begin(57600);
  
  // Start up the DallasTemperature library object
  refSensors.begin();
  numRefSensors = refSensors.getDeviceCount();
  getRefSensorAddresses(max31820, refSensors, numRefSensors, sensorAddr);
  Serial.print(F("Number of sensors detected: "));
  Serial.println(numRefSensors);
  Serial.println("Addresses:");
  for (byte i = 0; i < numRefSensors; i++){
    for (byte j = 0; j < 8; j++){
        Serial.write(' ');
        Serial.print(sensorAddr[i][j], HEX);
    }
    Serial.println();
  }

  // Tell all MAX31820s to begin taking a temperature reading
  refSensors.requestTemperatures();
  delay(500); // wait before requesting initial temperatures
  
  // Array to hold temperature results
  float myTemps[numRefSensors-1] = {};

  // Cycle through all MAX31820s and show their temps
  for (int i = 0; i < numRefSensors; i++){
    //********** Faster version, using known addresses ********
    // Extract current device's 8-byte address from sensorAddr array
    for (int n = 0; n < 8; n++){
      addr[n] = sensorAddr[i][n];
    }
    myTemps[i] = refSensors.getTempC(addr); // query using device address
  }
  Serial.print(F("Temps: "));
  for (int i = 0; i < numRefSensors; i++){
    Serial.print(myTemps[i],3); // print with 3 digit precision
    Serial.print("C ");
  }
  Serial.println();

  
}

void loop() {
  // put your main code here, to run repeatedly:

}


//void getRefSensorAddresses(OneWire& max31820, 
//                            DallasTemperature& refSensors, 
//                            uint8_t numRefSensors, 
//                            uint8_t sensorAddr[][8]){
//    uint8_t addr[8]; // OneWire address array, 8 bytes long
//    max31820.reset_search();
//    for (uint8_t i = 0; i < numRefSensors; i++){
//        max31820.search(addr); // read next sensor address into addr
//        // Copy address values to sensorAddr array
//        for (uint8_t j = 0; j < 8; j++){
//            sensorAddr[i][j] = addr[j];
//        }
//        // Set sensor resolution to 11 bits, approx 400ms conversion time
//        refSensors.setResolution(addr, 11);   
//    }
//    max31820.reset_search();
//    
//    // Tell the DallasTemperature library to not wait for the
//    // temperature reading to complete after telling devices
//    // to take a new temperature reading (so we can do other things
//    // while the temperature reading is being taken by the devices).
//    // You will have to arrange your code so that an appropriate
//    // amount of time passes before you try to use getTempC() after
//    // requestTemperatures() is used
//    refSensors.setWaitForConversion(false);
//}
