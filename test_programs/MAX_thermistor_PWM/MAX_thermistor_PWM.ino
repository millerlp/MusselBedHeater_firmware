/*
 * Basic working example, initializing multiple MAX31820s
 * and reading them back using MusselBedHeaterlib library,
 * along with reading thermistors and doing multiple PID calculations 
 * 
 * TODO: Set up multiple PID objects to handle 16 thermistor channels
 * TODO: Send PID outputs to PWM objects to turn on heater
 * TODO: Tune PID
 * TODO: Implement SD card file creation, data saving
 * TODO: Implement real time clock
 * TODO: Implement battery monitoring
 * TODO: Implement tide prediction routine
 * TODO: Set up state machine to only run heaters at low tide
 */

#include <OneWire.h>  // For MAX31820 temperature sensor https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // For MAX31820 sensors https://github.com/milesburton/Arduino-Temperature-Control-Library
#include "NTC_Thermistor.h" // https://github.com/YuriiSalimov/NTC_Thermistor
#include "PID_v1.h" // https://github.com/br3ttb/Arduino-PID-Library/
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h> // https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib

double tempIncrease = 1.0; // Celsius, target temperature increase for heated 
                           // mussels relative to reference mussels

//----------------------------------------------
// Multiplexer macros for use in reading thermistors in heated mussels
#define CS_MUX 7 // Arduino pin D7 connected to ADG725 SYNC pin
#define CS_SD 10 // Pin used for SD card
#define THERM A0 // Arduino analog input pin A0, reading thermistors from ADG725 mux
#define SPI_SPEED 4000000 // Desired SPI speed for talking to ADG725
// Create Thermistor object
Thermistor* thermistor1;
// Create multiplexer object
ADG725 mux; 
uint8_t ADGchannel = 0x00; // Initial value to activate ADG725 channel S1
#define NUM_THERMS 2 // Number of thermistor channels

//--------------------------------------------------
// NTC_Thermistor library constants - specific to my chosen thermistor model
#define REFERENCE_RESISTANCE  4700
#define NOMINAL_RESISTANCE    6800
#define NOMINAL_TEMPERATURE   25
#define B_VALUE               4250
//----------------------------------------------------------
// OneWire MAX31820 temperature sensor macros and variables
// These are for the reference (unheated) mussels
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
int MAXSampleTime = 500; // units milliseconds, time between MAX31820 readings
unsigned long prevMaxTime; 
double avgMAXtemp = 0; // Average of MAX31820 sensors
//---------------------------------------
// PID variables + timing
//unsigned long lastTime; // units milliseconds
//double Input1, Output1, Setpoint1;
//double Input2, Output2, Setpoint2;

double Input[16] = {}; // 16 position array of Input values for PID, this
                       // also serves as the current thermistor temperature
                       // readings array if you want to write these to disk
double Output[16] = {}; // 16 position array of Output values for PID (0-4095)
double Setpoint; // All heated mussels use the same target Setpoint temperature

int SampleTime = 1000; // units milliseconds, time between PID updates 
// Specify initial tuning parameters
double Kp = 2, Ki = 5, Kd = 1;
PID myPID1(&Input[0], &Output[0], &Setpoint, Kp, Ki, Kd, DIRECT);
PID myPID2(&Input[1], &Output[1], &Setpoint, Kp, Ki, Kd, DIRECT);
//-------------------------------------------------------------
// PCA9685 pulse width modulation driver chip
// Called this way, uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
byte pwmnum = 0; // Channel on PWM driver (0-15)

