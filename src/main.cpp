#define SS_CONNECTED
//#define DEBUG
//#define DEBUG2
//#define USE_ILI9341
#define USE_ST7789

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#ifdef SS_CONNECTED
#include <Adafruit_seesaw.h>
#endif
#include <blinkyLights.h>
#ifdef USE_ST7789
  #include "Adafruit_ST7789.h"
#endif
#ifdef USE_ILI9341
  #include "Adafruit_ILI9341.h"
#endif
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSansBold9pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSansBold12pt7b.h"
#include "fonts/FreeMono9pt7b.h"
#include "Fonts/FreeSerif9pt7b.h"
#include "Fonts/FreeSerifItalic9pt7b.h"
#include "Fonts/FreeSansBold18pt7b.h"

#define TFT_DC 11
#define TFT_CS 13
//#define TFT_BL A2
//#define SD_CS 13
#define TFT_RST 12

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define DARKGREEN 0x03E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

#define GRAY 0xBBBB
#define LIGHTGREY 0xC618
#define DARKCYAN 0x03EF
#define GREENYELLOW 0xAFE5
#define PINK 0xFC18


void handleSwitchUp();
void handleSwitchDown();
void handleSwitchLeft();
void handleSwitchRight();
void printWifiStatus();
void digitalClockDisplay(time_t now);
void printDigits(int digits);
bool postDataToFeed(uint8_t tempC,uint16_t capread);
bool updateThresholds(uint16_t &green, uint16_t &yellow);
bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay,uint8_t tempC, uint16_t capread, char lightStatus);
bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay,uint8_t tempC, uint16_t capread, char lightStatus, String message, bool isErrorMessage);

Adafruit_seesaw ss;


#ifdef USE_ILI9341
 Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
#endif

#ifdef USE_ST7789
  Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
#endif



const int ss_addr = 0x36;
const uint8_t ledGreen = A5;
const uint8_t ledYellow = A4;
const uint8_t ledRed = A3;
const uint8_t swLeft = 10;
const uint8_t swUp = 9;
const uint8_t swRight = 6;
const uint8_t swDown = 5;

const uint8_t postThreshold = 5;

volatile uint8_t onLED;

uint8_t brightnessLevel = 128;
uint16_t delayTime = 600;

volatile bool swLeftPressed = false;
volatile bool swUpPressed = false;
volatile bool swRightPressed = false;
volatile bool swDownPressed = false;

volatile uint8_t lastTempC=255;
volatile uint16_t lastCapread=4000;
volatile uint8_t lastHour = 0;
volatile uint8_t lastMinute = 0;
volatile uint8_t thisSecond;
volatile uint16_t thisYear;
volatile uint8_t thisMonth;
volatile uint8_t thisDay;
volatile char lightStatus = 'R';

volatile bool didTheHalfHour = 0;
volatile bool sentTheData = 0;
volatile bool gotTheThresholds = 0;

uint16_t greenThreshold = 600;
uint16_t yellowThreshold = 450;
uint16_t lastGreenThreshold=0;
uint16_t lastYellowThreshold=0;

int screenWidth;
int screenHeight;
uint8_t screenRotation = 3;

/*

Current Endpoints
Web	https://io.adafruit.com/jkobler/feeds/plant-test-1
API	https://io.adafruit.com/api/v2/jkobler/feeds/plant-test-1
MQTT by Key	jkobler/feeds/plant-test-1
*/

char adafruitIOserver[] = "";


