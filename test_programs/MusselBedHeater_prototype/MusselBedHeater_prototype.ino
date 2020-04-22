/*
 * Basic working example, initializing multiple MAX31820s
 * and reading them back using MusselBedHeaterlib library,
 * along with reading thermistors and doing multiple PID calculations 
 * 
 * A thermistor temperature of -273.15 or thereabouts indicates a 
 * non-functional or unplugged thermistor
 * 
 * TODO: Tune PID
 * TODO: Implement temperature filtering from the tuning version of this sketch
 * TODO: Add the PID PONE define + begin
 */
#include <EEPROM.h> // built in library, for reading the serial number stored in EEPROM
#include "SdFat.h" // https://github.com/greiman/SdFat
#include "OneWire.h"  // For MAX31820 temperature sensor https://github.com/PaulStoffregen/OneWire
#include "DallasTemperature.h" // For MAX31820 sensors https://github.com/milesburton/Arduino-Temperature-Control-Library
#include "NTC_Thermistor.h" // https://github.com/YuriiSalimov/NTC_Thermistor
//#include "PID_v1.h" // https://github.com/br3ttb/Arduino-PID-Library/
#include <Wire.h>
#include "RTClib.h" // https://github.com/millerlp/RTClib
#include "Adafruit_PWMServoDriver.h" // https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib
#include "TidelibNorthSpitHumboldtBayCalifornia.h" // https://github.com/millerlp/Tide_calculator
//-----------------------------------------------------------------------------------
// User-settable values
float tideHeightThreshold = -1.0; // threshold for low vs high tide, units feet (5.9ft = 1.8m)
double tempIncrease = 1.0; // Target temperature increase for heated 
                           // mussels relative to reference mussels. Units = Celsius
#define SAVE_INTERVAL 10 // Number of seconds between saving temperature data to SD card
#define SUNRISE_HOUR 6 // Hour of day when sun rises 
#define SUNSET_HOUR 19 // Hour of day when sun sets
#define NUM_THERMISTORS 16 // Number of thermistor channels
float voltageMin = 11.20; // Minimum battery voltage allowed, shut off below this. Units: volts 
//-----------------------------------------------------------------------------------
#define BUTTON1 2     // BUTTON1 on INT0, pin PD2

//---------------------------------------------------------------------------
// Thermistor multiplexer setup
// Multiplexer macros for use in reading thermistors in heated mussels
#define CS_MUX 7 // Arduino pin D7 connected to ADG725 SYNC pin
#define THERM A0 // Arduino analog input pin A0, reading thermistors from ADG725 mux
#define SPI_SPEED 4000000 // Desired SPI speed for talking to ADG725
// Create Thermistor object
Thermistor* thermistor1;
// Create multiplexer object
ADG725 mux; 
uint8_t ADGchannel = 0x00; // Initial value to activate ADG725 channel S1


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
// Create a OneWire object called max31820 that we will use to communicate
OneWire max31820(ONE_WIRE_BUS); 
// Pass our OneWire reference to DallasTemperature library object.
DallasTemperature refSensors(&max31820);
uint8_t numRefSensors; // store number of available MAX31820 temperature sensors
// Array to store MAX31820 addresses, sized for max possible number of sensors, 
// each 8 bytes long
uint8_t sensorAddr[4][8] = {}; // Up to 4 MAX31820 sensors on MusselBedHeater RevC boards
uint8_t addr[8] = {};  // Address array for MAX31820, 8 bytes long
int MAXSampleTime = 500; // units milliseconds, time between MAX31820 readings
unsigned long prevMaxTime; 
double avgMAXtemp = 0; // Average of MAX31820 sensors
//---------------------------------------
// PID variables + timing

double pidInput[16] = {}; // 16 position array of Input values for PID, this
                       // also serves as the current thermistor temperature
                       // readings array if you want to write these to disk
