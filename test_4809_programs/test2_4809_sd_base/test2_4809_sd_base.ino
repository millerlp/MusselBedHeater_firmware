// 4809 SD card futzing

#include <SPI.h>
//#include <SD.h>
#include "SdFat.h"

//const int chipSelect = 7;
const uint8_t SD_CHIP_SELECT = 7;
#define MOSI 4
#define MISO 5
#define SCK 6


SdFat sd;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
