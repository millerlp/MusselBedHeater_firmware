/*
 * Thermistor_calibration.ino
 * Luke Miller 2020
 * 
 * Use this sketch to display temperature data for MAX31820 sensors
 * and up to 16 thermistors (the MAX31820s can serve as references).
 * 
 * Testing with MusselBedHeater RevC hardware

 * 
 * Analog input on pin A0 (PC0) for thermistor voltages
 * ADG725 MUX SYNC pin connected to D7 (PD7)
 */

#include <SPI.h>
#include "NTC_Thermistor.h" // https://github.com/YuriiSalimov/NTC_Thermistor
#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib

#define PLOTTER_OUTPUT 0 // Set 0 for Serial Monitor (text) output, 1 for plotter output

#define CS_MUX 7 // Arduino pin D7 connected to ADG725 SYNC pin
#define CS_SD 10 // Pin used for SD card (not implemented here, but should be defined)
#define THERM A0 // Arduino analog input pin A0, reading thermistors from ADG725 mux
#define SPI_SPEED 4000000 // Desired SPI speed for talking to ADG725
// Thermistor library constants
#define REFERENCE_RESISTANCE  4700  // Specific to MusselBedHeater RevC on-board reference resistors
#define NOMINAL_RESISTANCE    6800  // Specific to MusselBedHeater RevC chosen thermistor
#define NOMINAL_TEMPERATURE   25
#define B_VALUE               4250  // Specific to MusselBedHeater RevC chosen thermistor
// Other thermistor-related constants
#define NUM_THERMISTORS       16  // Number of thermistors to sample
#define THERM_AVG             4   // Number of thermistor readings to average
#define TEMP_FILTER           2.0 // Threshold temperature change limit between readings


Thermistor* thermistor1;
ADG725 mux;
uint8_t ADGchannel = 0x00; // Initial value to activate ADG725 channel S1

double pidInput[16] = {}; // 16 position array of Input values for PID, this
                       // also serves as the current thermistor temperature
                       // readings array if you want to write these to disk

//--------- RGB LED setup --------------------------
// Create object for red green blue LED
RGBLED rgb;
//-------- timing ------------------
unsigned long prevMillis; // units of milliseconds
unsigned int sampleInterval = 500; // time between sampling events, units milliseconds
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
double avgMAXtemp = 0; // Average of MAX31820 sensors


//----------------------------------------------------------------
//------------- Setup loop ---------------------------------------
void setup() {
  Serial.begin(57600);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(CS_MUX, OUTPUT); // Set CS_MUX pin as output for ADG725 SYNC
  pinMode(THERM, INPUT); // Analog input from ADG725
  rgb.begin(); // Setup RGB with default pins (9,6,5)
  for (byte i = 0; i < 5; i++){
    rgb.setColor(0,100,0);
    delay(30);
    rgb.setColor(0,0,0);
    delay(30);    
  }
  // Set up thermistor1 object
  thermistor1 = new NTC_Thermistor(
    THERM,
    REFERENCE_RESISTANCE,
    NOMINAL_RESISTANCE,
    NOMINAL_TEMPERATURE,
    B_VALUE);
  // Initialize multiplexer object
  mux.begin(CS_MUX, SPI_SPEED);
  SPI.begin();
  // Get an initial reading of the thermistor(s)
  for (byte i = 0; i < NUM_THERMISTORS; i++){
    mux.setADG725channel(ADGchannel | i); // Activate channel i 
    // Take an initial reading and throw it away (gives ADC time to settle)
    thermistor1 -> readCelsius();
    delay(1);
    // Take a reading from thermistor1
    pidInput[i] = thermistor1 -> readCelsius();
  }
  // Turn off all multiplexer channels (shuts off thermistor circuits)
  mux.disableADG725(); 

  //---------- MAX31820 setup ---------------------------------
  // Start up the DallasTemperature library object
  refSensors.begin();
  numRefSensors = refSensors.getDeviceCount();
  // Call function from MusselBedHeaterlib library
  getRefSensorAddresses(max31820, refSensors, numRefSensors, sensorAddr); 
  for (byte i = 0; i < numRefSensors; i++){
    for (byte j = 0; j < 8; j++){
    }
  }
  // Set library to not wait for a conversion to complete after requesting temperatures
  // (Allows the program to do other things while MAX31820s are taking ~400ms to 
  // generate new temperature values)
  refSensors.setWaitForConversion(false);
  // Tell all MAX31820s to begin taking the first temperature readings
  refSensors.requestTemperatures();
  delay(500); // Let some time run after requesting temperatures
  
}


