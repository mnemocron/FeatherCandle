/**
  * FeatherCandleFeather.ino
  *
  * @file     FeatherCandleFeather.ino
  * @author   Simon Burkhardt - github.com/mnemocron
  * @date     2018-03-15
  * @brief    This is the code for the animation part of the feather candle
  * @see      https://github.com/mnemocron/FeatherCandle
  * 
  * @details  This code can be use to drive an Adafruit 15x7 CharliePlex LED Matrix FeatherWing
  *           with any Adafruit Feather board (tested on the Feather HUZZAH).
  *           Original code for the flame animation by wbphelps on github.
  * @see      https://github.com/wbphelps/FeatherCandle
  * @see      https://learn.adafruit.com/animated-flame-pendant/overview
  * 
  * @note     using Adafruit Feather (tested with Feather HUZZAH)
  * @see      https://www.adafruit.com/product/2821
  */

#include <Adafruit_IS31FL3731.h>
#include "data7x15flip.h"           // Flame animation data
/* @note   convert.py is changed to flip the images top to bottom before converting
 *         so that they lign up with the orientation of the FeatherWing matrix
 */

// @see   https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/pinouts
// #define PIN_SDA        4   // pin is reserved for TWI/I2C
// #define PIN_SCL        5   // pin is reserved for TWI/I2C
#define    PIN_BTN       16   // button on the back of the candle PCB
#define    PIN_ONOFF_LED 13

// using the FeatherWing version
Adafruit_IS31FL3731_Wing leds = Adafruit_IS31FL3731_Wing();

uint8_t        page = 0;     // Front/back buffer control
const uint8_t *ptr  = anim;  // Current pointer into animation data
const uint8_t  w    = 7;     // image width
const uint8_t  h    = 15;    // image height
uint8_t        img[w*h];     // Buffer for rendering image

bool flameOn = false;        // candle on/off
uint32_t BTN_reading = LOW;  // edge detection for button
uint32_t BTN_previous = LOW; // remember old button reading
uint32_t WEB_reading = LOW;  // edge detection for HUZZAH
uint32_t WEB_previous = LOW; // remember old HUZZAH reading
long buttontime_btn = 0;     // used for debounce time
long buttontime_web = 0;     // used for debounce time
long debounce = 100;         // debouce time in [ms]

void setup() {
//  Serial.begin(57600);
//  delay(100);
//  Serial.println("Trinket Candle");

  if (! leds.begin()) {
//    Serial.println("IS31 not found");
    while (1);
  }
  pinMode(PIN_BTN, INPUT);
  pinMode(PIN_ONOFF_LED, OUTPUT);
  delay(200);
}

void loop() {
  BTN_reading = digitalRead(PIN_BTN);
//  WEB_reading = digitalRead(PIN_ONOFF_WEB);
  if(BTN_reading == HIGH && BTN_previous == LOW && (millis() - buttontime_btn) > debounce){
    buttontime_btn = millis();
    flameOn = !flameOn;
//    if(flameOn) Serial.println("Flame On!");
//    else Serial.println("Flame Off!");
    ptr = anim;    // restart the animation
  }
  BTN_previous = BTN_reading;


  if(flameOn){
    digitalWrite(PIN_ONOFF_LED, HIGH);    // signal to the HUZZAH that the flame is ON
    /* BEGIN: ANIMATION SECTION */
    uint8_t  a, x1, y1, x2, y2, x, y;
    // read NEXT frame.  Start by getting bounding rect for new frame:
    a = pgm_read_byte(ptr++);     // New frame X1/Y1
    if(a >= 0x90) {               // EOD marker? (valid X1 never exceeds 8)
      ptr = anim;                 // Reset animation data pointer to start
      a   = pgm_read_byte(ptr++); // and take first value
    }
    x1 = a >> 4;                  // X1 = high 4 bits
    y1 = a & 0x0F;                // Y1 = low 4 bits
    a  = pgm_read_byte(ptr++);    // New frame X2/Y2
    x2 = a >> 4;                  // X2 = high 4 bits
    y2 = a & 0x0F;                // Y2 = low 4 bits

    // Read rectangle of data from anim[] into portion of img[] buffer
    for(uint8_t y=y1; y<=y2; y++)
      for(uint8_t x=x1; x<=x2; x++) { 
        img[y*w + x] = pgm_read_byte(ptr++);
    }

    page ^= 1; // Flip front/back buffer index
    leds.setFrame(page);

    int i = 0;
    for (uint8_t x=0; x<h; x++) {
      for (uint8_t y=0; y<w; y++) {
        leds.drawPixel(x, y, img[i++]);  
      }
    }

    leds.displayFrame(page);
    /* END: ANIMATION SECTION */
  } else {
    digitalWrite(PIN_ONOFF_LED, LOW);    // signal to the HUZZAH that the flame is OFF
    leds.clear();
  }
}

