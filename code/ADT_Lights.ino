// Arduino IDE settings: 

//               Board: ESP32 Dev Module
//          Flash size: 8MB
//    Partition Scheme: 8MB with SPIFFS
//               PSRAM: Enabled



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

/////   Code and Language Setup

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////// || Includes || /////////////////////////
#include <APA102.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <MCP7940.h>
#include <NetworkUdp.h>
#include <Preferences.h>
#include <SparkFun_Alphanumeric_Display.h> //Click here to get the library: http://librarymanager/All#SparkFun_Qwiic_Alphanumeric_Display by SparkFun
#include <SPIFFS.h>
#include <WiFi.h>
#include <Wire.h>


///////////////////////// || Defines || /////////////////////////
#define DB_PIN 0
#define LED_UV 26
#define LED_R 25
#define LED_G 33
#define LED_B 32 
#define PWM_FREQ 5000     // Frequency in Hz
#define PWM_RESOLUTION 8  // 8-bit resolution (0 - 255)

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define NUM_STATES  9  // number of patterns to cycle through
#define LED_COUNT 30
#define RECSTR_LIMIT  64
#define BLE_STR_TIME  500 // time limit between characters determines inclusion in string from ble

#define TIMEINDAY   86400   // seconds in a day

#define NUM_FADERS 7
#define STRIP_R   0
#define STRIP_G   1
#define STRIP_B   2
#define AMBI_R    3
#define AMBI_G    4
#define AMBI_B    5
#define AMBI_UV   6



///////////////////////// || Variables || /////////////////////////


// ADT Variables

unsigned long on_sec, off_sec;    // Calculated times of day (in seconds) for seasonally adjusted sunrise and sunset.

uint8_t DayBrightness[NUM_FADERS];       // "Full" brightness for this color in daytime, after fading up.
unsigned int FadeSeconds[NUM_FADERS];   // How many seconds for the fade? (On / Off times determined by ADT [on_sec and off_sec times are at 100% Daybrights])
double DutyCycles[NUM_FADERS];       // Current duty cycles for each fader

unsigned int amplitude = 14400;   // 4 hrs
unsigned int avgday = 43200;      // 12 hrs
int offset = 7200;       // 2 hrs

unsigned int last_day = 0;
unsigned int last_month = 0;

int psRAMsize, psRAMfree;

uint8_t stripDelay = 1;

const char* htmlPath = "/index.html";
//const char* dataPath = "/data.json";

String files_json = "Default Name";

HT16K33 display;

Preferences prefs;

bool WifiOK = false;

int anInt;
int anotherInt;
char aChar;
float aFloat;
float aFloat2;
float aFloat3;
float aFloat4;
String aString;

//const uint32_t SERIAL_SPEED{115200};     // Set the baud rate for Serial I/O
//const uint8_t  LED_PIN{13};              // Arduino built-in LED pin number
const uint8_t  SPRINTF_BUFFER_SIZE{32};  // Buffer size for sprintf()
MCP7940_Class MCP7940;                           // Create an instance of the MCP7940
char          inputBuffer[SPRINTF_BUFFER_SIZE];  // Buffer for sprintf()/sscanf()
DateTime       nowRTC;
uint8_t       last_sec, this_sec;


unsigned int bootCnt=0;

rgb_color colors[LED_COUNT*2];

String ssid;
String password;
AsyncWebServer server(80);    // Create AsyncWebServer object on port 80


uint8_t spot_led = 0;

const uint8_t dataPin = 13;
const uint8_t clockPin = 14;
APA102<dataPin, clockPin> ledStrip;

uint8_t globalBrightness = 31;    // Set the brightness to use (the maximum is 31).


unsigned int maxLoops;
unsigned int loopCount = 0;   // system timer, incremented by one every time through the main loop
unsigned int seed = 0;  // used to initialize random number generator



enum Pattern {      // enumerate the functional modes
  AnnualDaylightTimer = 0,
  WarmWhiteShimmer = 1,
  RandomColorWalk = 2,
  TraditionalColors = 3,
  ColorExplosion = 4,
  Gradient = 5,
  BrightTwinkle = 6,
  Collision = 7,
  AllBright = 8,
  SingleSpot = 9,
  AllManual = 10,
  AllOff = 255
};
unsigned char pattern = AnnualDaylightTimer;


String RecString;
unsigned long last_rec = 0; // time (millis) of last receive for determining string inclusion.  0 = not receiving a string.
unsigned long this_rec = 0;
unsigned char Received = 0;   // Single char receive buffer
uint8_t str_cnt = 0;          // Counter for string mode
uint8_t led_timer = 0;      // Convert from waiting 20 ms to counting 10 waits of 2 ms...


BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;



///////////////////////// || BLE Classes || /////////////////////////

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
        Received = rxValue[i];
      }

      //Serial.println();
      //Serial.println("*********");
    }
  }
};


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

/////   Setup and Loop Functions

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);


  Serial.print(F("- Compiled with c++ version "));
  Serial.print(F(__VERSION__));  // Show compiler information
  Serial.print(F("\n- On "));
  Serial.print(F(__DATE__));
  Serial.print(F(" at "));
  Serial.print(F(__TIME__));
  Serial.print(F("\n"));


  ////////////////////// Initialize SPIFFS /////////////////////////////////////
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }


  /////////////// I2C Stuff ////////////////////////////////////////////////
  Wire.begin(15, 5);  // ESP32 Qwiic Pro Mini I2C pins.

  if (display.begin() == false)
  {
    Serial.println("Device did not acknowledge! Freezing.");
    while (1);
  }
  Serial.println("Display acknowledged.");
  
  display.setBrightness(1);   // ints between 0 (1/16 brightness) and 15 (full brightness)
  
  display.print("ADT ");
  

  ////////////// Preferences //////////////////////////////////
  prefs.begin("LED_Bar_data", false);

    prefs.getUInt("bootCnt", bootCnt);
    bootCnt++;
    prefs.putUInt("bootCnt", bootCnt);

  prefs.end();

  prefs.begin("Wifi", false);
    ssid = prefs.getString("ssid", " ");
    password = prefs.getString("password", " ");
  prefs.end();  


  prefs.begin("ADT", false);
    amplitude = prefs.getUInt("amplitude", 14400);
    avgday = prefs.getUInt("avgday", 43200);
    offset = prefs.getUInt("offset", 0);
  prefs.end();

  prefs.begin("faders", false);
    DayBrightness[STRIP_R] = prefs.getUInt("stripRday", 200);    
    DayBrightness[STRIP_G] = prefs.getUInt("stripGday", 180);    
    DayBrightness[STRIP_B] = prefs.getUInt("stripBday", 150);    
  
    DayBrightness[AMBI_R] = prefs.getUInt("ambiRday", 200);    
    DayBrightness[AMBI_G] = prefs.getUInt("ambiGday", 180);    
    DayBrightness[AMBI_B] = prefs.getUInt("ambiBday", 150);    
    DayBrightness[AMBI_UV] = prefs.getUInt("ambiUVday", 0);    
  
    FadeSeconds[STRIP_R] = prefs.getUInt("stripRfade", 2000);    
    FadeSeconds[STRIP_G] = prefs.getUInt("stripGfade", 1800);    
    FadeSeconds[STRIP_B] = prefs.getUInt("stripBfade", 1500);    
  
    FadeSeconds[AMBI_R] = prefs.getUInt("ambiRfade", 2000);    
    FadeSeconds[AMBI_G] = prefs.getUInt("ambiGfade", 1800);    
    FadeSeconds[AMBI_B] = prefs.getUInt("ambiBfade", 1500);    
    FadeSeconds[AMBI_UV] = prefs.getUInt("ambiUVfade", 1800);    
  
    for(int i=0; i < NUM_FADERS; i++) DutyCycles[i] = 0;
  prefs.end();


  //////////////// LED Lighting ///////////////////////////////////////////
  // initialize the random number generator with a seed obtained by
  // summing the voltages on the disconnected analog inputs
  for (int i = 0; i < 8; i++)
  {
    seed += analogRead(i);
  }
  seed += random(256);
  randomSeed(seed);