//-------------- Main loop ----------------------------
void loop() {

  byte goodReadings = 0;
  double MAXTemps[numRefSensors] = {}; // MAX31820 temperature array
  unsigned long newMillis = millis();
  
  // If we've passed the sample interval time, take new readings
  if ( (newMillis - prevMillis) >= sampleInterval){
      prevMillis = newMillis; // Update prevMillis for next loop
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
          avgMAXtemp = avgMAXtemp + MAXTemps[i];
          goodReadings = goodReadings + 1;   
        }
        
      }
      
      // Start the next temperature reading
      refSensors.requestTemperatures();
      // Calculate average MAX31820 temperature of reference mussels
      avgMAXtemp = avgMAXtemp / (double)goodReadings; 
    
      //------------ Thermistor readings --------------
      // Read the thermistor(s)
      for (byte i = 0; i < NUM_THERMISTORS; i++){
        mux.setADG725channel(ADGchannel | i); // Activate channel i
        // Take an initial reading and throw it away (gives ADC time to settle)
        thermistor1 -> readCelsius();
        delay(2); 
        double tempVal = 0;
        double thermAvg = 0;
        // Take THERM_AVG number of successive readings from this thermistor
        // and average them together, along with checking and removing 
        // spurious temperature spikes
        for (byte j = 0; j < THERM_AVG; j++){
          // Take the temperature reading
          tempVal = thermistor1 -> readCelsius();
          // Check the temperature reading relative to previous step's temperature value
          // I was getting spurious temperature spikes (high and low) in excess of 2-3C every
          // 10-30 seconds, which really threw off the PID routine. This filtering below 
          // just throws out temperatures than change too much in the ~500ms between loops
          if ( abs(tempVal - pidInput[i]) < TEMP_FILTER ){
            // Value is probably good
            thermAvg += tempVal;
            goodReadings = goodReadings + 1;
          }        
          delay(2);
        }
        // Calculate an average temperature from the good readings
        pidInput[i] = thermAvg / (double)goodReadings;
      }
      // Turn off all multiplexer channels (shuts off thermistor circuits)
      mux.disableADG725(); 
      //----------------------------
      // Output block
#if (PLOTTER_OUTPUT == 0) 
    // Code for Serial monitor text output 
    // Print individual MAX31820 sensor values
    for (byte i = 0; i < numRefSensors; i++){
      Serial.print(F("MAX"));
      Serial.print(i);
      Serial.print(F(":"));
      Serial.print(MAXTemps[i],2);
      Serial.print(F("\t"));
    }
    // Print thermistor values
    for (byte i = 0; i < NUM_THERMISTORS; i++){
      Serial.print(F("T"));
      Serial.print(i);
      Serial.print(F(":"));
      Serial.print(pidInput[i],2);
      Serial.print(F("C\t"));  
    }
    Serial.println();
#elif (PLOTTER_OUTPUT == 1) 
    // Code for Serial plotter output
    Serial.print(avgMAXtemp,2); // Plot average MAX31820 temperature
    Serial.print(F("\t"));
    // Plot the individual thermistor temperatures
    for (byte i = 0; i < NUM_THERMISTORS; i++){
      Serial.print(pidInput[i]);
      Serial.print(F("\t"));
    }
    Serial.println();
#endif
      

  } // End of temperature sampling routine



}
