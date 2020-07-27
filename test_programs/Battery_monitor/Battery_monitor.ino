/* Battery_monitor.ino
 *  Luke Miller 2020
 *  Test script to see if battery voltage monitor is reading correctly
 * 
 * 
 */

#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib

//  Battery monitor 
#define BATT_MONITOR A7  // analog input channel to sense battery voltage
#define BATT_MONITOR_EN 4 // digital output channel to turn on battery voltage check
float voltageMin = 11.20; // Minimum battery voltage allowed, shut off below this. Units: volts
float dividerRatio = 5.7; // Ratio of voltage divider (47k + 10k) / 10k = 5.7
float refVoltage = 3.00; // Voltage at AREF pin from MAX6103 3.0V reference chip (RevF)
float batteryVolts = 0; // Estimated battery voltage returned from readBatteryVoltage function
bool lowVoltageFlag = false;
byte lowVoltageCounter = 0;
bool flashFlag = true;

//--------- RGB LED setup --------------------------
// Create object for red green blue LED
RGBLED rgb;

unsigned long myMillis;
unsigned long timeStepMillis = 1000;

void setup() {
  Serial.begin(57600);
  Serial.println(F("Battery voltage"));
  analogReference(EXTERNAL); // hooked to MAX6103 3.00V reference on RevF hardware 

  // Battery monitor pins
  pinMode(BATT_MONITOR, INPUT);
  pinMode(BATT_MONITOR_EN, OUTPUT);

  rgb.begin(); // Setup RGB with default pins (9,6,5)
  for (byte i = 0; i < 5; i++){
    rgb.setColor(0,255,0);
    delay(30);
    rgb.setColor(0,0,0);
    delay(30);    
  }
  myMillis = millis();
}

void loop() {

  if ( (millis() - myMillis) > timeStepMillis) {
    myMillis = millis(); // update myMillis
    // Read battery voltage
    batteryVolts = readBatteryVoltage(BATT_MONITOR_EN,BATT_MONITOR,dividerRatio,refVoltage);
    Serial.print(batteryVolts,2);
    Serial.print("\t Counter: ");
    Serial.println(lowVoltageCounter);
    if (batteryVolts < voltageMin){
      Serial.print("Low");
        if (lowVoltageCounter <= 5){
          lowVoltageCounter++;
        }
        if (lowVoltageCounter > 5) {
          lowVoltageFlag = true; // Battery voltage is too low to continue        
        }
    }

    if (flashFlag){
      if (lowVoltageFlag) {
        rgb.setColor(0,0,50); // flash blue
      } else {
        rgb.setColor(0,50,0); // flash green
      }
    } else {
      rgb.setColor(0,0,0); // turn off
    }
    flashFlag = !(flashFlag); // toggle flashFlag each time
  }

}