double pidOutput[16] = {}; // 16 position array of Output values for PID (0-4095)
double pidOutputSum[16] = {}; // Used to store PID ongoing calculations
double pidLastInput[16] = {}; // Used to store PID ongoing calculations
double pidSetpoint; // All heated mussels use the same target Setpoint temperature
int pidSampleTime = 1000; // units milliseconds, time between PID updates 
unsigned long lastTime; // units milliseconds, used for PID timekeeping
// Specify initial tuning parameters
double kp = 2, ki = 5, kd = 1;
PID myPID; // Creates a PID object that will update 16 PID values

//-------------------------------------------------------------
// PCA9685 pulse width modulation driver chip
// Called this way, uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
byte pwmnum = 0; // Channel on PWM driver (0-15)

//-------------------------------------------------------------
// SD card
#define CS_SD 10 // Pin used for SD card
SdFat sd;
SdFile logfile; // This is the file object that will be written to
// Declare initial name for output files written to SD card
char filename[] = "YYYYMMDD_HHMM_00_SN00.csv";
// Placeholder serialNumber
char serialNumber[] = "SN00";
bool sdErrorFlag = false; // Flag to mark initialization problem with SD card
bool saveData = false; // Flag to show SD card is functional
bool writeData = false; // Flag to mark when to write to SD card
bool serialValid = false; // Flag to show whether the serialNumber value is real or just zeros
//-------------------------------------------------------------
// Real Time Clock DS3231M  
// Create real time clock object
RTC_DS3231 rtc;
char buf[20]; // declare a string buffer to hold the time result
//**************** Time variables
DateTime newtime;
DateTime oldtime; // used to track time in main loop
byte oldday;     // used to keep track of midnight transition
bool rtcErrorFlag = false; // Flag to show error with RTC
volatile unsigned long button1Time; // hold the initial button press millis() value
byte debounceTime = 20; // milliseconds to wait for debounce
int mediumPressTime = 2000; // milliseconds to hold button1 to register a medium press
bool flashFlag = false; // Used to flash LED
//--------------------------------------------------------------
// Tide calculator setup
TideCalc myTideCalc; // Create TideCalc object called myTideCalc
float tideHeightft; // Hold results of tide calculation, units feet
bool lowtideLimitFlag = false; // Flag to show that tide is above tideHeightThreshold

//------------------------------------------------------------
//  Battery monitor 
#define BATT_MONITOR A7  // analog input channel to sense battery voltage
#define BATT_MONITOR_EN 4 // digital output channel to turn on battery voltage check
float dividerRatio = 5.7; // Ratio of voltage divider (47k + 10k) / 10k = 5.7
// The refVoltage needs to be measured individually for each board (use a 
// voltmeter and measure at the AREF and GND pins on the board). Enter the 
// voltage value here. 
float refVoltage = 3.22; // Voltage at AREF pin on ATmega microcontroller, measured per board
float batteryVolts = 0; // Estimated battery voltage returned from readBatteryVoltage function
bool lowVoltageFlag = false;
//--------- RGB LED setup --------------------------
// Create object for red green blue LED
RGBLED rgb;

//------- TYPE DEFINITIONS -----------------------------------------
// Types used for the state machine in the main loop
typedef enum STATE
{
  STATE_IDLE, // Idling, heaters off
  STATE_HEATING, // collecting data normally
  STATE_OFF, // Battery voltage is low, wait for user to replace batteries
  STATE_CLOSE_FILE, // close data file, start new file
} mainState_t;

// main state machine variable, this takes on the various
// values defined for the STATE typedef above. 
mainState_t mainState;

typedef enum DEBOUNCE_STATE
{
  DEBOUNCE_STATE_IDLE,
  DEBOUNCE_STATE_CHECK,
  DEBOUNCE_STATE_TIME
} debounceState_t;

// debounce state machine variable, this takes on the various
// values defined for the DEBOUNCE_STATE typedef above.
volatile debounceState_t debounceState;




