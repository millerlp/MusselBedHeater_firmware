#include "MusselBedHeaterlib.h" // https://github.com/millerlp/MusselBedHeaterlib

//--------- RGB LED setup --------------------------
// Create object for red green blue LED
RGBLED rgb;



void setup() {
  rgb.begin(); // Setup RGB with default pins (9,6,5)
}

void loop() {

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