/*
  for(int i=0; i < NUM_FADERS; i++) {    // Set initial conditions for faders
    DayBrightness[i] = 200;
    FadeSeconds[i] = 400;
    DutyCycles[i] = 0;
  }
  DayBrightness[AMBI_UV] = 0;     // Return the UV to off;
  
  FadeSeconds[STRIP_R] =  2400;
  FadeSeconds[STRIP_G] =  1800;
  DayBrightness[STRIP_B] = 100;
  DayBrightness[STRIP_G] = 100;
*/
  pinMode(DB_PIN, OUTPUT);

  ledcAttach(LED_UV, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(LED_R, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(LED_G, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(LED_B, PWM_FREQ, PWM_RESOLUTION);


  delay(10);  // give pull-ups time raise the input voltage





  ////////////////// BLE //////////////////////////////////////////////

  // Create the BLE Device
  BLEDevice::init("ESP32 LED Bar");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");


  
  /////////////// Wifi /////////////////////////////////////////////////

  WiFi.mode(WIFI_STA);
  
  WiFi.begin(ssid, password);

  Serial.println("Creds:");
  Serial.println(ssid);
  Serial.println(password);
  Serial.println("-Creds:");

  for(int i = 0; i < 3; i++) {
    if(WiFi.waitForConnectResult() != WL_CONNECTED) delay(100);
    else WifiOK = true;
  } 

  if(WifiOK) {
    
    
    String ipStr = WiFi.localIP().toString();


    // Split the IP into its individual octets
    int firstDot = ipStr.indexOf('.');
    int secondDot = ipStr.indexOf('.', firstDot + 1);
    int thirdDot = ipStr.indexOf('.', secondDot + 1);

    String octet1 = ipStr.substring(0, firstDot);
    String octet2 = ipStr.substring(firstDot + 1, secondDot);
    String octet3 = ipStr.substring(secondDot + 1, thirdDot);
    String octet4 = ipStr.substring(thirdDot + 1);


    display.print(octet1);
    delay(1000);
    display.print(octet2);
    delay(1000);
    display.print(octet3);
    delay(1000);
    display.print(octet4);
    delay(1000);

  }




  ////////////// RTC /////////////////////////////////////////

  while (!MCP7940.begin()) {  // Initialize RTC communications
    Serial.println(F("Unable to find MCP7940M. Checking again in 3s."));  // Show error text
    delay(3000);                                                          // wait a second
  }  // of loop until device is located
  Serial.println(F("MCP7940 initialized."));
  while (!MCP7940.deviceStatus()) {  // Turn oscillator on if necessary
    Serial.println(F("Oscillator is off, turning it on."));
    bool deviceStatus = MCP7940.deviceStart();  // Start oscillator and return state
    if (!deviceStatus) {                        // If it didn't start
      Serial.println(F("Oscillator did not start, trying again."));  // Show error and
      delay(500);                                                   // wait for a second
    }                // of if-then oscillator didn't start
  }                  // of while the oscillator is off
  
  MCP7940.adjust();       // Try to get the time from the network
  nowRTC = MCP7940.now();  // get the current time
  ReCalcADT();              // Use current time to get initial values for ADT


/////////////// OTA /////////////////////////////////////////////////

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  if(WifiOK) ArduinoOTA.begin();
 
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());



////////////////////// WEB SERVER /////////////////////////////////

  if(WifiOK)
  {

    // Serve the image
    server.on("/Faders.jpg", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/Faders.jpg", "image/jpeg");
    });
    // Serve the image
    server.on("/Faders-off.jpg", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/Faders-off.jpg", "image/jpeg");
    });

    // Serve HTML file from SPIFFS
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, htmlPath, "text/html");
    });

    // Serve faders.html
    server.on("/faders.html", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/faders.html", "text/html");
    });

    // Endpoint to get the list of files
    server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request){
      String json = listFiles();
     //Serial.println("Sending JSON: " + json); // Log the JSON
      files_json = json;
      request->send(200, "application/json", json);
    });

    // Endpoint to send dynamic JSON data
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
      StaticJsonDocument<5000> jsonDoc;

      jsonDoc["PSRAM Size"] = psRAMsize;
      jsonDoc["PSRAM Free"] = psRAMfree;
      jsonDoc["PSRAM Used"] = psRAMsize - psRAMfree;
      jsonDoc["Boot Count"] = bootCnt;
      jsonDoc["Year"] = nowRTC.year();
      jsonDoc["Month"] = nowRTC.month();
      jsonDoc["Day"] = nowRTC.day();
      jsonDoc["Hour"] = nowRTC.hour();
      jsonDoc["Minute"] = nowRTC.minute();
      jsonDoc["Second"] = nowRTC.second();
      jsonDoc["AvgDay"] = avgday;
      jsonDoc["amplitude"] = amplitude;
      jsonDoc["offset"] = offset;
      jsonDoc["on_sec"] = on_sec;
      jsonDoc["off_sec"] = off_sec;
/*      jsonDoc["globalBrightness"] = globalBrightness;
      jsonDoc["Received"] = String(Received);
      jsonDoc["RecString"] = RecString;
      jsonDoc["str_cnt"] = str_cnt;*/
      jsonDoc["FadeSeconds[STRIP_R]"] = FadeSeconds[STRIP_R];
      jsonDoc["FadeSeconds[STRIP_G]"] = FadeSeconds[STRIP_G];
      jsonDoc["FadeSeconds[STRIP_B]"] = FadeSeconds[STRIP_B];
      jsonDoc["FadeSeconds[AMBI_R] (white)"] = FadeSeconds[AMBI_R];
      //jsonDoc["FadeSeconds[AMBI_G]"] = FadeSeconds[AMBI_G];
      //jsonDoc["FadeSeconds[AMBI_B]"] = FadeSeconds[AMBI_B];
      jsonDoc["FadeSeconds[AMBI_UV]"] = FadeSeconds[AMBI_UV];
/*      jsonDoc["Pattern"] = pattern;
      jsonDoc["An Int"] = anInt;
      jsonDoc["Another Int"] = anotherInt;
      jsonDoc["A Char"] = String(aChar);
      jsonDoc["A String"] = aString;
      jsonDoc["A Float"] = aFloat;
      jsonDoc["A Float2"] = aFloat2;
      jsonDoc["A Float3"] = aFloat3;
      jsonDoc["A Float4"] = aFloat4;*/

      String jsonData;
      serializeJson(jsonDoc, jsonData);
     
      request->send(200, "application/json", jsonData);
    });


    // Handle button request
    server.on("/runCode", HTTP_GET, [](AsyncWebServerRequest *request){
        // Run your ESP32 code here
        Serial.println("Button was clicked! Running ESP32 code...");

        MCP7940.adjust();

        // Send response to client
        request->send(200, "text/plain", "ESP32 code executed!");
    });


    // Handle file uploads
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200);
      request->redirect("/");  // Redirect the user back to the index.html after submission
    }, handleFileUpload);


    // Handle form submission
    server.on("/submit-LED", HTTP_POST, handleFormSubmit);

    // Handle form submission
    server.on("/submit-time", HTTP_POST, handleTimeSubmit);

    // Handle form submission
    server.on("/submit-date", HTTP_POST, handleDateSubmit);

    // Handle form submission
    server.on("/submit-ADT", HTTP_POST, handleADTSubmit);

    // Handle form submission
    server.on("/faders-update", HTTP_POST, handleFadersSubmit);


    // Start server
    server.begin();
    Serial.println("Web Server Started");

  }

}