//-----------------------------------------------------------------------
//------- Setup loop ----------------------------------------------------
void setup() {
  Serial.begin(57600);
  Serial.println(F("Hello"));
  // Set BUTTON1 as an input
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BATT_MONITOR, INPUT);
  pinMode(BATT_MONITOR_EN, OUTPUT);
  pinMode(CS_SD, OUTPUT); // declare as output so that SPI library plays nice
  pinMode(CS_MUX, OUTPUT); // Set CS_MUX pin as output for ADG725 SYNC
  pinMode(THERM, INPUT); // Analog input from ADG725
  rgb.begin(); // Setup RGB with default pins (9,6,5)
  for (byte i = 0; i < 5; i++){
    rgb.setColor(0,255,0);
    delay(30);
    rgb.setColor(0,0,0);
    delay(30);    
  }
  // Grab the serial number from the EEPROM memory
  // This will be in the format "SNxx". The serial number
  // for a board can be permanently set with the separate
  // program 'serial_number_generator.ino' available in 
  // one of the subfolders of the MusselGapeTracker software
  EEPROM.get(0, serialNumber);
  if (serialNumber[0] == 'S') {
    serialValid = true; // set flag   
  }  
  if (serialValid){
    Serial.print(F("Read serial number: "));
    Serial.println(serialNumber);
  } else {
    Serial.println(F("No valid serial number"));
  }

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
  Serial.print(F("Number of MAX31820 sensors detected: "));
  Serial.println(numRefSensors);
  Serial.println(F("Addresses:"));
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


  //------------ Real Time Clock setup ------------------------------------
  // Initialize the real time clock DS3231M
  Wire.begin(); // Start the I2C library with default options
  Wire.setClock(400000L); // Set speed to 400kHz
  rtc.begin();  // Start the rtc object with default options
  printTimeSerial(rtc.now()); // print time to serial monitor
  Serial.println();
  newtime = rtc.now(); // read a time from the real time clock
  //-----------------------
  // Check that real time clock has a reasonable time value
  if ( (newtime.year() < 2020) | (newtime.year() > 2035) ) {
    // There is an error with the clock, halt everything
    while(1){
    // Flash the error led to notify the user
    // This permanently halts execution, no data will be collected
      rgb.setColor(127,0,0);  // red
      delay(150);
      rgb.setColor(0,127,0); // green
      delay(150);
      rgb.setColor(0,0,127); // blue
      delay(150);
      rgb.setColor(0,0,0);  // off
      delay(150);
    } // while loop end
  }

  // Calculate new tide height based on current time
  tideHeightft = myTideCalc.currentTide(newtime);
  Serial.println(myTideCalc.returnStationID());
  Serial.print(F("Tide height, ft: "));
  Serial.println(tideHeightft,3);
  
  //--------- SD card startup -------------------------------
  if (!sd.begin(CS_SD, SPI_FULL_SPEED)) {
  // If the above statement returns FALSE after trying to 
  // initialize the card, enter into this section and
  // hold in an infinite loop.
    // There is an error with the SD card, halt everything
//    oled1.home();
//    oled1.clear();
//    oled1.println(F("SD ERROR"));
//    oled1.println();
//    oled1.println(F("Continue?"));
//    oled1.println(F("Press 1"));
    Serial.println(F("SD error, press BUTTON1 on board to continue"));

    sdErrorFlag = true;
    bool stallFlag = true; // set true when there is an error
    while(stallFlag){ // loop due to SD card initialization error
      rgb.setColor(255,0,0);
      delay(100);
      rgb.setColor(0,0,255);
      delay(100);
      rgb.setColor(0,0,0);
      delay(100);
      if (digitalRead(BUTTON1) == LOW){
        delay(40);  // debounce pause
        if (digitalRead(BUTTON1) == LOW){
          // If button is still low 40ms later, this is a real press
          // Now wait for button to go high again
          while(digitalRead(BUTTON1) == LOW) {;} // do nothing
          stallFlag = false; // break out of while(stallFlag) loop
        } 
      }              
    }
  }  else {
    Serial.println(F("SD OKAY"));
  }  // end of (!sd.begin(chipSelect, SPI_FULL_SPEED))
  delay(1000);
  // If the clock and sd card are both working, we can save data
  if (!sdErrorFlag && !rtcErrorFlag){ 
    saveData = true;
  } else {
    saveData = false;
  }
  
  if (saveData){
    // If both error flags were false, continue with file generation
    newtime = rtc.now(); // grab the current time
    initFileName(sd, logfile, newtime, filename, serialValid, serialNumber); // generate a file name
    Serial.println(filename);
  }