volatile int keyIndex = 0;
int status = WL_IDLE_STATUS;
WiFiClient client;
const long utcOffsetInSeconds = -6*3600; 
const char* ntpServer = "pool.ntp.org";
void setup() {

#ifdef DEBUG
    Serial.begin(9600);
#endif
    pinMode(swLeft,INPUT);
    pinMode(swUp,INPUT);
    pinMode(swRight,INPUT);    
    pinMode(swDown,INPUT);    
    pinMode(ledGreen,OUTPUT);
    pinMode(ledYellow,OUTPUT);
    pinMode(ledRed,OUTPUT);  

    //WiFi.disconnect(); 
  
    //delay(1000);    
#ifdef DEBUG
    while(!Serial) {
    }
#endif

#ifdef USE_ILI9341
    tft.begin();
 #endif
#ifdef USE_ST7789
    tft.init(240, 320);
#endif

    tft.setRotation(screenRotation);
    tft.setFont(&FreeSansBold12pt7b);    


    screenWidth = tft.width();
    screenHeight = tft.height();

    tft.fillScreen(BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(5, 17);
    tft.println("GREEN THUMB - Ver 20260103a");

    tft.setFont(&FreeSans9pt7b);    
    tft.print("Initializing Soil Sensor:");
    // WiFi.mode(WIFI_STA);
    // WiFi.setHostname("GreenThumbProject");


    if (!ss.begin(ss_addr)) {
#ifdef DEBUG
        Serial.println("ERROR! seesaw not found");
#endif
        tft.println("not found");
        //while(1) {
            fadeInLights(ledRed, brightnessLevel, 250);   
            fadeOutLights(ledRed, brightnessLevel, 250);
        //}
    } else {
        constantLights(ledGreen,1,brightnessLevel);
        tft.print("found ver: ");
        tft.println(ss.getVersion(), HEX);
#ifdef DEBUG
        Serial.print("found ver: ");
        Serial.println(ss.getVersion(), HEX);
#endif
        constantLights(ledGreen,0,brightnessLevel);
    }

    WiFi.begin(WIFI_SSID,WIFI_PASS);
#ifdef DEBUG
    Serial.print("Starting WiFi, connecting to ");
    Serial.println(WIFI_SSID);
#endif
    tft.print("Starting WiFi, connecting to ");
    tft.println(WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
        Serial.print(".");
#endif
        fadeInLights(ledRed, brightnessLevel, 250);   
        fadeOutLights(ledRed, brightnessLevel, 250);

    }
#ifdef DEBUG
    Serial.println("");
    Serial.println("Connected to WiFi");
#endif
    tft.println("Connected to WiFi");
    delay(1000);
    printWifiStatus();
    blinkyLights(ledGreen,brightnessLevel,1000);

    configTime(utcOffsetInSeconds, 3600, ntpServer); 

#ifdef DEBUG
    Serial.println("Time synchronized using configTime().");
#endif
    tft.println("Time synchronized using configTime().");

    attachInterrupt(swLeft, handleSwitchLeft, RISING);
    attachInterrupt(swUp, handleSwitchUp, RISING);
    attachInterrupt(swRight, handleSwitchRight, RISING);
    attachInterrupt(swDown, handleSwitchDown, RISING);

    fadeInLights(ledGreen, brightnessLevel, delayTime);
    fadeOutLights(ledGreen, brightnessLevel, delayTime);
    fadeInLights(ledYellow, brightnessLevel, delayTime);
    fadeOutLights(ledYellow, brightnessLevel, delayTime);
    fadeInLights(ledRed, brightnessLevel, delayTime);   
    fadeOutLights(ledRed, brightnessLevel, delayTime);

    updateThresholds(greenThreshold, yellowThreshold);
}

void loop() {
#ifdef DEBUG2
    Serial.print("Loop");
    delay(200);
#endif


    uint8_t tempC = (ss.getTemp()*1.8)+27;
    uint16_t capread = ss.touchRead(0);

    struct tm timeinfo;
    getLocalTime(&timeinfo);
    uint8_t thisHour = timeinfo.tm_hour;
    uint8_t thisMinute = timeinfo.tm_min;
    thisSecond = timeinfo.tm_sec;
    thisYear = timeinfo.tm_year+1900;
    thisMonth = timeinfo.tm_mon+1;
    thisDay = timeinfo.tm_mday;
    int isDST = timeinfo.tm_isdst;



    if (thisMinute%5 == 0 && !gotTheThresholds) {
        updateThresholds(greenThreshold, yellowThreshold);
        gotTheThresholds = 1;
    }
    else if (thisMinute%5 != 0) {
        gotTheThresholds = 0;
    }
 
    if (swLeftPressed == 1) {
#ifdef DEBUG
        Serial.println("Left Switch");
#endif
        swLeftPressed = 0;
    }
    else if (swUpPressed == 1) {
#ifdef DEBUG
        Serial.println("Up Switch");
#endif
        swUpPressed = 0;
    }
    else if (swRightPressed == 1) {
#ifdef DEBUG
        Serial.println("Right Switch");
#endif
        swRightPressed = 0;    
    }
    else if (swDownPressed == 1) {
#ifdef DEBUG
        Serial.println("Down Switch");
#endif
        swDownPressed = 0;
    }


    if (capread < yellowThreshold) {
        fadeOutLights(onLED, brightnessLevel, delayTime);
        onLED= ledRed;
        fadeInLights(onLED, brightnessLevel, delayTime);
        lightStatus='R';
//        delay(2000);
    }
    else if (capread > greenThreshold) {
        fadeOutLights(onLED, brightnessLevel, delayTime);
        onLED= ledGreen;
        fadeInLights(onLED, brightnessLevel, delayTime);
        lightStatus='G';
//        delay(2000);
    }
    else {
        fadeOutLights(onLED, brightnessLevel, delayTime);
        onLED= ledYellow;
        fadeInLights(onLED, brightnessLevel, delayTime); 
        lightStatus='Y';       
//        delay(2000);
    }

    // if (lastCapread < capread-postThreshold 
    //         || lastCapread > capread+postThreshold 
    //         || thisHour != lastHour 
    //         || (thisMinute == 30 && didTheHalfHour == 0)
    //         || (lastGreenThreshold != greenThreshold || lastYellowThreshold != yellowThreshold)
    //     ) {

    if (lastMinute != thisMinute) {

#ifdef DEBUG
        Serial.println();
        Serial.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);

        Serial.print("Last Temperature: "); Serial.print(lastTempC); Serial.println("(F)");
        Serial.print("Last Moisture Reading: "); Serial.println(lastCapread);
#endif

        lastCapread = capread;
        lastTempC = tempC;


#ifdef DEBUG
        Serial.print("Temperature: "); Serial.print(tempC); Serial.println("(F)");
        Serial.print("Moisture Reading: "); Serial.println(capread);
#endif

        sentTheData = postDataToFeed(tempC,capread);

#ifdef DEBUG
        if (sentTheData==true) 
            Serial.println("Sent data.");
        else 
            Serial.println("Didn't send data.");
#endif

        if (thisHour != lastHour) 
            lastHour = thisHour;

        if (thisMinute == 30) 
            didTheHalfHour = 1;
        else 
            didTheHalfHour = 0;

        lastGreenThreshold = greenThreshold;
        lastYellowThreshold = yellowThreshold;

        updateDisplay( thisHour,thisMinute,thisYear,thisMonth,thisDay,tempC,capread,lightStatus );
        lastMinute = thisMinute;
    }


}


bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay,uint8_t tempC, uint16_t capread, char lightStatus) {
    tft.fillScreen(BLACK);
    tft.setTextColor(CYAN);
    tft.setCursor(5, 30);
  
    tft.setFont(&FreeSansBold18pt7b);
    tft.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);
    
    tft.setFont(&FreeSansBold12pt7b);    
    switch(lightStatus) {
    case 'G':
        tft.setTextColor(GREEN);
        break;
    case 'Y':
        tft.setTextColor(YELLOW);
        break;
    default:
        tft.setTextColor(RED);   
    } 
    tft.print("Temperature: "); tft.print(tempC); tft.println("(F)");
    tft.print("Moisture Reading: "); tft.println(capread);
    switch(lightStatus) {
    case 'G':
        tft.drawCircle((screenWidth/2), (screenHeight)/2+40, 25, GREEN);
        tft.fillRect((screenWidth/2)-40, (screenHeight/2), 80,50,BLACK);
        tft.drawCircle((screenWidth/2),  (screenHeight)/2+40,40, GREEN);
        tft.fillCircle((screenWidth/2)-17, (screenHeight/2)+25, 6, GREEN);
        tft.fillCircle((screenWidth/2)+17, (screenHeight/2)+25, 6, GREEN);
        break;
    case 'Y':
        // tft.drawCircle((screenWidth/2), (screenHeight)/2+40, 25, YELLOW);
        // tft.fillRect((screenWidth/2)-40, (screenHeight/2), 80,50,BLACK);
        tft.drawLine((screenWidth/2)-17,(screenHeight)/2+60, (screenWidth/2)+17,(screenHeight)/2+60, YELLOW);
        tft.drawCircle((screenWidth/2),  (screenHeight)/2+40,40, YELLOW);
        tft.fillCircle((screenWidth/2)-17, (screenHeight/2)+25, 6, YELLOW);
        tft.fillCircle((screenWidth/2)+17, (screenHeight/2)+25, 6, YELLOW);
        break;
    default:
        tft.drawCircle((screenWidth/2), (screenHeight/2)+80, 25, RED);
        tft.fillRect((screenWidth/2)-40, (screenHeight/2)+65, 80,50,BLACK);
        tft.drawCircle((screenWidth/2),  (screenHeight)/2+40,40, RED);
        tft.fillCircle((screenWidth/2)-17, (screenHeight/2)+25, 6, RED);
        tft.fillCircle((screenWidth/2)+17, (screenHeight/2)+25, 6, RED);
    } 
    return true;
}


bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay,uint8_t tempC, uint16_t capread, char lightStatus, String message, bool isErrorMessage) {
    tft.fillScreen(BLACK);
    tft.setTextColor(CYAN);
    tft.setCursor(5, 30);
  
    tft.setFont(&FreeSansBold18pt7b);
    tft.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);
    
    tft.setFont(&FreeSansBold12pt7b);    
    switch(lightStatus) {
    case 'G':
        tft.setTextColor(GREEN);
        break;
    case 'Y':
        tft.setTextColor(YELLOW);
        break;
    default:
        tft.setTextColor(RED);   
    } 
    tft.print("Temperature: "); tft.print(tempC); tft.println("(F)");
    tft.print("Moisture Reading: "); tft.println(capread);

    tft.setFont(&FreeSansBold9pt7b);    
    if (isErrorMessage == true)
        tft.setTextColor(RED);   

    tft.println(message);
    
    return true;
}

