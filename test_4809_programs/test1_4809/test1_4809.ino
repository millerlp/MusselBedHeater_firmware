// First 4809 test - works after updating Atmel ICE to firmware v1.29 from 1.0

#include "Arduino.h"

// RevD RGB led pins
byte redpin = 16;  // pin 16 on 4809
byte greenpin = 14;  // pin 14 on 4809
byte bluepin = 15;  // pin 15 on 4809

void setup() {
  // put your setup code here, to run once:

  pinMode(redpin, OUTPUT);
  pinMode(greenpin, OUTPUT);
  pinMode(bluepin, OUTPUT);

  digitalWrite(redpin, HIGH); // for common anode LED, set high to shut off
  digitalWrite(greenpin, HIGH);
  digitalWrite(bluepin, HIGH);  

  Serial.begin(115200);
  delay(100);
  Serial.println("Hello");
  

}

void loop() {
  // put your main code here, to run repeatedly:
  for (byte i = 0; i < 5; i++){
    RGBLEDsetColor(0,10,0);
    delay(1000);
//    Serial.println("Flash");
    RGBLEDsetColor(0,0,0);
    delay(1000);    
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
