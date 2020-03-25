/*
 * MAX31820_initial_test2.ino
 * 
 * Tested working on MusselBedHeater RevC hardware
 * 
 */

#include <OneWire.h>  // For MAX31820 temperature sensor https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // For MAX31820 sensors https://github.com/milesburton/Arduino-Temperature-Control-Library

#define ONE_WIRE_BUS 8  // Data pin for MAX31820s is D8 on MusselBedHeater RevC
#define TEMPERATURE_PRECISION 11 // 11-bit = 0.125C resolution
// Shut off alarm functions on MAX31820 sensors in the DallasTemperature library
#define REQUIRESALARMS false 

// Create a OneWire object called max31820 that we will use to communicate
OneWire max31820(ONE_WIRE_BUS); 
// Pass our OneWire reference to DallasTemperature library object.
DallasTemperature refSensors(&max31820);

byte numRefSensors; // store number of available MAX31820 temperature sensors
// Array to store MAX31820 addresses, sized for max possible number of sensors, 
// each 8 bytes long
byte sensorAddr[4][8]; // Up to 4 MAX31820 sensors on MusselBedHeater RevC boards
// 8 byte array to hold a single MAX31820 address
byte addr[8];  // oneWire address array (8 bytes long)

void setup() {

  Serial.begin(57600);

  // Start up the DallasTemperature library object
  refSensors.begin();

  // Locate MAX31820 devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  numRefSensors = refSensors.getDeviceCount();
  Serial.print(numRefSensors, DEC);
  Serial.println(" devices.");
  // Go through all MAX31820 devices on the bus and 
  // store their addresses
  max31820.reset_search();
  for (int i = 0; i < numRefSensors; i++){
    max31820.search(addr);
    // Copy address values to sensorAddr array
    for (int j = 0; j < 8; j++){
      sensorAddr[i][j] = addr[j];
    }
  }

  // Cycle through the available OneWire devices, print out the 
  // address of each device, set its resolution, and read back 
  // the set temperature resolution
  max31820.reset_search();
  while ( max31820.search(addr)) {
    // Display current device address
    Serial.print("ROM =");
    for(byte i = 0; i < 8; i++) {
        Serial.write(' ');
        Serial.print(addr[i], HEX);
    }
    // Set device resolution
    refSensors.setResolution(addr, TEMPERATURE_PRECISION);
    Serial.print("\tDevice Resolution: ");
    Serial.print(refSensors.getResolution(addr), DEC);
    Serial.println();
  } 
  max31820.reset_search();
  delay(250);
  
  // Tell the DallasTemperature library to not wait for the 
  // temperature reading to complete after telling devices
  // to take a new temperature reading (so we can do other things
  // while the temperature reading is being taken by the devices).
  // You will have to arrange your code so that an appropriate 
  // amount of time passes before you try to use getTempC() after
  // requestTemperatures() is used
  refSensors.setWaitForConversion(false);
}

void loop() {
  // Array to hold temperature results
  float myTemps[numRefSensors-1] = {};
  // Tell all MAX31820s to begin taking a temperature reading
  refSensors.requestTemperatures();
  // You must wait a certain amount of time (specified in MAX31820
  // data sheet) after issuing the requestTemperatures() function. 
  // Ideally in a real program you'd be using some other timing method
  // in the loop to determine when you've waited long enough to query 
  // the sensors. In this case we'll just wait using the delay() 
  // function, which is simple, but doesn't let you do other useful things
  delay(400); // wait >375 milliseconds for 11-bit setting, >750ms for 12-bit

  // Cycle through all MAX31820s and show their temps
  unsigned long tic = micros(); // start timing the query loop
  for (int i = 0; i < numRefSensors; i++){
    //********** Faster version, using known addresses ********
    // Extract current device's 8-byte address from sensorAddr array
    for (int n = 0; n < 8; n++){
      addr[n] = sensorAddr[i][n];
    }
    myTemps[i] = refSensors.getTempC(addr); // query using device address
    //************************************************************
    // Slower method. Comment out the lines above to use this version. 
//    myTemps[i] = refSensors.getTempCByIndex(i); // query using device index (slower)
  }
  unsigned long tock = micros(); // finish timing the query loop
  for (int i = 0; i < numRefSensors; i++){
    Serial.print(myTemps[i],3); // print with 3 digit precision
    Serial.print("C ");
  }
  Serial.print(" Elapsed micros: ");
  Serial.print(tock - tic); 
  // The query time using each known addresses via getTempC(addr) is about 25,300
  // microseconds to query 2 sensors.  
  // If you instead use getTempCByIndex(), the total query time is approx 69,560 micros 
  // for reading 2 sensors
  Serial.println();
  

}
