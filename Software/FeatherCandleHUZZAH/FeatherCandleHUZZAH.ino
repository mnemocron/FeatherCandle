/**
  * FeatherCandleHUZZAH.ino
  *
  * @file     FeatherCandleHUZZAH.ino
  * @author   Simon Burkhardt - github.com/mnemocron
  * @date     2018-03-23
  * @brief    This is the code for the WiFi part. It connects the candle to the Adafruit.io cloud.
  * @see      https://github.com/mnemocron/FeatherCandle
  * @details  This code uses two input pins. One is for the reset on startup button. The other input is to
  *           detect wether or not the candle was turned on by the Trinket M0.
  *           The output is used to signal state changes comming from the Adafruit server to the Trinket M0.
  *           This way the Trinket can control the state on the Adafruit server and the state on the Adafruit server
  *           can also be reflected back to the Trinket. 
  * @see      https://www.adafruit.com/product/3163
  * @note     using Adafruit Feather HUZZAH
  * @see      https://www.adafruit.com/product/3404
  *
  * @todo     make use of the onboard LED and the status LED
  */

#include <FS.h>                   // this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "ESP8266HTTPClient.h"

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson

// select which pin will trigger the configuration portal when set to LOW
// @see   https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/pinouts
#define PIN_BTN 16
#define PIN_LED_ONBOARD 0
#define PIN_ONOFF_WEB 13
#define PIN_ONOFF_LED 12
#define PIN_LED_STATUS 14

// define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server      [34] = "io.adafruit.com";  // MQTT server
char mqtt_port        [6]  = "1883";             // use 8883 for SSL
char aio_key          [34] = "";                 // Adafruit IO key
char aio_username     [34] = "";                 // Adafruit username
char aio_feed         [34] = "candle";           // feed name
char aio_poll_interval[5]  = "3";                // http API request polling interval in [s]
char aio_last_will    [3]  = "0";                // value to publish on disconnect --> 0 = turn off candle

// aio_username[34] + "/feed/" + aio_feed[34]
char aio_publisher[34+34+9] = "";                // "url" to the Adafruit IO feed
// "http://io.adafruit.com/api/v2/" + aio_publisher[34+34+9] + "?X-AIO-Key=" + aio_key[34]
char api_uri[34+(34+34+9)+13+34] = "";           // uri used for http requests to the Adafruit IO API

uint32_t flame_on = LOW;    // candle on/off
uint32_t LED_reading = 0;   // edge detection for button
uint32_t LED_previous = 0;  // remember old button reading
long buttontime = 0;        // used for debounce time
long debounce = 100;        // debouce time in [ms]
uint32_t counter = 0;
uint32_t polling_delay = 3; // default to 3

// flag for saving data
bool shouldSaveConfig = true;

// callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
/** @note   dirty: just pass placeholder values to the constructor, since the MQTT library has no
  *         begin() method to later initialize the values loaded from flash
  */
Adafruit_MQTT_Client mqtt(&client, "io.adafruit.com", 1883, "placeholder", "somekey");
// Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup a feed called 'candlePublish' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
/** @note   dirty: just pass placeholder values to the constructor, since the MQTT library has no
  *         begin() method to later initialize the values loaded from flash
  */