//------------ PWM chip initialization ----------------------------------
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency
  // Set all PWM channels off initially
  for (byte i = 0; i < 16; i++){
    pwm.setPWM(i, 0, 0); // Channel, on, off (relative to start of 4096-part cycle)  
  }
  
//--------- PID start -------------------------------------
  myPID.begin(&kp, &ki, &kd, pidSampleTime);
  Serial.print(F("kp: "));
  Serial.print(kp, 5);
  Serial.print(F(" ki: "));
  Serial.print(ki, 5);
  Serial.print(F(" kd: "));
  Serial.println(kd, 5);

//------- Battery voltage start -------------------------------------
  batteryVolts = readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage);
  if (batteryVolts < voltageMin){
    lowVoltageFlag = true; // Battery voltage is too low to continue        
  }  
  Serial.print(F("Battery voltage: "));
  Serial.println(batteryVolts,2);


//-----------------------------------------------------------------
  debounceState = DEBOUNCE_STATE_IDLE;
  mainState = STATE_IDLE; // Start the main loop in idle mode (mosfets off)
  attachInterrupt(0, buttonFunc, LOW);
  newtime = rtc.now();
  oldtime = newtime;
  oldday = oldtime.day();
  // Initialize the timing for sampling MAX31820s
  prevMaxTime = millis();

}         // end of setup loop------------------------------------------------