///////////////////////////////////////////////////////////// main loop /////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////// main loop /////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////// main loop /////////////////////////////////////////////////////////////////////////
void loop()
{
  uint8_t startTime = millis();  
  psRAMsize = ESP.getPsramSize();
  psRAMfree = ESP.getFreePsram();

  if(WifiOK) ArduinoOTA.handle();

  if(led_timer > stripDelay) {

    ///////////////////// RTC and Clock Display //////////////////////////////
    nowRTC = MCP7940.now();  // get the current time
    this_sec = nowRTC.second();
    
    if(this_sec != last_sec) {    
      last_sec = this_sec;
      if((last_sec % 2) != 0) sprintf(inputBuffer, "%02d:%02d", nowRTC.hour(), nowRTC.minute());
      else sprintf(inputBuffer, "%02d%02d", nowRTC.hour(), nowRTC.minute());
      display.print(inputBuffer);
    }

    //////////////////// LED Strip /////////////////////////////////////////////////////////////
    if (loopCount == 0) {
      for (int i = 0; i < LED_COUNT; i++) {
        colors[i] = rgb_color(0, 0, 0);     // whenever timer resets, clear the LED colors array (all off)
      }
    }    
    
    process_patterns();

    for(int i=0; i < LED_COUNT; i++) colors[LED_COUNT+i] = colors[LED_COUNT-i-1];  // Mirror the half populated strip (U-turn)
    ledStrip.write(colors, LED_COUNT*2, globalBrightness);
    
    digitalWrite(DB_PIN, HIGH);
    ledcWrite(LED_UV, DutyCycles[AMBI_UV]);
    ledcWrite(LED_R, DutyCycles[AMBI_R]);
    ledcWrite(LED_G, DutyCycles[AMBI_G]);
    ledcWrite(LED_B, DutyCycles[AMBI_B]);


    ////////////////// Handle BLE Messages ////////////////////////////////////////
    if(last_rec != 0) this_rec = millis();
    else this_rec = 0;
    if((this_rec - last_rec) > BLE_STR_TIME) {
      last_rec = 0;   
      CommandString(RecString);
    }

    if(Received != 0) {
      if(last_rec == 0) {
        CommandChar(Received);
        Received = 0;
      }
      else {
        if (RecString.length() <= str_cnt) {      // Append enough characters manually to extend the length
          while (RecString.length() <= str_cnt) {
            RecString += ' ';  // Add space characters to expand the string
          }
        }

        RecString.setCharAt(str_cnt, Received);
        Received = 0;
        str_cnt++;
        last_rec = this_rec;
      }
    }
  }

  ////////////////// BLE Processing ////////////////////////////
  if (deviceConnected) {
    pTxCharacteristic->setValue(&txValue, 1);
    pTxCharacteristic->notify();
    txValue++;
    delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
  }

  if (!deviceConnected && oldDeviceConnected) {   // disconnecting
    //delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    //Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {   // connecting
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }




  //////////////// TIMING //////////////////////////////////////
  if(led_timer > stripDelay) led_timer = 0;
  while((uint8_t)(millis() - startTime) < 1) { }
  led_timer++;
  if(led_timer > stripDelay) loopCount++;  // increment our loop counter/timer.

  digitalWrite(DB_PIN, LOW);
}


























////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

/////   Functions

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


void ReCalcADT(void) {
  float yearFraction;
  float now_amplitude;
  float on_time, off_time;
  
  switch(nowRTC.month())
  {
    case 1:
      yearFraction = nowRTC.day() - 1.0;
      break;
    case 2:
      yearFraction = nowRTC.day() + 30.0;
      break;
    case 3:
      yearFraction = nowRTC.day() + 58.0;
      break;
    case 4:
      yearFraction = nowRTC.day() + 89.0;
      break;
    case 5:
      yearFraction = nowRTC.day() + 119.0;
      break;
    case 6:
      yearFraction = nowRTC.day() + 150.0;
      break;
    case 7:
      yearFraction = nowRTC.day() + 180.0;
      break;
    case 8:
      yearFraction = nowRTC.day() + 211.0;
      break;
    case 9:
      yearFraction = nowRTC.day() + 242.0;
      break;
    case 10:
      yearFraction = nowRTC.day() + 272.0;
      break;
    case 11:
      yearFraction = nowRTC.day() + 303.0;
      break;
    case 12:
      yearFraction = nowRTC.day() + 333.0;
      break;
  }

  yearFraction = yearFraction / 365.0;

  now_amplitude = cos( 2 * PI * yearFraction) * amplitude + avgday;

  on_time = TIMEINDAY / 2.0 - now_amplitude / 2.0;
  off_time = on_time + now_amplitude;

  on_sec = round(on_time) + offset;
  off_sec = round(off_time) + offset;  
}


void process_patterns(void) {
  if (pattern == WarmWhiteShimmer || pattern == RandomColorWalk)
  {
    // for these two patterns, we want to make sure we get the same
    // random sequence six times in a row (this provides smoother
    // random fluctuations in brightness/color)
    if (loopCount % 6 == 0)
    {
      seed = random(30000);
    }
    randomSeed(seed);
  }

  // call the appropriate pattern routine based on state; these
  // routines just set the colors in the colors array
  switch (pattern)
  {
    case AnnualDaylightTimer:
      int secofday;
      float fade_level;
      int i;

      if( (nowRTC.day() != last_day) || (nowRTC.month() != last_month) ) {    // Do a ReCalc if the day or month has changed
        last_day = nowRTC.day();
        last_month = nowRTC.month();
        ReCalcADT();
      }

      secofday = nowRTC.hour() * 60 * 60 + nowRTC.minute() * 60 + nowRTC.second();

      for( i = 0; i < NUM_FADERS; i++ ) {
        // determine ON-ness via slope of %max-duty per second
        fade_level = ((float)DayBrightness[i]/255.0) / (float)FadeSeconds[i] * ((float)secofday - (float)on_sec) + ((float)DayBrightness[i]/255.0);
        DutyCycles[i] = round(fade_level * 255.0);

        // determine OFF-ness via slope of %max-duty per second
        if(secofday > on_sec + 1) {           // Let the Fade in do it's thing first
          fade_level = ((float)DayBrightness[i] / -255.0) / (float)FadeSeconds[i] * ((float)secofday - (float)off_sec) + ((float)DayBrightness[i]/255.0);
          DutyCycles[i] = round(fade_level * 255.0);
        }

        if( DutyCycles[i]  > DayBrightness[i] ) DutyCycles[i] = DayBrightness[i];
        if( DutyCycles[i] < 0) DutyCycles[i] = 0;
      }
  
      for( i = 0; i < LED_COUNT; i++ ) {
        colors[i] = rgb_color(DutyCycles[STRIP_R], DutyCycles[STRIP_G], DutyCycles[STRIP_B]);
      }
      break;

    case WarmWhiteShimmer:
      // warm white shimmer for 300 loopCounts, fading over last 70
      maxLoops = 300;
      warmWhiteShimmer(loopCount > maxLoops - 70);
      if(loopCount > maxLoops) loopCount = 0; // continue forever
      break;

    case RandomColorWalk:
      // start with alternating red and green colors that randomly walk
      // to other colors for 400 loopCounts, fading over last 80
      maxLoops = 400;
      randomColorWalk(loopCount == 0 ? 1 : 0, loopCount > maxLoops - 80);
      if(loopCount > maxLoops) loopCount = 0; // continue forever
      break;

    case TraditionalColors:
      // repeating pattern of red, green, orange, blue, magenta that
      // slowly moves for 400 loopCounts
      maxLoops = 400;
      traditionalColors();
      break;

    case ColorExplosion:
      // bursts of random color that radiate outwards from random points
      // for 630 loop counts; no burst generation for the last 70 counts
      // of every 200 count cycle or over the over final 100 counts
      // (this creates a repeating bloom/decay effect)
      maxLoops = 630;
      colorExplosion((loopCount % 200 > 130) || (loopCount > maxLoops - 100));
      if(loopCount > maxLoops) loopCount = 0; // continue forever
      break;

    case Gradient:
      // red -> white -> green -> white -> red ... gradiant that scrolls
      // across the strips for 250 counts; this pattern is overlaid with
      // waves of dimness that also scroll (at twice the speed)
      maxLoops = 250;
      gradient();
      //delay(6);  // add an extra 6ms delay to slow things down
      break;

    case BrightTwinkle:
      // random LEDs light up brightly and fade away; it is a very similar
      // algorithm to colorExplosion (just no radiating outward from the
      // LEDs that light up); as time goes on, allow progressively more
      // colors, halting generation of new twinkles for last 100 counts.
      maxLoops = 1200;
      if (loopCount < 400)
      {
        brightTwinkle(0, 1, 0);  // only white for first 400 loopCounts
      }
      else if (loopCount < 650)
      {
        brightTwinkle(0, 2, 0);  // white and red for next 250 counts
      }
      else if (loopCount < 900)
      {
        brightTwinkle(1, 2, 0);  // red, and green for next 250 counts
      }
      else
      {
        // red, green, blue, cyan, magenta, yellow for the rest of the time
        brightTwinkle(1, 6, loopCount > maxLoops - 100);
      }
      if(loopCount > maxLoops) loopCount = 0; // continue forever
      break;

    case Collision:
      // colors grow towards each other from the two ends of the strips,
      // accelerating until they collide and the whole strip flashes
      // white and fades; this repeats until the function indicates it
      // is done by returning 1, at which point we stop keeping maxLoops
      // just ahead of loopCount
      if (!collision())
      {
        maxLoops = loopCount + 2;
      }
      break;

      case AllBright:
        for (int i = 0; i < LED_COUNT; i++)
        {
          colors[i] = rgb_color(255, 255, 255);
        }
        globalBrightness = 31;
        break;

      case AllOff:
        for (int i = 0; i < LED_COUNT; i++) {
          colors[i] = rgb_color(0, 0, 0);
        }
        for (int i = 0; i < NUM_FADERS; i++) {
          DutyCycles[i] = 0;
        }
        globalBrightness = 31;
        break;

/*      case AllManual:
        for (int i = 0; i < LED_COUNT; i++) {
          colors[i] = rgb_color();
        }
        for (int i = 0; i < NUM_FADERS; i++) {
          DutyCycles[i] = ;
        }
        globalBrightness = 31;
        break;
*/
      case SingleSpot:
        for (int i = 0; i < LED_COUNT; i++)
        {
          colors[i] = rgb_color(0, 0, 0); // Black out everything
        }
        //globalBrightness = 31;
        colors[spot_led] = rgb_color(255,255,255);
        break;
  }

}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandChar(char cmd) {
  
  switch( cmd )
  {
    case 'B':
      globalBrightness++;
      if(globalBrightness > 31) globalBrightness = 31;
      break;

    case 'b':
      globalBrightness--;
      if(globalBrightness > 31) globalBrightness = 0;   //looks wrong, actually correct.  there is no < 0, there is only roll over to 256
      break;
      
    case 'n':
      loopCount = 0;
      pattern = ((unsigned char)(pattern+1))%NUM_STATES;
      break;
      
    case 'S':
      spot_led++;
      if(spot_led >= LED_COUNT) spot_led = LED_COUNT-1;
      break;
      
    case 's':
      spot_led--;
      if(spot_led > LED_COUNT) spot_led = 0;
      break;
      
    case 't':
      MCP7940.adjust();
      break;
      
    case '.':       // [dot + code + string] to use string mode
      last_rec = millis();
      str_cnt = 0;
      break;
      
  }

}




