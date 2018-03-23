/**
  * FeatherCandleTrinket.ino
  *
  * @file     FeatherCandleTrinket.ino
  * @author   Simon Burkhardt - github.com/mnemocron
  * @date     2018-02-16
  * @brief    This is the code for the animation part of the feather candle
  * @see      https://github.com/mnemocron/FeatherCandle
  * 
  * @details  This code can be use to drive an Adafruit 15x7 CharliePlex LED Matrix FeatherWing
  *           with an Adafrtuit Trinket M0. This code uses one output pin to signal the state of the flame
  *           to the HUZZAH. There are two inputs. One is connected to a pushbutton to turn on/off the flame.
  *	          The other input is connected to the HUZZAH and listens to state changes on the Adafruit.io server.
  *           Original code for the flame animation by wbphelps on github.
  * @see      https://www.adafruit.com/product/3163
  * @see      https://github.com/wbphelps/FeatherCandle
  * @see      https://learn.adafruit.com/animated-flame-pendant/overview
  * 
  * @note     using Adafruit Trinket M0
  * @see      https://www.adafruit.com/product/3500
  */

#include <Adafruit_IS31FL3731.h>    // https://github.com/adafruit/Adafruit_IS31FL3731
#include "data7x15flip.h"           // Flame animation data
/* @note   convert.py is changed to flip the images top to bottom before converting
 *         so that they lign up with the orientation of the FeatherWing matrix
 */

// @see   https://learn.adafruit.com/adafruit-trinket-m0-circuitpython-arduino/pinouts
// #define PIN_SDA       0       // pin is reserved for TWI/I2C
#define    PIN_BTN       1
// #define PIN_SCL       2       // pin is reserved for TWI/I2C
#define    PIN_ONOFF_WEB 3
#define    PIN_ONOFF_LED 4

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

/* SETUP FUNCTION - RUNS ONCE AT STARTUP --------------------------------------------------- */
void setup() {
//  Serial.begin(57600);
//  delay(100);
//  Serial.println("Trinket Candle");

  if (! leds.begin()) {
//    Serial.println("IS31 not found");
    while (1);
  }
  pinMode(PIN_BTN, INPUT);
  /* @todo   test INPUT_PULLUP with and without Feather HUZZAH */
  // pinMode(PIN_ONOFF_WEB, INPUT_PULLUP);  // pull up for standalone Trinket (w/o Feather HUZZAH)
  pinMode(PIN_ONOFF_WEB, INPUT);
  pinMode(PIN_ONOFF_LED, OUTPUT);
}

/* LOOP FUNCTION - RUNS EVERY FRAME -------------------------------------------------------- */
void loop() {
  BTN_reading = digitalRead(PIN_BTN);
  WEB_reading = digitalRead(PIN_ONOFF_WEB);
  if(BTN_reading == HIGH && BTN_previous == LOW && (millis() - buttontime_btn) > debounce){
    buttontime_btn = millis();
    flameOn = !flameOn;
//    if(flameOn) Serial.println("Flame On!");
//    else Serial.println("Flame Off!");
    ptr = anim;    // restart the animation at the first frame
  }
  // rising edge from the HUZZAH  --> Candle was turned ON on the Adafruit IO feed
  if(WEB_reading == HIGH && WEB_previous == LOW && (millis() - buttontime_web) > debounce){
    flameOn = true;
    buttontime_web = millis();
//    Serial.println("Flame On!");
    ptr = anim;    // restart the animation at the first frame
  }
  // falling edge from the HUZZAH --> Candle was turned OFF on the Adafruit IO feed
  if(WEB_reading == LOW && WEB_previous == HIGH && (millis() - buttontime_web) > debounce){
    flameOn = false;
    buttontime_web = millis();
//    Serial.println("Flame Off!");
  }
  BTN_previous = BTN_reading;
  WEB_previous = WEB_reading;

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