//----------------------------------------------------------------------------
//----- Main loop ------------------------------------------------------------
void loop() {
    // First we have several operations that should update every time through the
    // loop, regardless of the mainState of the state machine
    // Array to hold temperature results
    double MAXTemps[numRefSensors] = {}; // MAX31820 temperature array
    byte goodReadings = 0;              // Number of good MAX31820 readings in a cycle

    //---------------------------------------------------
    // Check to see if a new minute has elapsed, update tide height estimate
    newtime = rtc.now();
    if ( newtime.minute() != oldtime.minute() ) {
      // If the minute values don't match, a new minute has turned over
      // Recalculate tide height, update oldtime
      oldtime = newtime;
      // Calculate new tide height based on current time
      tideHeightft = myTideCalc.currentTide(newtime);
      // Reset lowtideLimitFlag if tide is above threshold
      if (tideHeightft > (tideHeightThreshold + 0.1)){
        lowtideLimitFlag = false; // Reset to false to allow heating on next low tide 
      }
      // Check the battery voltage as well
      batteryVolts = readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage);
      if (batteryVolts < voltageMin){
        lowVoltageFlag = true; // Battery voltage is too low to continue        
      }
    }
    //---------------------------------------------------------------------
    // Check to see if enough time has elapsed to read the mussel temps
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
      for (byte i = 0; i < NUM_THERMISTORS; i++){
        mux.setADG725channel(ADGchannel | i); // Activate channel i 
 // Take an initial reading and throw it away (gives ADC time to settle)
        thermistor1 -> readCelsius();
        delay(2); 
        double tempVal = 0;
        double thermAvg = 0;
        goodReadings = 0;
        
        for (byte j = 0; j < THERM_AVG; j++){
          // Take the temperature reading
          tempVal = thermistor1 -> readCelsius();
          // Check the temperature reading relative to previous step's temperature value
          // I was getting spurious temperature spikes (high and low) in excess of 2-3C every
          // 10-30 seconds, which really threw off the PID routine. This filtering below 
          // just throws out temperatures than change too much in the ~500ms between loops
          if (pidInput[i] > -100) {
            // If previous pidInput temperature was reasonable, use it to 
            // filter out spurious large changes in the new temperature readings
           if ( abs(tempVal - pidInput[i]) < TEMP_FILTER ){
              // Value is probably good
              thermAvg += tempVal;
              goodReadings = goodReadings + 1;
           }           
          } else {
              // If previous pidInput was questionable, just get a new set of temps
              // This allows recovery from cases where a thermistor goes offline 
              // temporarily. 
              thermAvg += tempVal;
              goodReadings = goodReadings + 1;
          }
        }
        // Calculate an average temperature from the good readings
        pidInput[i] = thermAvg / (double)goodReadings;
      }
      // Turn off all multiplexer channels (shuts off thermistor circuits)
      mux.disableADG725(); 

      //------------ Save data to card
      if ( (newtime.second() % SAVE_INTERVAL) == 0){
          if (saveData){
            writeData = true; // set flag to write samples to SD card     
          }          
      }
      if (saveData && writeData){
        // If saveData is true, and it's time to writeData, then do this:
        // Check to see if a new day has started. If so, open a new file
        // with the initFileName() function
        if (oldtime.day() != oldday) {
          // Close existing file
          logfile.close();
          // Generate a new output filename based on the new date
          initFileName(sd, logfile, oldtime, filename, serialValid, serialNumber); // generate a file name
          // Update oldday value to match the new day
          oldday = oldtime.day();
        }
        // Call the writeToSD function to output the data array contents
        // to the SD card
//        Serial.print(F("sd write  ")); // troubleshooting
//        delay(10);
//          bitSet(PIND, 3);  // toggle on, troubleshooting
        writeToSD(newtime, MAXTemps);
//          bitSet(PIND, 3);  // toggle off, troubleshooting
        writeData = false; // reset flag
      }

        // Write up to 4 reference mussel temperature values in a loop
      for (byte i = 0; i < numRefSensors; i++){
        Serial.print(F(","));
        Serial.print(MAXTemps[i],2); // write with 2 sig. figs.
      }
      // Handle a case where there are missing reference mussels, fill in NA values
      if (numRefSensors < 4){
        byte fillNAs = 4 - numRefSensors;
        for (byte i = 0; i < fillNAs; i++){
          Serial.print(F(","));
          Serial.print(F("NA"));
        }
      }
      Serial.print(F("\t"));
      // Write the 16 thermistor temperature values in a loop
      for (byte i = 0; i < 16; i++){
        Serial.print(F(","));
        if ( (pidInput[i] < -10) | (pidInput[i] > 60) ){
          // If temperature value is out of bounds, write NA
          Serial.print(F("NA"));
        } else {
          // If value is in bounds, write temperature
          Serial.print(pidInput[i],2); // write with 2 sig. figs.
        }
      }


      Serial.println();
      flashFlag = !flashFlag;
    } // end of MAX31820 & thermistor sampling




  //-------------------------------------------------------------
  // Check the debounceState to 
  // handle any button presses
  switch (debounceState) {
    // debounceState should normally start off as 
    // DEBOUNCE_STATE_IDLE until button1 is pressed,
    // which causes the state to be set to 
    // DEBOUNCE_STATE_CHECK
    //************************************
    case DEBOUNCE_STATE_IDLE:
      // Do nothing in this case
    break;
    //************************************
    case DEBOUNCE_STATE_CHECK:
      // If the debounce state has been set to 
      // DEBOUNCE_STATE_CHECK by the buttonFunc interrupt,
      // check if the button is still pressed
      if (digitalRead(BUTTON1) == LOW) {
          if (millis() > button1Time + debounceTime) {
            // If the button has been held long enough to 
            // be a legit button press, switch to 
            // DEBOUNCE_STATE_TIME to keep track of how long 
            // the button is held
            debounceState = DEBOUNCE_STATE_TIME;
          } else {
            // If button is still pressed, but the debounce 
            // time hasn't elapsed, remain in this state
            debounceState = DEBOUNCE_STATE_CHECK;
          }
        } else {
          // If button1 is high again when we hit this
          // case in DEBOUNCE_STATE_CHECK, it was a false trigger
          // Reset the debounceState
          debounceState = DEBOUNCE_STATE_IDLE;
          // Restart the button1 interrupt
          attachInterrupt(0, buttonFunc, LOW);
        }
    break; // end of case DEBOUNCE_STATE_CHECK
    //*************************************
    case DEBOUNCE_STATE_TIME:
      if (digitalRead(BUTTON1) == HIGH) {
        // If the user released the button, now check how
        // long the button was depressed. This will determine
        // which state the user wants to enter. 

        unsigned long checkTime = millis(); // get the time
        
        if (checkTime < (button1Time + mediumPressTime)) {
//          Serial.println(F("Short press registered"));
          // User held button briefly, treat as a normal
          // button press, which will be handled differently
          // depending on which mainState the program is in.
          mainState = STATE_IDLE;
        } else if ( checkTime > (button1Time + mediumPressTime)){
          // User held button1 long enough to enter mediumPressTime mode
          // Set state to STATE_HEATING
          mainState = STATE_HEATING;
        }
        // Now that the button press has been handled, return
        // to DEBOUNCE_STATE_IDLE and await the next button press
        debounceState = DEBOUNCE_STATE_IDLE;
        // Restart the button1 interrupt now that button1
        // has been released
        attachInterrupt(0, buttonFunc, LOW);  
      } else {
        // If button is still low (depressed), remain in 
        // this DEBOUNCE_STATE_TIME
        debounceState = DEBOUNCE_STATE_TIME;
      }
      break; // end of case DEBOUNCE_STATE_TIME 
  } // end switch(debounceState)

  //-------------------------------------------------------------
  //-------------------------------------------------------------
  switch(mainState){
    case STATE_IDLE:
      // Tide is high, or it is night time, so the heaters should be off
      // To do here: 
      //    check if battery lowVoltageFlag has been set true (go to STATE_OFF)
      //    check if tide height has dropped below tideHeightThreshold 
      //    check if time of day is between SUNRISE_HOUR and SUNSET_HOUR
      
      if (lowVoltageFlag == true){
        // If the lowVoltageFlag has been set true, switch to STATE_OFF
        mainState = STATE_OFF;
      }

      if ( (tideHeightft > tideHeightThreshold) & 
            (newtime.hour() >= SUNRISE_HOUR) & 
            (newtime.hour() < SUNSET_HOUR) & 
            lowVoltageFlag == false & 
            lowtideLimitFlag == false){
              // If the statements here are all true, switch to STATE_HEATING
              mainState = STATE_HEATING;
              // Zero out the PID values
              myPID.resetPID(pidOutput, pidOutputSum, lastTime, NUM_THERMISTORS);
     }
     if (flashFlag){
      rgb.setColor(0,30,0); // flash green
     } else if (!flashFlag){
      rgb.setColor(0,0,0); // turn off green
     }
    break; // end of STATE_IDLE case
    //---------------------------------------
    // You can arrive at STATE_HEATING via one of 2 paths, either
    // because the user pressed button1 long enough to trigger the 
    // state change, or because the heating conditions were satisfied
    // in the STATE_IDLE case. 
    case STATE_HEATING:

      if ( (tideHeightft < tideHeightThreshold) & 
            (newtime.hour() >= SUNRISE_HOUR) & 
            (newtime.hour() < SUNSET_HOUR) & 
            lowVoltageFlag == false & 
            lowtideLimitFlag == false)
      {
          // Assuming all of the above is true, we should be heating
          //------PID update-----------------
          // Update the target temperature Setpoint for the heated mussels
          pidSetpoint = avgMAXtemp + tempIncrease;
          // The PID Compute() function updates whenever SampleTime has
          // been exceeded (checked inside the Compute() function).
          myPID.Compute(pidInput, 
                        pidOutput, 
                        pidOutputSum, 
                        pidLastInput, 
                        pidSetpoint, 
                        pidSampleTime,
                        lastTime,
                        kp, ki, kd,
                        NUM_THERMISTORS);    

          //------PWM update----------------
          // This should only run during daytime low tides
          // Update PWM output values
          // Send the channel, start value (0), and end value (0-4095)
          for (byte i = 0; i < NUM_THERMISTORS; i++){
            pwm.setPWM(i, 0, (uint16_t)pidOutput[i]);  
          }
         if (flashFlag){
          rgb.setColor(30,0,0); // flash red
         } else if (!flashFlag){
          rgb.setColor(0,0,0); // turn off red
         }
      } else if (tideHeightft >= tideHeightThreshold) {
        // Tide has gone high, go back to idle
        mainState = STATE_IDLE;
        lowtideLimitFlag = false;
      } else if (newtime.hour() >= SUNSET_HOUR){
        // If time has rolled past SUNSET_HOUR, go to idle
        mainState = STATE_IDLE;
        lowtideLimitFlag = false;
      } else if (lowVoltageFlag == true){
        mainState = STATE_OFF;
    break; // end of STATE_HEATING case
    //-----------------------------------------
    // STATE_OFF handles the case when the lowVoltageFlag is true,
    // and forces the device into a waiting mode that it can't escape
    // from without user intervention (reset or button press).
    case STATE_OFF:
        // Zero out the PID values
        myPID.resetPID(pidOutput, pidOutputSum, lastTime, NUM_THERMISTORS);
        // Set all PWM heater channels to zero
        for (byte pwmchan = 0; pwmchan < 16; pwmchan++){
          pwm.setPWM(pwmchan, 0, 0); // Channel, on, off (relative to start of 4096-part cycle)  
        }
        
        if (flashFlag){
          if (lowVoltageFlag){
           rgb.setColor(0,0,60); // flash blue color  
          }
        } else if (!flashFlag) {
          rgb.setColor(0,0,0); // turn off LED
        }
        
        mainState = STATE_OFF;
        
    break; // end of STATE_OFF case
  }

} // end of main loop ---------------------------------------
//-----------------------------------------------------------------------------