bool postDataToFeed(uint8_t tempC, uint16_t capread) {
    const char* host = "io.adafruit.com";
    const int port = 80; // Use 443 with WiFiClientSecure for SSL

    if (!client.connect(host, port)) {
#ifdef DEBUG
        Serial.println("Connection to Adafruit IO failed.");
#endif
        updateDisplay(lastHour, lastMinute,thisYear,thisMonth,thisDay,lastTempC,lastCapread,lightStatus,"Connection to Adafruit IO failed.",true);
        return false;
    }

    // Build the bulk update URL for your group
    // Replace 'green-thumb' with your actual group key if it's different
    String url = "/api/v2/" + String(IO_USERNAME) + "/groups/green-thumb/data";

    // Create a JSON payload with all three feed keys and values
    String body = "{\"feeds\": [";
    body += "{\"key\": \"temperature\", \"value\": \"" + String(tempC) + "\"},";
    body += "{\"key\": \"moisture-level\", \"value\": \"" + String(capread) + "\"},";
    body += "{\"key\": \"light-status\", \"value\": \"" + String(lightStatus) + "\"}";
    body += "]}";

    // Send a single HTTP POST
    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "X-AIO-Key: " + IO_KEY + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + body.length() + "\r\n" +
                 "Connection: close\r\n\r\n" +
                 body);

    // Wait for the server to process (essential for reliable delivery)
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            client.stop();
            return false;
        }
    }

    client.stop();
    return true;
}

bool updateThresholds(uint16_t &green, uint16_t &yellow) {
    const char* host = "io.adafruit.com";
    String url = "/api/v2/" + String(IO_USERNAME) + "/groups/green-thumb";

    if (!client.connect(host, 80)){

#ifdef DEBUG
        Serial.println("Connection to Adafruit IO failed.");
#endif

        updateDisplay(lastHour, lastMinute,thisYear,thisMonth,thisDay,lastTempC,lastCapread,lightStatus,"Connection to Adafruit IO failed.",true);
        return false;
    }

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "X-AIO-Key: " + IO_KEY + "\r\n" +
                 "Connection: close\r\n\r\n");

    // Skip headers
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }

    String body = client.readString();
    client.stop();
#ifdef DEBUG2
    Serial.print("What was returned: ");
    Serial.println(body);
#endif
    // Simple parsing logic for two known feeds
    auto parseVal = [&](String name) {
        int kIdx = body.indexOf("\"name\":\"" + name + "\"");
        int vIdx = body.indexOf("\"last_value\":\"", kIdx);
        if (vIdx == -1) return 0L;
        int start = vIdx + 14;
        int end = body.indexOf("\"", start);
        return body.substring(start, end).toInt();
    };

    green = parseVal("Green Threshold");
    yellow = parseVal("Yellow Threshold");
#ifdef DEBUG    
    Serial.printf("Updated Thresholds -> Green: %d, Yellow: %d\n", green, yellow);
#endif
    return true;
}

void handleSwitchUp() {
    swUpPressed = 1;
}
void handleSwitchDown() {
    swDownPressed = 1;
}
void handleSwitchLeft() {
    swLeftPressed = 1;
}
void handleSwitchRight() {
    swRightPressed = 1;
}

void printWifiStatus() {
    // print the SSID of the network you're attached to:
    IPAddress ip = WiFi.localIP();
    long rssi = WiFi.RSSI();
    String currectSSID = WiFi.SSID();
    String macAddress = WiFi.macAddress();
    String hostname = WiFi.getHostname();
#ifdef DEBUG
    Serial.print("SSID: ");
    Serial.println(currectSSID);
    // print your board's IP address:
    Serial.print("Hostname: ");
    Serial.println(hostname);
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.print("MAC Address: ");
    Serial.println(macAddress);
    // print the received signal strength:
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
#endif
    tft.setFont(&FreeMono9pt7b);
    tft.fillScreen(BLACK);
    tft.setTextColor(CYAN);
    tft.setCursor(0, 15);
    tft.print("SSID: ");
    tft.println(currectSSID);
    // print your board's IP address:
    tft.print("Hostname: ");
    tft.println(hostname);
    tft.print("IP Address: ");
    tft.println(ip);
    tft.print("MAC Address: ");
    tft.println(macAddress);
    // print the received signal strength:
    tft.print("signal strength (RSSI):");
    tft.print(rssi);
    tft.println(" dBm");
  


}