void setup() {
  Serial.begin(57600);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(CS_MUX, OUTPUT); // Set CS_MUX pin as output for ADG725 SYNC
  pinMode(THERM, INPUT); // Analog input from ADG725
  //--------- Thermistor setup ------------------------------------------
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
  //---------- MAX31820 setup ---------------------------------
  // Start up the DallasTemperature library object
  refSensors.begin();
  numRefSensors = refSensors.getDeviceCount();
  // Call function from MusselBedHeaterlib library
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
  // Set library to not wait for a conversion to complete after requesting temperatures
  // (Allows the program to do other things while MAX31820s are taking ~400ms to 
  // generate new temperature values)
  refSensors.setWaitForConversion(false);
  // Tell all MAX31820s to begin taking the first temperature readings
  refSensors.requestTemperatures();
  //------------ PWM chip initialization ----------------------------------
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency
  // Set all PWM channels off initially
  for (byte i = 0; i < 16; i++){
    pwm.setPWM(i, 0, 0); // Channel, on, off (relative to start of 4096-part cycle)  
  }
  
  //--------- PID start -------------------------------------
  myPID1.SetMode(AUTOMATIC);
  myPID1.SetOutputLimits(0,4095); // PWM chip takes a value from 0 to 4095
  myPID1.SetSampleTime(SampleTime); // Tell PID routine how long to wait between updates
  myPID2.SetMode(AUTOMATIC);
  myPID2.SetOutputLimits(0,4095); // PWM chip takes a value from 0 to 4095
  myPID2.SetSampleTime(SampleTime); // Tell PID routine how long to wait between updates
  
  // Initialize the timing for sampling MAX31820s
  prevMaxTime = millis();

}         // end of setup loop

//----------------------------------------------------------------
//----- Main loop ------------------------------------------------
void loop() {
    // Array to hold temperature results
    double MAXTemps[numRefSensors] = {}; // MAX31820 temperature array
    byte goodReadings = 0;              // Number of good MAX31820 readings in a cycle
    
    unsigned long newMillis = millis();
    
    // Check if enough time has elapsed to check MAX31820s
    if ( (newMillis - prevMaxTime) >= MAXSampleTime){
      prevMaxTime = newMillis; // update prevMaxTime
      avgMAXtemp = 0; // reset average value
      //------- MAX31820 readings -----------

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
      for (byte i = 0; i < NUM_THERMS; i++){
        mux.setADG725channel(ADGchannel | i); // Activate channel i 
        // Take a reading from thermistor1
        Input[i] = thermistor1 -> readCelsius();
      }
      // Turn off all multiplexer channels (shuts off thermistor circuits)
      mux.disableADG725(); 

//      Serial.print(F("Ref1: "));
//      Serial.print(MAXTemps[0],2);
//      Serial.print(F("\t Ref2: "));
//      Serial.print(MAXTemps[1],2);
      Serial.print(F(" Avg ref temp: "));
      Serial.print(avgMAXtemp,2);
//      Serial.print(F("\t Therm1: "));
//      Serial.print(thermTemps[0]);
//      Serial.print(F("\t Therm2: "));
//      Serial.print(thermTemps[1]);
      Serial.print(F(" Input1: "));
      Serial.print(Input[0]);
      Serial.print(F("\t Setpoint: "));
      Serial.print(Setpoint);
      Serial.print(F("\t Output1: "));
      Serial.print((uint16_t)Output[0]);

      Serial.print(F("\t Input2: "));
      Serial.print(Input[1]);
      Serial.print(F("\t Setpoint: "));
      Serial.print(Setpoint);
      Serial.print(F("\t Output2: "));
      Serial.print((uint16_t)Output[1]);

      Serial.println();
    }

    //------PID update-----------------
    // Update the target temperature Setpoint for the heated mussels
    Setpoint = avgMAXtemp + tempIncrease;
    // The PID Compute() function updates whenever SampleTime has
    // been exceeded (checked inside the Compute() function).
    myPID1.Compute(); // Will update when SampleTime is exceeded
    myPID2.Compute(); // Will update when SampleTime is exceeded
    // Update PWM output values
    // Send the channel, start value (0), and end value (0-4095)
    pwm.setPWM(0, 0, (uint16_t)Output[0]);
    pwm.setPWM(1, 0, (uint16_t)Output[1]);
    
    //---------------------------------
} // end of main loop