Adafruit_MQTT_Publish candlePublish = Adafruit_MQTT_Publish(&mqtt, "placeholder", MQTT_QOS_1);
// Adafruit_MQTT_Publish candlePublish = Adafruit_MQTT_Publish(&mqtt, AIO_PUBLISHER, MQTT_QOS_1);

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  // put your setup code here, to run once:
  pinMode(PIN_BTN, INPUT);
  pinMode(PIN_ONOFF_WEB, OUTPUT);
  pinMode(PIN_ONOFF_LED, INPUT);
  pinMode(PIN_BTN, INPUT);
  pinMode(PIN_LED_ONBOARD, OUTPUT);
  Serial.begin(115200);
  Serial.println("Feather Candle");

  // clean FS, for testing
  // SPIFFS.format();
  
  /* END: READ JSON CONFIG */
  // read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      /** @important needed if you add/change keys to the json file
        *            or else the ESP8266 it will throw an exception (28)
        */
      // SPIFFS.remove("/config.json");

      // file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server,       json["mqtt_server"]);
          strcpy(mqtt_port,         json["mqtt_port"]);
          strcpy(aio_key,           json["aio_key"]);
          strcpy(aio_username,      json["aio_username"]);
          strcpy(aio_feed,          json["aio_feed"]);
          strcpy(aio_poll_interval, json["aio_poll_interval"]);
          strcpy(aio_last_will,     json["aio_last_will"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  /* END: READ JSON CONFIG */

  /* BEGIN: RESET BUTTON PRESSED ON STARTUP */
  // user pressed the button on the board and restarted the Feather HUZZAH 
  // --> reset config and open access point 
  if ( digitalRead(PIN_BTN) == HIGH ) {
    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server      ("server",   "mqtt server",                   mqtt_server,       34);
    WiFiManagerParameter custom_mqtt_port        ("port",     "mqtt port",                     mqtt_port,         6);
    WiFiManagerParameter custom_aio_key          ("key",      "AdafruitIO key",                aio_key,           34);
    WiFiManagerParameter custom_aio_username     ("username", "AdafruitIO username",           aio_username,      34);
    WiFiManagerParameter custom_aio_feed         ("feed",     "AdafruitIO feed",               aio_feed,          34);
    WiFiManagerParameter custom_aio_poll_interval("interval", "polling interval (s)",          aio_poll_interval, 5);
    WiFiManagerParameter custom_aio_last_will    ("will",     "\"last will\" (on disconnect)", aio_last_will,     3);

    // WiFiManager
    // Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    // exit after config instead of connecting
    wifiManager.setBreakAfterConfig(true);
    // set static ip
    // wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    // add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_aio_key);
    wifiManager.addParameter(&custom_aio_username);
    wifiManager.addParameter(&custom_aio_feed);
    wifiManager.addParameter(&custom_aio_poll_interval);
    wifiManager.addParameter(&custom_aio_last_will);

    //reset settings - for testing
    wifiManager.resetSettings();      // reset, because the button was pressed to load new values

    // set minimu quality of signal so it ignores AP's under that quality
    // defaults to 8%
    // wifiManager.setMinimumSignalQuality();
    
    // sets timeout until configuration portal gets turned off
    // useful to make it all retry or go to sleep
    // in seconds
    wifiManager.setTimeout(180);

    // fetches ssid and pass and tries to connect
    // if it does not connect it starts an access point with the specified name
    // here  "AutoConnectAP"
    // and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("Feather Candle", "iloveyou")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }

    // if you get here you have connected to the WiFi
    Serial.println("fetched...yeey :)");

    // read updated parameters
    strcpy(mqtt_server,       custom_mqtt_server.getValue());
    strcpy(mqtt_port,         custom_mqtt_port.getValue());
    strcpy(aio_key,           custom_aio_key.getValue());
    strcpy(aio_username,      custom_aio_username.getValue());
    strcpy(aio_feed,          custom_aio_feed.getValue());
    strcpy(aio_poll_interval, custom_aio_poll_interval.getValue());
    strcpy(aio_last_will,     custom_aio_last_will.getValue());

    // save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();

      json["mqtt_server"]       = mqtt_server;
      json["mqtt_port"]         = mqtt_port;
      json["aio_key"]           = aio_key;
      json["aio_username"]      = aio_username;
      json["aio_feed"]          = aio_feed;
      json["aio_poll_interval"] = aio_poll_interval;
      json["aio_last_will"]     = aio_last_will;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      // end save
    }
    ESP.reset();  // reset the chip
    delay(3000);
  }
  /* END: RESET BUTTON PRESSED ON STARTUP */
  uint16_t aio_port = 0;
  if(strcmp(mqtt_port, "1883") == 0) aio_port = 1883;
  if(strcmp(mqtt_port, "8883") == 0) aio_port = 8883;

  /** @todo   add char[] to int parse for the polling interval time
    *         to set the counter time in the loop()
    */

  Serial.println("Initialize MQTT client:");
  Serial.print(mqtt_server);  Serial.print(":");   Serial.println(aio_port);
  Serial.print(aio_username); Serial.print(" / "); Serial.println(aio_key);
  mqtt = Adafruit_MQTT_Client(&client, mqtt_server, aio_port, aio_username, aio_key);
  
  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to WiFi");

  WiFi.begin();    // connects to last known access point (values in flash)
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  strcat(aio_publisher, aio_username);
  strcat(aio_publisher, "/feeds/");
  strcat(aio_publisher, aio_feed);
  Serial.println("Initialize MQTT publisher:");
  Serial.println(aio_publisher);
  candlePublish = Adafruit_MQTT_Publish(&mqtt, aio_publisher, MQTT_QOS_1);

  mqtt.will(aio_publisher, aio_last_will);
  strcat(api_uri, "http://io.adafruit.com/api/v2/");
  strcat(api_uri, aio_publisher);
  strcat(api_uri, "?X-AIO-Key=");
  strcat(api_uri, aio_key);

  polling_delay = atoi(aio_poll_interval);
  if(polling_delay < 1)   polling_delay = 1;
  if(polling_delay > 100) polling_delay = 100;
}

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  /** @todo   adjust this value to the polling_interval value in seconds
    */
  if(counter > polling_delay){
  	Serial.print("Polling Interval: "); Serial.println(atoi(aio_poll_interval));
    counter = 0;
    if (WiFi.status() == WL_CONNECTED) {
      // Serial.println("getting update...");
      HTTPClient http;
      // Serial.println(api_uri);
      http.begin( api_uri  );
      int httpCode = http.GET();
      if (httpCode > 0) { //Check the returning code
        String payload = http.getString();   //Get the request response payload
        // Serial.println(payload);          //Print the response payload
        const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        JsonObject& jsonResponse = jsonBuffer.parseObject(payload);
        // Serial.println((int)jsonResponse["id"]);
        int last_value = jsonResponse["last_value"];
        // Serial.println(last_value);
        if(last_value != flame_on){
          flame_on = last_value;
          Serial.print(F("Value Update: "));
          Serial.println(last_value);
        }
      }
      http.end();
    }
  }

  LED_reading = digitalRead(PIN_ONOFF_LED);
  // rising edge from the Trinket  --> Candle was turned ON on by the button
  if(LED_reading == HIGH && LED_previous == LOW && millis() - buttontime > debounce){
    flame_on = true;
    buttontime = millis();
    Candle_SendUpdate(flame_on);
  }
  // falling edge from the Trinket  --> Candle was turned ON on by the button
  if(LED_reading == LOW && LED_previous == HIGH && millis() - buttontime > debounce){
    flame_on = false;
    buttontime = millis();
    Candle_SendUpdate(flame_on);
  }
  LED_previous = LED_reading;
  digitalWrite(PIN_ONOFF_WEB, flame_on);
  
  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  delay(1000);
  counter ++;
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}


void Candle_SendUpdate(uint32_t value){
  // Now we can publish stuff!
  Serial.print(F("\nSending candlePublish val "));
  Serial.print(value==1? "1" : "0");
  Serial.print("...");
  for(int i=0; i<1; i++){
    if (! candlePublish.publish(value==1? "1" : "0")) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
    delay(7);
  }
}