/////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandString(String codeString) {

  char StrCommand = codeString.charAt(0);
  
  aString = codeString;
  //aChar = StrCommand;
  
  switch( StrCommand )
  {
    case 's':     // Save SSID string
      prefs.begin("Wifi", false);
      prefs.remove("ssid");
      prefs.putString("ssid", codeString.substring(1));
      prefs.end();
      break;

    case 'p':     // Save Password string
      prefs.begin("Wifi", false);
      prefs.remove("password");
      prefs.putString("password", codeString.substring(1));
      prefs.end();
      delay(100);
      ESP.restart();
      break;

    case 'b':     // Adjust Brightness
      globalBrightness = codeString.substring(1).toInt();
      break;

    case 'y':     // Set Year
      MCP7940.adjust(DateTime(codeString.substring(1).toInt(), nowRTC.month(), nowRTC.day(), nowRTC.hour(), nowRTC.minute(), nowRTC.second()));
//      ReCalcADT();    // Times have changed, recalculate
      break;

    case 'm':     // Set Month
      MCP7940.adjust(DateTime(nowRTC.year(), codeString.substring(1).toInt(), nowRTC.day(), nowRTC.hour(), nowRTC.minute(), nowRTC.second()));
      ReCalcADT();    // Times have changed, recalculate
      break;

    case 'd':     // Set Day
      MCP7940.adjust(DateTime(nowRTC.year(), nowRTC.month(), codeString.substring(1).toInt(), nowRTC.hour(), nowRTC.minute(), nowRTC.second()));
      ReCalcADT();    // Times have changed, recalculate
      break;

    case 'h':     //Set Hour
      MCP7940.adjust(DateTime(nowRTC.year(), nowRTC.month(), nowRTC.day(), codeString.substring(1).toInt(), nowRTC.minute(), nowRTC.second()));
      ReCalcADT();    // Times have changed, recalculate
      break;

    case 'M':     //Set Minute
      MCP7940.adjust(DateTime(nowRTC.year(), nowRTC.month(), nowRTC.day(), nowRTC.hour(), codeString.substring(1).toInt(), nowRTC.second()));
        ReCalcADT();    // Times have changed, recalculate
    break;

    case 'S':     //Set Second
      MCP7940.adjust(DateTime(nowRTC.year(), nowRTC.month(), nowRTC.day(), nowRTC.hour(), nowRTC.minute(), codeString.substring(1).toInt()));
      ReCalcADT();    // Times have changed, recalculate
      break;

    case 'a':     //Set ADT Amplitude
      amplitude = codeString.substring(1).toInt();
      prefs.begin("ADT", false);
      prefs.remove("amplitude");
      prefs.putUInt("amplitude", amplitude);
      prefs.end();
      break;

    case 'A':     //Set ADT avgday
      avgday =  codeString.substring(1).toInt();
      prefs.begin("ADT", false);
      prefs.remove("avgday");
      prefs.putUInt("avgday", avgday);
      prefs.end();
      break;

    case 'o':     //Set ADT offset
      offset =  codeString.substring(1).toInt();
      prefs.begin("ADT", false);
      prefs.remove("offset");
      prefs.putUInt("offset", offset);
      prefs.end();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
String listFiles() {
  // Get SPIFFS memory info
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  size_t remainingBytes = totalBytes - usedBytes;

  // Start building the JSON output
  String output = "{";
  
  // Add memory info
  output += "\"memory\": {";
  output += "\"total\": " + String(totalBytes) + ",";
  output += "\"used\": " + String(usedBytes) + ",";
  output += "\"remaining\": " + String(remainingBytes);
  output += "},";

  // Add file list
  output += "\"files\": [";

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  bool firstFile = true; // Track if it's the first file
  
  while (file) {
    if (!firstFile) {
      output += ','; // Add a comma only if it's not the first file
    }
    output += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
    firstFile = false; // Mark that we've processed the first file
    file = root.openNextFile();
  }

  output += "]";
  output += "}";
  
  return output;
}

/////////////// Function to handle form submission /////////////////////////////////////////////////////////////////////////
void handleFormSubmit(AsyncWebServerRequest *request) {
  // Get the values from the form
  if (request->hasParam("codeString", true)) {
    RecString = request->getParam("codeString", true)->value();
    CommandString(RecString);
  }
  if (request->hasParam("Pattern", true)) {
    pattern = request->getParam("Pattern", true)->value().toInt();
  }
  if (request->hasParam("globalBrightness", true)) {
    globalBrightness = request->getParam("globalBrightness", true)->value().toInt();
  }
  if (request->hasParam("duty_W", true)) {
    DutyCycles[AMBI_R] = request->getParam("duty_W", true)->value().toInt();
  }
  /*if (request->hasParam("duty_G", true)) {
    DutyCycles[AMBI_G] = request->getParam("duty_G", true)->value().toInt();
  }
  if (request->hasParam("duty_B", true)) {
    DutyCycles[AMBI_B] = request->getParam("duty_B", true)->value().toInt();
  }*/
  if (request->hasParam("duty_UV", true)) {
    DutyCycles[AMBI_UV] = request->getParam("duty_UV", true)->value().toInt();
  }
  if (request->hasParam("duty_R", true)) {
    DutyCycles[STRIP_R] = request->getParam("duty_R", true)->value().toInt();
  }
  if (request->hasParam("duty_G", true)) {
    DutyCycles[STRIP_G] = request->getParam("duty_G", true)->value().toInt();
  }
  if (request->hasParam("duty_B", true)) {
    DutyCycles[STRIP_B] = request->getParam("duty_B", true)->value().toInt();
  }

  if (request->hasParam("stripDelay", true)) {
    stripDelay = request->getParam("stripDelay", true)->value().toInt();
  }

  // Redirect the user back to the index.html after submission
  request->redirect("/");

  for(int i = 0; i < LED_COUNT; i++ ) colors[i] = rgb_color(DutyCycles[STRIP_R], DutyCycles[STRIP_G], DutyCycles[STRIP_B]);
 
}


/////////////// Function to handle Time form submission /////////////////////////////////////////////////////////////////////////
void handleDateSubmit(AsyncWebServerRequest *request) {

  uint8_t new_year, new_month, new_day;

  // Get the values from the form
  if (request->hasParam("year", true)) {
    new_year = request->getParam("year", true)->value().toInt();
  }
  if (request->hasParam("month", true)) {
    new_month = request->getParam("month", true)->value().toInt();
  }
  if (request->hasParam("day", true)) {
    new_day = request->getParam("day", true)->value().toInt();
  }

  // Redirect the user back to the index.html after submission
  request->redirect("/");
 
  MCP7940.adjust(DateTime(new_year, new_month, new_day, nowRTC.hour(), nowRTC.minute(), nowRTC.second()));
  ReCalcADT();

}


/////////////// Function to handle Date form submission /////////////////////////////////////////////////////////////////////////
void handleTimeSubmit(AsyncWebServerRequest *request) {

  uint8_t new_hour, new_min, new_sec;

  // Get the values from the form
  if (request->hasParam("hour", true)) {
    new_hour = request->getParam("hour", true)->value().toInt();
  }
  if (request->hasParam("minute", true)) {
    new_min = request->getParam("minute", true)->value().toInt();
  }
  if (request->hasParam("second", true)) {
    new_sec = request->getParam("second", true)->value().toInt();
  }

  // Redirect the user back to the index.html after submission
  request->redirect("/");
 
  MCP7940.adjust(DateTime(nowRTC.year(), nowRTC.month(), nowRTC.day(), new_hour, new_min, new_sec));
  ReCalcADT();

}




/////////////// Function to handle ADT form submission /////////////////////////////////////////////////////////////////////////
void handleADTSubmit(AsyncWebServerRequest *request) {

  int new_avgday, new_amplitude, new_offset;
  String fieldString;

  // Get the values from the form
  if (request->hasParam("avgday", true)) {
    fieldString = request->getParam("avgday", true)->value();
    if(fieldString.length() != 0) {
      new_avgday = round(fieldString.toFloat() * 3600.0);
      avgday = new_avgday;
    }
  }
  if (request->hasParam("amplitude", true)) {
     fieldString = request->getParam("amplitude", true)->value();
     if(fieldString.length() != 0) {
      new_amplitude = round(fieldString.toFloat() * 3600.0);
      amplitude = new_amplitude;
     }
  }
  if (request->hasParam("offset", true)) {
     fieldString = request->getParam("offset", true)->value();
     if(fieldString.length() != 0) {
      new_offset = round(fieldString.toFloat() * 3600.0);
      offset = new_offset;
     }
  }
  
  // Redirect the user back to the index.html after submission
  request->redirect("/");
 

  ReCalcADT();


  prefs.begin("ADT", false);
  prefs.clear();

  prefs.putUInt("amplitude", amplitude);
  prefs.putUInt("avgday", avgday);
  prefs.putUInt("offset", offset);

  prefs.end();


}




/////////////// Function to handle Faders form submission /////////////////////////////////////////////////////////////////////////
void handleFadersSubmit(AsyncWebServerRequest *request) {

  String fieldString;

  if (request->hasParam("stripRday", true)) {
    fieldString = request->getParam("stripRday", true)->value();
    if(fieldString.length() != 0) DayBrightness[STRIP_R] = fieldString.toInt();
  }
  if (request->hasParam("stripGday", true)) {
     fieldString = request->getParam("stripGday", true)->value();
     if(fieldString.length() != 0) DayBrightness[STRIP_G] = fieldString.toInt();
  }
  if (request->hasParam("stripBday", true)) {
     fieldString = request->getParam("stripBday", true)->value();
     if(fieldString.length() != 0) DayBrightness[STRIP_B] = fieldString.toInt();
  }


  if (request->hasParam("ambiWday", true)) {
     fieldString = request->getParam("ambiWday", true)->value();
     if(fieldString.length() != 0) DayBrightness[AMBI_R] = fieldString.toInt();
  }
  /*if (request->hasParam("ambiGday", true)) {
     fieldString = request->getParam("ambiGday", true)->value();
     if(fieldString.length() != 0) DayBrightness[AMBI_G] = fieldString.toInt();
  }
  if (request->hasParam("ambiBday", true)) {
     fieldString = request->getParam("ambiBday", true)->value();
     if(fieldString.length() != 0) DayBrightness[AMBI_B] = fieldString.toInt();
  }*/
  if (request->hasParam("ambiUVday", true)) {
     fieldString = request->getParam("ambiUVday", true)->value();
     if(fieldString.length() != 0) DayBrightness[AMBI_UV] = fieldString.toInt();
  }
  

  if (request->hasParam("stripRfade", true)) {
     fieldString = request->getParam("stripRfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[STRIP_R]  = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[STRIP_R] < 1 ) FadeSeconds[STRIP_R] = 1;  // prevent devide by 0 and negative slopes
  }
  if (request->hasParam("stripGfade", true)) {
     fieldString = request->getParam("stripGfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[STRIP_G]  = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[STRIP_G] < 1 ) FadeSeconds[STRIP_G] = 1;  // prevent devide by 0 and negative slopes
  }
  if (request->hasParam("stripBfade", true)) {
     fieldString = request->getParam("stripBfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[STRIP_B]  = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[STRIP_B] < 1 ) FadeSeconds[STRIP_B] = 1;  // prevent devide by 0 and negative slopes
  }



  if (request->hasParam("ambiWfade", true)) {
     fieldString = request->getParam("ambiWfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[AMBI_R] = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[AMBI_R] < 1 ) FadeSeconds[AMBI_R] = 1;  // prevent devide by 0 and negative slopes
  }
/*  if (request->hasParam("ambiGfade", true)) {
     fieldString = request->getParam("ambiGfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[AMBI_G] = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[AMBI_G] < 1 ) FadeSeconds[AMBI_G] = 1;  // prevent devide by 0 and negative slopes
  }
  if (request->hasParam("ambiBfade", true)) {
     fieldString = request->getParam("ambiBfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[AMBI_B] = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[AMBI_B] < 1 ) FadeSeconds[AMBI_B] = 1;  // prevent devide by 0 and negative slopes
  } */
  if (request->hasParam("ambiUVfade", true)) {
     fieldString = request->getParam("ambiUVfade", true)->value();
     if(fieldString.length() != 0) FadeSeconds[AMBI_UV] = round(fieldString.toFloat() * 60.0);
     if( FadeSeconds[AMBI_UV] < 1 ) FadeSeconds[AMBI_UV] = 1;  // prevent devide by 0 and negative slopes
  }
  


  // Redirect the user back to the index.html after submission
  request->redirect("/");
 
  ReCalcADT();

  prefs.begin("faders", false);
  prefs.clear();

  prefs.putUInt("stripRday", DayBrightness[STRIP_R]);
  prefs.putUInt("stripGday", DayBrightness[STRIP_G]);
  prefs.putUInt("stripBday", DayBrightness[STRIP_B]);

  prefs.putUInt("ambiRday", DayBrightness[AMBI_R]);
  prefs.putUInt("ambiGday", DayBrightness[AMBI_G]);
  prefs.putUInt("ambiBday", DayBrightness[AMBI_B]);
  prefs.putUInt("ambiUVday", DayBrightness[AMBI_UV]);


  prefs.putUInt("stripRfade", FadeSeconds[STRIP_R]);
  prefs.putUInt("stripGfade", FadeSeconds[STRIP_G]);
  prefs.putUInt("stripBfade", FadeSeconds[STRIP_B]);

  prefs.putUInt("ambiRfade", FadeSeconds[AMBI_R]);
  prefs.putUInt("ambiGfade", FadeSeconds[AMBI_G]);
  prefs.putUInt("ambiBfade", FadeSeconds[AMBI_B]);
  prefs.putUInt("ambiUVfade", FadeSeconds[AMBI_UV]);

  prefs.end();


}



//////////////////// Function to handle web file uploads////////////////////////////////////////////////////////////////////////////
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.printf("UploadStart: %s\n", filename.c_str());
    // Open the file for writing
    request->_tempFile = SPIFFS.open("/" + filename, "w");
  }
  if (len) {
    // Write the file data to SPIFFS
    request->_tempFile.write(data, len);
  }
  if (final) {
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
    // Close the file after the upload is complete
    request->_tempFile.close();
  }
}




// This function applies a random walk to val by increasing or
// decreasing it by changeAmount or by leaving it unchanged.
// val is a pointer to the byte to be randomly changed.
// The new value of val will always be within [0, maxVal].
// A walk direction of 0 decreases val and a walk direction of 1
// increases val.  The directions argument specifies the number of
// possible walk directions to choose from, so when directions is 1, val
// will always decrease; when directions is 2, val will have a 50% chance
// of increasing and a 50% chance of decreasing; when directions is 3,
// val has an equal chance of increasing, decreasing, or staying the same.
void randomWalk(unsigned char *val, unsigned char maxVal, unsigned char changeAmount, unsigned char directions)
{
  unsigned char walk = random(directions);  // direction of random walk
  if (walk == 0)
  {
    // decrease val by changeAmount down to a min of 0
    if (*val >= changeAmount)
    {
      *val -= changeAmount;
    }
    else
    {
      *val = 0;
    }
  }
  else if (walk == 1)
  {
    // increase val by changeAmount up to a max of maxVal
    if (*val <= maxVal - changeAmount)
    {
      *val += changeAmount;
    }
    else
    {
      *val = maxVal;
    }
  }
}


// This function fades val by decreasing it by an amount proportional
// to its current value.  The fadeTime argument determines the
// how quickly the value fades.  The new value of val will be:
//   val = val - val*2^(-fadeTime)
// So a smaller fadeTime value leads to a quicker fade.
// If val is greater than zero, val will always be decreased by
// at least 1.
// val is a pointer to the byte to be faded.
void fade(unsigned char *val, unsigned char fadeTime)
{
  if (*val != 0)
  {
    unsigned char subAmt = *val >> fadeTime;  // val * 2^-fadeTime
    if (subAmt < 1)
      subAmt = 1;  // make sure we always decrease by at least 1
    *val -= subAmt;  // decrease value of byte pointed to by val
  }
}


// ***** PATTERN WarmWhiteShimmer *****
// This function randomly increases or decreases the brightness of the
// even red LEDs by changeAmount, capped at maxBrightness.  The green
// and blue LED values are set proportional to the red value so that
// the LED color is warm white.  Each odd LED is set to a quarter the
// brightness of the preceding even LEDs.  The dimOnly argument
// disables the random increase option when it is true, causing
// all the LEDs to get dimmer by changeAmount; this can be used for a
// fade-out effect.
void warmWhiteShimmer(unsigned char dimOnly)
{
  const unsigned char maxBrightness = 120;  // cap on LED brighness
  const unsigned char changeAmount = 2;   // size of random walk step

  for (int i = 0; i < LED_COUNT; i += 2)
  {
    // randomly walk the brightness of every even LED
    randomWalk(&colors[i].red, maxBrightness, changeAmount, dimOnly ? 1 : 2);

    // warm white: red = x, green = 0.8x, blue = 0.125x
    colors[i].green = colors[i].red*4/5;  // green = 80% of red
    colors[i].blue = colors[i].red >> 3;  // blue = red/8

    // every odd LED gets set to a quarter the brighness of the preceding even LED
    if (i + 1 < LED_COUNT)
    {
      colors[i+1] = rgb_color(colors[i].red >> 2, colors[i].green >> 2, colors[i].blue >> 2);
    }
  }
}


// ***** PATTERN RandomColorWalk *****
// This function randomly changes the color of every seventh LED by
// randomly increasing or decreasing the red, green, and blue components
// by changeAmount (capped at maxBrightness) or leaving them unchanged.
// The two preceding and following LEDs are set to progressively dimmer
// versions of the central color.  The initializeColors argument
// determines how the colors are initialized:
//   0: randomly walk the existing colors
//   1: set the LEDs to alternating red and green segments
//   2: set the LEDs to random colors
// When true, the dimOnly argument changes the random walk into a 100%
// chance of LEDs getting dimmer by changeAmount; this can be used for
// a fade-out effect.
void randomColorWalk(unsigned char initializeColors, unsigned char dimOnly)
{
  const unsigned char maxBrightness = 180;  // cap on LED brightness
  const unsigned char changeAmount = 3;  // size of random walk step

  // pick a good starting point for our pattern so the entire strip
  // is lit well (if we pick wrong, the last four LEDs could be off)
  unsigned char start;
  switch (LED_COUNT % 7)
  {
    case 0:
      start = 3;
      break;
    case 1:
      start = 0;
      break;
    case 2:
      start = 1;
      break;
    default:
      start = 2;
  }

  for (int i = start; i < LED_COUNT; i+=7)
  {
    if (initializeColors == 0)
    {
      // randomly walk existing colors of every seventh LED
      // (neighboring LEDs to these will be dimmer versions of the same color)
      randomWalk(&colors[i].red, maxBrightness, changeAmount, dimOnly ? 1 : 3);
      randomWalk(&colors[i].green, maxBrightness, changeAmount, dimOnly ? 1 : 3);
      randomWalk(&colors[i].blue, maxBrightness, changeAmount, dimOnly ? 1 : 3);
    }
    else if (initializeColors == 1)
    {
      // initialize LEDs to alternating red and green
      if (i % 2)
      {
        colors[i] = rgb_color(maxBrightness, 0, 0);
      }
      else
      {
        colors[i] = rgb_color(0, maxBrightness, 0);
      }
    }
    else
    {
      // initialize LEDs to a string of random colors
      colors[i] = rgb_color(random(maxBrightness), random(maxBrightness), random(maxBrightness));
    }

    // set neighboring LEDs to be progressively dimmer versions of the color we just set
    if (i >= 1)
    {
      colors[i-1] = rgb_color(colors[i].red >> 2, colors[i].green >> 2, colors[i].blue >> 2);
    }
    if (i >= 2)
    {
      colors[i-2] = rgb_color(colors[i].red >> 3, colors[i].green >> 3, colors[i].blue >> 3);
    }
    if (i + 1 < LED_COUNT)
    {
      colors[i+1] = colors[i-1];
    }
    if (i + 2 < LED_COUNT)
    {
      colors[i+2] = colors[i-2];
    }
  }
}


// ***** PATTERN TraditionalColors *****
// This function creates a repeating patern of traditional Christmas
// light colors: red, green, orange, blue, magenta.
// Every fourth LED is colored, and the pattern slowly moves by fading
// out the current set of lit LEDs while gradually brightening a new
// set shifted over one LED.
void traditionalColors()
{
  // loop counts to leave strip initially dark
  const unsigned char initialDarkCycles = 10;
  // loop counts it takes to go from full off to fully bright
  const unsigned char brighteningCycles = 20;

  if (loopCount < initialDarkCycles)  // leave strip fully off for 20 cycles
  {
    return;
  }

  // if LED_COUNT is not an exact multiple of our repeating pattern size,
  // it will not wrap around properly, so we pick the closest LED count
  // that is an exact multiple of the pattern period (20) and is not smaller
  // than the actual LED count.
  unsigned int extendedLEDCount = (((LED_COUNT-1)/20)+1)*20;

  for (int i = 0; i < extendedLEDCount; i++)
  {
    unsigned char brightness = (loopCount - initialDarkCycles)%brighteningCycles + 1;
    unsigned char cycle = (loopCount - initialDarkCycles)/brighteningCycles;

    // transform i into a moving idx space that translates one step per
    // brightening cycle and wraps around
    unsigned int idx = (i + cycle)%extendedLEDCount;
    if (idx < LED_COUNT)  // if our transformed index exists
    {
      if (i % 4 == 0)
      {
        // if this is an LED that we are coloring, set the color based
        // on the LED and the brightness based on where we are in the
        // brightening cycle
        switch ((i/4)%5)
        {
           case 0:  // red
             colors[idx].red = 200 * brightness/brighteningCycles;
             colors[idx].green = 10 * brightness/brighteningCycles;
             colors[idx].blue = 10 * brightness/brighteningCycles;
             break;
           case 1:  // green
             colors[idx].red = 10 * brightness/brighteningCycles;
             colors[idx].green = 200 * brightness/brighteningCycles;
             colors[idx].blue = 10 * brightness/brighteningCycles;
             break;
           case 2:  // orange
             colors[idx].red = 200 * brightness/brighteningCycles;
             colors[idx].green = 120 * brightness/brighteningCycles;
             colors[idx].blue = 0 * brightness/brighteningCycles;
             break;
           case 3:  // blue
             colors[idx].red = 10 * brightness/brighteningCycles;
             colors[idx].green = 10 * brightness/brighteningCycles;
             colors[idx].blue = 200 * brightness/brighteningCycles;
             break;
           case 4:  // magenta
             colors[idx].red = 200 * brightness/brighteningCycles;
             colors[idx].green = 64 * brightness/brighteningCycles;
             colors[idx].blue = 145 * brightness/brighteningCycles;
             break;
        }
      }
      else
      {
        // fade the 3/4 of LEDs that we are not currently brightening
        fade(&colors[idx].red, 3);
        fade(&colors[idx].green, 3);
        fade(&colors[idx].blue, 3);
      }
    }
  }
}


// Helper function for adjusting the colors for the BrightTwinkle
// and ColorExplosion patterns.  Odd colors get brighter and even
// colors get dimmer.
void brightTwinkleColorAdjust(unsigned char *color)
{
  if (*color == 255)
  {
    // if reached max brightness, set to an even value to start fade
    *color = 254;
  }
  else if (*color % 2)
  {
    // if odd, approximately double the brightness
    // you should only use odd values that are of the form 2^n-1,
    // which then gets a new value of 2^(n+1)-1
    // using other odd values will break things
    *color = *color * 2 + 1;
  }
  else if (*color > 0)
  {
    fade(color, 4);
    if (*color % 2)
    {
      (*color)--;  // if faded color is odd, subtract one to keep it even
    }
  }
}


// Helper function for adjusting the colors for the ColorExplosion
// pattern.  Odd colors get brighter and even colors get dimmer.
// The propChance argument determines the likelihood that neighboring
// LEDs are put into the brightening stage when the central LED color
// is 31 (chance is: 1 - 1/(propChance+1)).  The neighboring LED colors
// are pointed to by leftColor and rightColor (it is not important that
// the leftColor LED actually be on the "left" in your setup).
void colorExplosionColorAdjust(unsigned char *color, unsigned char propChance,
 unsigned char *leftColor, unsigned char *rightColor)
{
  if (*color == 31 && random(propChance+1) != 0)
  {
    if (leftColor != 0 && *leftColor == 0)
    {
      *leftColor = 1;  // if left LED exists and color is zero, propagate
    }
    if (rightColor != 0 && *rightColor == 0)
    {
      *rightColor = 1;  // if right LED exists and color is zero, propagate
    }
  }
  brightTwinkleColorAdjust(color);
}


// ***** PATTERN ColorExplosion *****
// This function creates bursts of expanding, overlapping colors by
// randomly picking LEDs to brighten and then fade away.  As these LEDs
// brighten, they have a chance to trigger the same process in
// neighboring LEDs.  The color of the burst is randomly chosen from
// among red, green, blue, and white.  If a red burst meets a green
// burst, for example, the overlapping portion will be a shade of yellow
// or orange.
// When true, the noNewBursts argument changes prevents the generation
// of new bursts; this can be used for a fade-out effect.
// This function uses a very similar algorithm to the BrightTwinkle
// pattern.  The main difference is that the random twinkling LEDs of
// the BrightTwinkle pattern do not propagate to neighboring LEDs.
void colorExplosion(unsigned char noNewBursts)
{
  // adjust the colors of the first LED
  colorExplosionColorAdjust(&colors[0].red, 9, (unsigned char*)0, &colors[1].red);
  colorExplosionColorAdjust(&colors[0].green, 9, (unsigned char*)0, &colors[1].green);
  colorExplosionColorAdjust(&colors[0].blue, 9, (unsigned char*)0, &colors[1].blue);

  for (int i = 1; i < LED_COUNT - 1; i++)
  {
    // adjust the colors of second through second-to-last LEDs
    colorExplosionColorAdjust(&colors[i].red, 9, &colors[i-1].red, &colors[i+1].red);
    colorExplosionColorAdjust(&colors[i].green, 9, &colors[i-1].green, &colors[i+1].green);
    colorExplosionColorAdjust(&colors[i].blue, 9, &colors[i-1].blue, &colors[i+1].blue);
  }

  // adjust the colors of the last LED
  colorExplosionColorAdjust(&colors[LED_COUNT-1].red, 9, &colors[LED_COUNT-2].red, (unsigned char*)0);
  colorExplosionColorAdjust(&colors[LED_COUNT-1].green, 9, &colors[LED_COUNT-2].green, (unsigned char*)0);
  colorExplosionColorAdjust(&colors[LED_COUNT-1].blue, 9, &colors[LED_COUNT-2].blue, (unsigned char*)0);

  if (!noNewBursts)
  {
    // if we are generating new bursts, randomly pick one new LED
    // to light up
    for (int i = 0; i < 1; i++)
    {
      int j = random(LED_COUNT);  // randomly pick an LED

      switch(random(7))  // randomly pick a color
      {
        // 2/7 chance we will spawn a red burst here (if LED has no red component)
        case 0:
        case 1:
          if (colors[j].red == 0)
          {
            colors[j].red = 1;
          }
          break;

        // 2/7 chance we will spawn a green burst here (if LED has no green component)
        case 2:
        case 3:
          if (colors[j].green == 0)
          {
            colors[j].green = 1;
          }
          break;

        // 2/7 chance we will spawn a white burst here (if LED is all off)
        case 4:
        case 5:
          if ((colors[j].red == 0) && (colors[j].green == 0) && (colors[j].blue == 0))
          {
            colors[j] = rgb_color(1, 1, 1);
          }
          break;

        // 1/7 chance we will spawn a blue burst here (if LED has no blue component)
        case 6:
          if (colors[j].blue == 0)
          {
            colors[j].blue = 1;
          }
          break;

        default:
          break;
      }
    }
  }
}


// ***** PATTERN Gradient *****
// This function creates a scrolling color gradient that smoothly
// transforms from red to white to green back to white back to red.
// This pattern is overlaid with waves of brightness and dimness that
// scroll at twice the speed of the color gradient.
void gradient()
{
  unsigned int j = 0;

  // populate colors array with full-brightness gradient colors
  // (since the array indices are a function of loopCount, the gradient
  // colors scroll over time)
  while (j < LED_COUNT)
  {
    // transition from red to green over 8 LEDs
    for (int i = 0; i < 8; i++)
    {
      if (j >= LED_COUNT){ break; }
      colors[(loopCount/2 + j + LED_COUNT)%LED_COUNT] = rgb_color(160 - 20*i, 20*i, (160 - 20*i)*20*i/160);
      j++;
    }
    // transition from green to red over 8 LEDs
    for (int i = 0; i < 8; i++)
    {
      if (j >= LED_COUNT){ break; }
      colors[(loopCount/2 + j + LED_COUNT)%LED_COUNT] = rgb_color(20*i, 160 - 20*i, (160 - 20*i)*20*i/160);
      j++;
    }
  }

  // modify the colors array to overlay the waves of dimness
  // (since the array indices are a function of loopCount, the waves
  // of dimness scroll over time)
  const unsigned char fullDarkLEDs = 10;  // number of LEDs to leave fully off
  const unsigned char fullBrightLEDs = 5;  // number of LEDs to leave fully bright
  const unsigned char cyclePeriod = 14 + fullDarkLEDs + fullBrightLEDs;

  // if LED_COUNT is not an exact multiple of our repeating pattern size,
  // it will not wrap around properly, so we pick the closest LED count
  // that is an exact multiple of the pattern period (cyclePeriod) and is not
  // smaller than the actual LED count.
  unsigned int extendedLEDCount = (((LED_COUNT-1)/cyclePeriod)+1)*cyclePeriod;

  j = 0;
  while (j < extendedLEDCount)
  {
    unsigned int idx;

    // progressively dim the LEDs
    for (int i = 1; i < 8; i++)
    {
      idx = (j + loopCount) % extendedLEDCount;
      if (j++ >= extendedLEDCount){ return; }
      if (idx >= LED_COUNT){ continue; }

      colors[idx].red >>= i;
      colors[idx].green >>= i;
      colors[idx].blue >>= i;
    }

    // turn off these LEDs
    for (int i = 0; i < fullDarkLEDs; i++)
    {
      idx = (j + loopCount) % extendedLEDCount;
      if (j++ >= extendedLEDCount){ return; }
      if (idx >= LED_COUNT){ continue; }

      colors[idx].red = 0;
      colors[idx].green = 0;
      colors[idx].blue = 0;
    }

    // progressively bring these LEDs back
    for (int i = 0; i < 7; i++)
    {
      idx = (j + loopCount) % extendedLEDCount;
      if (j++ >= extendedLEDCount){ return; }
      if (idx >= LED_COUNT){ continue; }

      colors[idx].red >>= (7 - i);
      colors[idx].green >>= (7 - i);
      colors[idx].blue >>= (7 - i);
    }

    // skip over these LEDs to leave them at full brightness
    j += fullBrightLEDs;
  }
}


// ***** PATTERN BrightTwinkle *****
// This function creates a sparkling/twinkling effect by randomly
// picking LEDs to brighten and then fade away.  Possible colors are:
//   white, red, green, blue, yellow, cyan, and magenta
// numColors is the number of colors to generate, and minColor
// indicates the starting point (white is 0, red is 1, ..., and
// magenta is 6), so colors generated are all of those from minColor
// to minColor+numColors-1.  For example, calling brightTwinkle(2, 2, 0)
// will produce green and blue twinkles only.
// When true, the noNewBursts argument changes prevents the generation
// of new twinkles; this can be used for a fade-out effect.
// This function uses a very similar algorithm to the ColorExplosion
// pattern.  The main difference is that the random twinkling LEDs of
// this BrightTwinkle pattern do not propagate to neighboring LEDs.
void brightTwinkle(unsigned char minColor, unsigned char numColors, unsigned char noNewBursts)
{
  // Note: the colors themselves are used to encode additional state
  // information.  If the color is one less than a power of two
  // (but not 255), the color will get approximately twice as bright.
  // If the color is even, it will fade.  The sequence goes as follows:
  // * Randomly pick an LED.
  // * Set the color(s) you want to flash to 1.
  // * It will automatically grow through 3, 7, 15, 31, 63, 127, 255.
  // * When it reaches 255, it gets set to 254, which starts the fade
  //   (the fade process always keeps the color even).
  for (int i = 0; i < LED_COUNT; i++)
  {
    brightTwinkleColorAdjust(&colors[i].red);
    brightTwinkleColorAdjust(&colors[i].green);
    brightTwinkleColorAdjust(&colors[i].blue);
  }

  if (!noNewBursts)
  {
    // if we are generating new twinkles, randomly pick four new LEDs
    // to light up
    for (int i = 0; i < 4; i++)
    {
      int j = random(LED_COUNT);
      if (colors[j].red == 0 && colors[j].green == 0 && colors[j].blue == 0)
      {
        // if the LED we picked is not already lit, pick a random
        // color for it and seed it so that it will start getting
        // brighter in that color
        switch (random(numColors) + minColor)
        {
          case 0:
            colors[j] = rgb_color(1, 1, 1);  // white
            break;
          case 1:
            colors[j] = rgb_color(1, 0, 0);  // red
            break;
          case 2:
            colors[j] = rgb_color(0, 1, 0);  // green
            break;
          case 3:
            colors[j] = rgb_color(0, 0, 1);  // blue
            break;
          case 4:
            colors[j] = rgb_color(1, 1, 0);  // yellow
            break;
          case 5:
            colors[j] = rgb_color(0, 1, 1);  // cyan
            break;
          case 6:
            colors[j] = rgb_color(1, 0, 1);  // magenta
            break;
          default:
            colors[j] = rgb_color(1, 1, 1);  // white
        }
      }
    }
  }
}


// ***** PATTERN Collision *****
// This function spawns streams of color from each end of the strip
// that collide, at which point the entire strip flashes bright white
// briefly and then fades.  Unlike the other patterns, this function
// maintains a lot of complicated state data and tells the main loop
// when it is done by returning 1 (a return value of 0 means it is
// still in progress).
unsigned char collision()
{
  const unsigned char maxBrightness = 180;  // max brightness for the colors
  const unsigned char numCollisions = 5;  // # of collisions before pattern ends
  static unsigned char state = 0;  // pattern state
  static unsigned int count = 0;  // counter used by pattern

  if (loopCount == 0)
  {
    state = 0;
  }

  if (state % 3 == 0)
  {
    // initialization state
    switch (state/3)
    {
      case 0:  // first collision: red streams
        colors[0] = rgb_color(maxBrightness, 0, 0);
        break;
      case 1:  // second collision: green streams
        colors[0] = rgb_color(0, maxBrightness, 0);
        break;
      case 2:  // third collision: blue streams
        colors[0] = rgb_color(0, 0, maxBrightness);
        break;
      case 3:  // fourth collision: warm white streams
        colors[0] = rgb_color(maxBrightness, maxBrightness*4/5, maxBrightness>>3);
        break;
      default:  // fifth collision and beyond: random-color streams
        colors[0] = rgb_color(random(maxBrightness), random(maxBrightness), random(maxBrightness));
    }

    // stream is led by two full-white LEDs
    colors[1] = colors[2] = rgb_color(255, 255, 255);
    // make other side of the strip a mirror image of this side
    colors[LED_COUNT - 1] = colors[0];
    colors[LED_COUNT - 2] = colors[1];
    colors[LED_COUNT - 3] = colors[2];

    state++;  // advance to next state
    count = 8;  // pick the first value of count that results in a startIdx of 1 (see below)
    return 0;
  }

  if (state % 3 == 1)
  {
    // stream-generation state; streams accelerate towards each other
    unsigned int startIdx = count*(count + 1) >> 6;
    unsigned int stopIdx = startIdx + (count >> 5);
    count++;
    if (startIdx < (LED_COUNT + 1)/2)
    {
      // if streams have not crossed the half-way point, keep them growing
      for (int i = 0; i < startIdx-1; i++)
      {
        // start fading previously generated parts of the stream
        fade(&colors[i].red, 5);
        fade(&colors[i].green, 5);
        fade(&colors[i].blue, 5);
        fade(&colors[LED_COUNT - i - 1].red, 5);
        fade(&colors[LED_COUNT - i - 1].green, 5);
        fade(&colors[LED_COUNT - i - 1].blue, 5);
      }
      for (int i = startIdx; i <= stopIdx; i++)
      {
        // generate new parts of the stream
        if (i >= (LED_COUNT + 1) / 2)
        {
          // anything past the halfway point is white
          colors[i] = rgb_color(255, 255, 255);
        }
        else
        {
          colors[i] = colors[i-1];
        }
        // make other side of the strip a mirror image of this side
        colors[LED_COUNT - i - 1] = colors[i];
      }
      // stream is led by two full-white LEDs
      colors[stopIdx + 1] = colors[stopIdx + 2] = rgb_color(255, 255, 255);
      // make other side of the strip a mirror image of this side
      colors[LED_COUNT - stopIdx - 2] = colors[stopIdx + 1];
      colors[LED_COUNT - stopIdx - 3] = colors[stopIdx + 2];
    }
    else
    {
      // streams have crossed the half-way point of the strip;
      // flash the entire strip full-brightness white (ignores maxBrightness limits)
      for (int i = 0; i < LED_COUNT; i++)
      {
        colors[i] = rgb_color(255, 255, 255);
      }
      state++;  // advance to next state
    }
    return 0;
  }

  if (state % 3 == 2)
  {
    // fade state
    if (colors[0].red == 0 && colors[0].green == 0 && colors[0].blue == 0)
    {
      // if first LED is fully off, advance to next state
      state++;

      // after numCollisions collisions, this pattern is done
      return state >= 3*numCollisions;
    }

    // fade the LEDs at different rates based on the state
    for (int i = 0; i < LED_COUNT; i++)
    {
      switch (state/3)
      {
        case 0:  // fade through green
          fade(&colors[i].red, 3);
          fade(&colors[i].green, 4);
          fade(&colors[i].blue, 2);
          break;
        case 1:  // fade through red
          fade(&colors[i].red, 4);
          fade(&colors[i].green, 3);
          fade(&colors[i].blue, 2);
          break;
        case 2:  // fade through yellow
          fade(&colors[i].red, 4);
          fade(&colors[i].green, 4);
          fade(&colors[i].blue, 3);
          break;
        case 3:  // fade through blue
          fade(&colors[i].red, 3);
          fade(&colors[i].green, 2);
          fade(&colors[i].blue, 4);
          break;
        default:  // stay white through entire fade
          fade(&colors[i].red, 4);
          fade(&colors[i].green, 4);
          fade(&colors[i].blue, 4);
      }
    }
  }

  return 0;
}