//--------------- buttonFunc --------------------------------------------------
// buttonFunc
void buttonFunc(void){
  detachInterrupt(0); // Turn off the interrupt
  button1Time = millis();
  debounceState = DEBOUNCE_STATE_CHECK; // Switch to new debounce state
}



//------------- writeToSD -----------------------------------------------
// writeToSD function. This formats the available data in the
// data arrays and writes them to the SD card file in a
// comma-separated value format.
void writeToSD (DateTime timestamp, double refTemps[]) {
  // Reopen logfile. If opening fails, notify the user
  if (!logfile.isOpen()) {
    if (!logfile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
      rgb.setColor(90,0,0); // turn on error LED
    }
  }
  // Write the unixtime
  logfile.print(timestamp.unixtime(), DEC); // POSIX time value
  logfile.print(F(","));
  printTimeToSD(logfile, timestamp); // human-readable 
  // Write up to 4 reference mussel temperature values in a loop
  for (byte i = 0; i < numRefSensors; i++){
    logfile.print(F(","));
    logfile.print(refTemps[i],2); // write with 2 sig. figs.
  }
  // Handle a case where there are missing reference mussels, fill in NA values
  if (numRefSensors < 4){
    byte fillNAs = 4 - numRefSensors;
    for (byte i = 0; i < fillNAs; i++){
      logfile.print(F(","));
      logfile.print(F("NA"));
    }
  }
  // Write the Setpoint being used currently
  logfile.print(F(","));
  logfile.print(pidSetpoint,2);
  // Write the 16 thermistor temperature values in a loop
  for (byte i = 0; i < 16; i++){
    logfile.print(F(","));
    if ( (pidInput[i] < -10) | (pidInput[i] > 60) ){
      // If temperature value is out of bounds, write NA
      logfile.print(F("NA"));
    } else {
      // If value is in bounds, write temperature
      logfile.print(pidInput[i],2); // write with 2 sig. figs.
    }
  }
  logfile.print(F(","));
  logfile.print(batteryVolts,2);
  logfile.println();
  // logfile.close(); // force the buffer to empty

  if (timestamp.second() % 30 == 0){
      logfile.timestamp(T_WRITE, timestamp.year(),timestamp.month(), \
      timestamp.day(),timestamp.hour(),timestamp.minute(),timestamp.second());
  }
}
