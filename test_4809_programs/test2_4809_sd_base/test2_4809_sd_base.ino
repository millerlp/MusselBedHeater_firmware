// 4809 SD card futzing
/*  This works with a 4809 installed on RevC of the MusselBedHeater board 
 *  using the "Standard" 48-pin pinout for the 4809 chip as defined in the MegaCoreX
 *  boards file (https://github.com/MCUdude/MegaCoreX) using version 1.0.8 (2021-05),
 *  with the Optiboot (UART0 default pins) bootloader option,
 *  and using SdFat-beta version=2.1.0-beta.1 (https://github.com/greiman/SdFat-beta)
 *  The bootloader was burned with an Atmel-ICE via UPDI, after upgrading the
 *  programmer to firmware v1.29 from the shipped v1.0. Arduino v1.8.13
 *  
 *  This will flash red-green-blue if the card is found, and just flash red
 *  if no card is found.
 */


#include "SdFat.h"

// SPI pins for SD card
const uint8_t SD_CHIP_SELECT = 7;
#define MOSI 4
#define MISO 5
#define SCK 6

// RGB LED pins
byte redpin = 16;  // pin 16 on 4809
byte greenpin = 14;  // pin 14 on 4809
byte bluepin = 15;  // pin 15 on 4809


SdFat sd;

bool failFlag = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello");

  // see if the card is present and can be initialized:
  if (!sd.begin(SD_CHIP_SELECT)) {
    Serial.println("Card failed, or not present");
    failFlag = true;
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

  pinMode(redpin, OUTPUT);
  pinMode(greenpin, OUTPUT);
  pinMode(bluepin, OUTPUT);

  digitalWrite(redpin, HIGH); // for common anode LED, set high to shut off
  digitalWrite(greenpin, HIGH);
  digitalWrite(bluepin, HIGH); 
  
}

void loop() {

  if (failFlag == false){
    for (byte i = 0; i < 5; i++){
      RGBLEDsetColor(0,10,0);
      delay(500);
      RGBLEDsetColor(0,0,0);
      delay(500);  
      RGBLEDsetColor(10,0,0);  
      delay(500);  
      RGBLEDsetColor(0,0,0);  
      delay(500);  
      RGBLEDsetColor(0,0,10);  
      delay(500);  
      RGBLEDsetColor(0,0,0);  
      delay(500);  
    }
  } else if (failFlag) {
      RGBLEDsetColor(10,0,0);  
      delay(500);  
      RGBLEDsetColor(0,0,0);  
      delay(500); 
  }
}


void RGBLEDsetColor(uint8_t red, uint8_t green, uint8_t blue)
{

  red = 255 - red;
  green = 255 - green;
  blue = 255 - blue;

  analogWrite(redpin, red);
  analogWrite(greenpin, green);
  analogWrite(bluepin, blue);  
}
