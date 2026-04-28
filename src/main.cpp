#define DEBUG
//#define DEBUG2


/*
Google AI ideas I liked...

Ideas for the "Touch" Action
Since you have an ESP32-S3 (WiFi capable), even if you don't have more local sensors, 
you can pull external data. Here are a few "low-hardware" ideas for what happens when 
you tap a plant icon:
Calibration Mode: Tapping a plant could trigger a 5-second "High/Low" read. You could 
put the sensor in bone-dry soil, tap it, then in a cup of water and tap it, to 
automatically set your greenThreshold and yellowThreshold.
Weather Context: Tapping could show a "Weather Sync." If the ESP32 knows it’s going to 
be 95°F (35°C) tomorrow via a weather API, it could highlight the moisture data in 
Orange to warn you it'll dry out fast.
Watering History: You could add a lastWatered timestamp to your soilSensor struct. 
Tapping the face "resets" the timer, telling the system "I just watered this." The 
emoji could then stay happy for a set number of hours regardless of the capacitance 
reading while the water "settles" in the soil.


*/

#include "Arduino.h"
#include "WiFi.h"
#include "config.h"
#include "vector"
#include "Adafruit_seesaw.h"
#include "Wire.h"
#include "blinkyLights.h"
#include "Adafruit_seesaw.h"
#include "spi.h"
#include "sd.h"

#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include "Adafruit_TSC2007.h"  // Touchscreen

#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSansBold12pt7b.h"
#include "Fonts/FreeSansBold18pt7b.h"

#include "bitmaps.h"


#define STMPE_CS 6// 32 //D6 GPIO6 0x001C IRQ for touch screen
#define TFT_CS 9// 15 //D9 GPIO9 0x0028
#define TFT_DC 10// 33 //D10 GPIO10 0x002C
#define SD_CS 5// 14 //D5 GPIO5 0x0018
#define TFT_RST -1
#define TSC_IRQ STMPE_CS
#define TSC_TS_MINX 300
#define TSC_TS_MAXX 3800
#define TSC_TS_MINY 185
#define TSC_TS_MAXY 3700
#define CONF_FILE "/gtconf.ini"


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

void handleTouchScreen(long whereX, long whereY);
void printWifiStatus(int16_t locY, bool showBackButton);
void digitalClockDisplay(time_t now);
void printDigits(int digits);
bool postDataToFeed(uint8_t tempC,uint16_t capread);
//bool updateThresholds(uint16_t &green, uint16_t &yellow);
bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay);
void showBack();
void showHome();
void showConfig(bool needsUpdate);
void showPlantDetails(bool needsUpdate);
void showGroupDetails(bool needsUpdate);
void printMessage(int16_t locX, int16_t locY, String message, bool isErrorMessage);
void drawFaceStatus(int16_t locX, int16_t locY, char lightStatus_l, String plantName, float temp, uint16_t capread );
bool readConfig();
void pcaselect(int mux_addr, uint8_t port);
void getSensorData();

//Adafruit_seesaw soilSensor;
using namespace std;

File green_thumb_conf;

Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
Adafruit_TSC2007 ts = Adafruit_TSC2007();
int16_t min_x, max_x, min_y, max_y;
volatile long tsWhere_x,tsWhere_y;
//volatile int16_t tsPressure;
//volatile bool tsTouched = false;
volatile long lastTouchTime = 0;
const uint16_t touchCoolDown = 300;
const uint16_t eachFaceX = 130;
const uint16_t eachFaceY = face_size_y + 35;

uint8_t displayContext = 0;
uint8_t selectedIdx = 0;

// #define DISPLAY_HOME 0;
// #define DISPLAY_CONFIG 1;
// #define DISPLAY_PLANT 2;
// #define DISPLAY_GROUP 3;



//const int ss_addr = 0x36;
const int ts_addr = 0x48;
//const int mx_addr = 0x70;
const uint8_t ledGreen = A5;
const uint8_t ledYellow = A4;
const uint8_t ledRed = A3;


const uint8_t postThreshold = 5;

volatile uint8_t onLED;

uint8_t brightnessLevel = 128;
//volatile uint8_t currentBrightness = 0;
//volatile bool brightnessUp = true;
uint16_t delayTime = 500;
//volatile uint16_t iterCount = 0;

// volatile uint8_t lastTempC=255;
// volatile uint16_t lastCapread=4000;
volatile uint8_t lastHour = 0;
volatile uint8_t lastMinute = 0;
volatile uint8_t lastSecond = 0;
volatile uint8_t thisSecond;
volatile uint16_t thisYear;
volatile uint8_t thisMonth;
volatile uint8_t thisDay;
volatile char lightStatus = 'R';

bool useCelcius = false;
int8_t temperatureOffset=0;
volatile bool didTheHalfHour = false;
//volatile bool sentTheData = 0;

int screenWidth;
int screenHeight;

uint8_t screenRotation = 3;


struct soilSensor {
    uint8_t idx;
    String name;
    String group;
    Adafruit_seesaw sensor;
    int muxAddr;
    uint8_t port;
    bool isOnline;
    long xloc;
    long yloc;
} ;

struct plantGroup {
    uint8_t idx;
    String name;
    uint16_t greenThreshold;
    uint16_t yellowThreshold;
    uint16_t overWaterThreshold;
};

typedef vector<soilSensor> soilSensors_t;
soilSensors_t soilSensors;

typedef vector<plantGroup> plantGroups_t;
plantGroups_t plantGroups;


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
 
    pinMode(ledGreen,OUTPUT);
    pinMode(ledYellow,OUTPUT);
    pinMode(ledRed,OUTPUT);  

    //WiFi.disconnect(); 
  
    //delay(1000);    
#ifdef DEBUG
    while(!Serial) {
    }
#endif

    //Wire.setClock(100000); // Drop to 100kHz if things get jumpy
 
    tft.begin();

    tft.setFont(&FreeSansBold12pt7b);    


    tft.setRotation(3);
    screenWidth = tft.width();
    screenHeight = tft.height();
    tft.fillScreen(BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(5, 17);
    tft.println("GREEN THUMB - Ver 20260427a");
#ifdef DEBUG    
    Serial.println("GREEN THUMB - Ver 20260427a");
#endif
    
    tft.setFont(&FreeSans9pt7b);    

#ifdef DEBUG    
    Serial.print("Initializing Touchscreen...");
#endif
    tft.print("Initializing Touchscreen...");
    if (! ts.begin(0x48, &Wire)) {
    #ifdef DEBUG
        Serial.println("failed!");
    #endif
        tft.println("failed!");
        delay(100);
    }
    min_x = TSC_TS_MINX; max_x = TSC_TS_MAXX;
    min_y = TSC_TS_MINY; max_y = TSC_TS_MAXY;

    pinMode(TSC_IRQ, INPUT);
    #ifdef DEBUG
        Serial.println("done.");
    #endif
    tft.println("done.");
    // WiFi.mode(WIFI_STA);
    // WiFi.setHostname("GreenThumbProject");

#ifdef DEBUG    
    Serial.print("Initializing SD card...");
#endif
    tft.print("Initializing SD card...");
        // Initialize the SD card
    if (!SD.begin(SD_CS)) {
#ifdef DEBUG   
        Serial.println("failed!");
#endif
        tft.println("failed");
        return;
    }
#ifdef DEBUG   
    Serial.println("done.");
#endif
    tft.println("done.");

#ifdef DEBUG   
    Serial.print("initialization Soil Sensors...");
#endif
    tft.print("Initializing Soil Sensors...");
    // soilSensors.reserve(10);
    // plantGroups.reserve(10);
    readConfig();
#ifdef DEBUG  
    Serial.println("done.");
#endif
    tft.print("done.");


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
    printWifiStatus(15,false);
    blinkyLights(ledGreen,brightnessLevel,1000);

    configTime(utcOffsetInSeconds, 3600, ntpServer); 

#ifdef DEBUG
    Serial.println("Time synchronized using configTime().");
#endif
    tft.println("Time synchronized using configTime().");

    //attachInterrupt(TSC_IRQ,handleTouchScreen,FALLING);

    fadeInLights(ledGreen, brightnessLevel, delayTime);
    fadeOutLights(ledGreen, brightnessLevel, delayTime);
    fadeInLights(ledYellow, brightnessLevel, delayTime);
    fadeOutLights(ledYellow, brightnessLevel, delayTime);
    fadeInLights(ledRed, brightnessLevel, delayTime);   
    fadeOutLights(ledRed, brightnessLevel, delayTime);

}

void loop() {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    uint8_t thisHour = timeinfo.tm_hour;
    uint8_t thisMinute = timeinfo.tm_min;
    thisSecond = timeinfo.tm_sec;
    thisYear = timeinfo.tm_year+1900;
    thisMonth = timeinfo.tm_mon+1;
    thisDay = timeinfo.tm_mday;
    int isDST = timeinfo.tm_isdst;
    uint8_t lastDisplayContext = displayContext;
    uint8_t lastSelectedIdx = selectedIdx;

    TS_Point p = ts.getPoint();
    if (p.z > 200 && (millis() - lastTouchTime > touchCoolDown)) {
        lastTouchTime = millis();
        //reversed because screen rotation
        tsWhere_y = map(p.x, TSC_TS_MINX, TSC_TS_MAXX, 0, screenHeight );
        tsWhere_x = screenWidth-map(p.y, TSC_TS_MINY, TSC_TS_MAXY, 0, screenWidth );
        
#ifdef DEBUG
        Serial.print("Touched at point ");
        Serial.print(tsWhere_x);
        Serial.print(",");
        Serial.print(tsWhere_y);
        Serial.print("; ");
        Serial.print(p.x);
        Serial.print(",");
        Serial.println(p.y);

#endif
        handleTouchScreen(tsWhere_x,tsWhere_y);

//        tsTouched = false;
    }

    bool needsUpdate = lastDisplayContext != displayContext 
        || ( lastDisplayContext == displayContext && lastSelectedIdx != selectedIdx);

    switch (displayContext) {
    case 1: //config
            showConfig(needsUpdate);
    break;
    case 2: //plant details
            showPlantDetails(needsUpdate);
    break;
    case 3: //group details
            showGroupDetails(needsUpdate);
    break;
    default: //home
        selectedIdx = 0;
        if (lastMinute != thisMinute || lastDisplayContext != 0) {

#ifdef DEBUG
            Serial.println();
            Serial.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);
#endif


    //        sentTheData = postDataToFeed(tempC,capread);

    // #ifdef DEBUG
    //         if (sentTheData==true) 
    //             Serial.println("Sent data.");
    //         else 
    //             Serial.println("Didn't send data.");
    // #endif

            if (thisHour != lastHour) 
                lastHour = thisHour;

            if (thisMinute == 30) 
                didTheHalfHour = 1;
            else 
                didTheHalfHour = 0;

            updateDisplay( thisHour,thisMinute,thisYear,thisMonth,thisDay);
            lastMinute = thisMinute;
            constantLights(onLED, 0, brightnessLevel);
            switch(lightStatus) {
            case 'G':
                onLED= ledGreen;
                break;
            case 'Y':
                onLED= ledYellow;
                break;
            default:
                onLED= ledRed;
            }   
            constantLights(onLED, 1, brightnessLevel);
        }

    }        




}

void handleTouchScreen(long whereX, long whereY) {

    switch (displayContext) {
        case 1: //config
            //home
            if (whereX < 40 && whereY < 40) displayContext = 0; 
            //back
            if (whereX > 40 && whereX < 80 && whereY < 40 && selectedIdx > 0) selectedIdx = 0; 
            //wifi 70
            if (whereY > 40 && whereY < 80) selectedIdx = 1;
            //plants 110
            if (whereY > 80 && whereY < 120) selectedIdx = 2;
            //groups 150
            if (whereY > 120 && whereY < 160) selectedIdx = 3;
            break;
        case 2: //plant
            if (whereX < 40 && whereY < 40) { //back to home
                displayContext = 0;
            }
            //direct to plant group
            selectedIdx = 1;
            break;
        case 3: //group
            if (whereX < 40 && whereY < 40) { //back to home
                displayContext = 0;
            }
            selectedIdx = 1;
            break;
        default:  // 0 or home
            if ( whereX > screenWidth-40 && whereY < 40) {
                displayContext = 1;
                selectedIdx = 0;
                return;         
            }

            for(auto ss : soilSensors) {
                if (ss.isOnline) {
                    if ((whereX > ss.xloc && whereX < ss.xloc+eachFaceX) && (whereY > ss.yloc && whereY < ss.yloc+eachFaceY) ) {
                        displayContext = 2;
                        selectedIdx = ss.idx;
                        break;
                    }
                }
            }

    }
#ifdef DEBUG
    Serial.print("displayContext:");
    Serial.print(displayContext);
    Serial.print("; selectedIdx:");
    Serial.println(selectedIdx);

#endif

}


bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay) {
    uint16_t currentX , currentY;//, spacingX, spacingY, eachFace;
    tft.fillScreen(BLACK);
    tft.setTextColor(CYAN);
    tft.setCursor(5, 30);
  
    tft.setFont(&FreeSansBold18pt7b);
    tft.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);
    
    currentX = 10;
    currentY= 50;
    //eachFace = 140;
    // spacingX=screenWidth/eachFace;
    // spacingY=(screenHeight/eachFace)+25;

    float tempAvg = 0;
    uint8_t sensorCount = 0;

//    tft.setFont(&FreeSansBold12pt7b);    
    lightStatus = 'G';
    for(auto &ss : soilSensors) {
        if (ss.isOnline) {
            pcaselect(ss.muxAddr, ss.port);
            uint16_t capread = ss.sensor.touchRead(0);
            float temp = ss.sensor.getTemp();
            uint16_t greenThreshold,yellowThreshold,overWaterThreshold;
            char status = 'R';
            if ( ! useCelcius ) {
                temp = (temp*1.8) + 32 + temperatureOffset;
            }

            tempAvg += temp;
            sensorCount++;

            for(auto pg : plantGroups) {
                if (pg.name == ss.group) {
                    greenThreshold = pg.greenThreshold;
                    yellowThreshold = pg.yellowThreshold;
                    overWaterThreshold = pg.overWaterThreshold;
                    break;
                }
                else {
                    //use default
                    greenThreshold = 450;
                    yellowThreshold = 200;
                }
            }


            if (capread < yellowThreshold ) {
                status = 'R';
                lightStatus = 'R';
            }
            else if (capread < greenThreshold ) {
                status = 'Y';
                if (lightStatus != 'R')
                    lightStatus = 'Y';
            }
            else if (capread > overWaterThreshold) {
                status = 'B';
                if (lightStatus != 'R')
                    lightStatus = 'Y';
            }
            else {
                status = 'G';
                if (lightStatus != 'Y' && lightStatus != 'R' && lightStatus != 'B') 
                    lightStatus = 'G';
            }



    #ifdef DEBUG
            Serial.print("plantName: ");
            Serial.print(ss.name);
            Serial.print("; status: ");
            Serial.print(status);
            Serial.print("; capread: ");
            Serial.print(capread);
            Serial.print("; temp: ");
            Serial.println(temp);
            Serial.print("currentX: ");
            Serial.print(currentX);
            Serial.print("; currentY: ");
            Serial.print(currentY);
            Serial.print("; screenWidth: ");
            Serial.print(screenWidth);
            Serial.print("; screenHeight: ");
            Serial.println(screenHeight);
            
    #endif
            ss.xloc=currentX;
            ss.yloc=currentY;
            drawFaceStatus(currentX,currentY,status,ss.name,temp,capread);
            currentX += eachFaceX;
            if (currentX+eachFaceX+10 > screenWidth) {
                currentX = 10;
                currentY += eachFaceY;
            }

        }
    }
    
    tft.fillRect(screenWidth-home_size_x, 0, home_size_x, home_size_y,BLACK);
    tft.drawBitmap(screenWidth-home_size_x, 0,configBitmap,home_size_x,home_size_y, CYAN);

    //tempAvg = tempAvg/sensorCount;
    
    // tft.setTextColor(CYAN);
    // tft.setCursor(360, 30);
  
    // tft.setFont(&FreeSansBold18pt7b);
   
    // if (useCelcius)  tft.printf("%.0f°C",tempAvg);
    // else  tft.printf("%.0f°F",tempAvg);


    return true;
}

void drawFaceStatus(int16_t locX, int16_t locY, char lightStatus_l, String plantName, float temp, uint16_t capread ) {
//    uint16_t eyeRad = 6;

    // if (sizePrecentage > .4) 
    //     sizePrecentage = .4;
    
    // if (eyeRad * sizePrecentage > 2) 
    //     eyeRad = eyeRad * sizePrecentage;
    
 //   tft.fillRect(locX, locY, 120*sizePrecentage,120*sizePrecentage,BLACK);
    tft.fillRect(locX, locY, eachFaceX, eachFaceY,BLACK);
    uint16_t xOffset = (eachFaceX-face_size_x)/2;
    switch(lightStatus_l) {
        case 'G':

            tft.drawBitmap(locX+xOffset,locY,happyfaceBitmap,face_size_x,face_size_y,GREEN);
            // tft.drawCircle(locX+(50*sizePrecentage), locY+(50*sizePrecentage), 30*sizePrecentage, GREEN); //smile
            // tft.fillRect(locX, locY, 100*sizePrecentage,66*sizePrecentage,BLACK);
            // tft.drawCircle(locX+(50*sizePrecentage), locY+(50*sizePrecentage),(50*sizePrecentage), GREEN); //head
            // tft.fillCircle(locX+(30*sizePrecentage), locY+(30*sizePrecentage), eyeRad, GREEN);
            // tft.fillCircle(locX+(70*sizePrecentage), locY+(30*sizePrecentage), eyeRad, GREEN);
            tft.setTextColor(GREEN);
            break;
        case 'Y':
            tft.drawBitmap(locX+xOffset,locY,mehfaceBitmap,face_size_x,face_size_y,YELLOW);
            // tft.drawLine(locX+(20*sizePrecentage),locY+(70*sizePrecentage), locX+(80*sizePrecentage),locY+(70*sizePrecentage), YELLOW);
            // tft.drawCircle(locX+(50*sizePrecentage), locY+(50*sizePrecentage) ,50*sizePrecentage, YELLOW);
            // tft.fillCircle(locX+(30*sizePrecentage), locY+(30*sizePrecentage), eyeRad, YELLOW);
            // tft.fillCircle(locX+(70*sizePrecentage), locY+(30*sizePrecentage), eyeRad, YELLOW);
            tft.setTextColor(YELLOW);
            break;
        case 'B':
            tft.drawBitmap(locX+xOffset,locY,mehfaceBitmap,face_size_x,face_size_y,BLUE);
            // tft.drawLine(locX+(20*sizePrecentage),locY+(70*sizePrecentage), locX+(80*sizePrecentage),locY+(70*sizePrecentage), BLUE);
            // tft.drawCircle(locX+(50*sizePrecentage), locY+(50*sizePrecentage),50*sizePrecentage, BLUE);
            // tft.fillCircle(locX+(30*sizePrecentage), locY+(30*sizePrecentage), eyeRad, BLUE);
            // tft.fillCircle(locX+(70*sizePrecentage), locY+(30*sizePrecentage), eyeRad, BLUE);
            tft.setTextColor(BLUE);
            break;
        default:
            tft.drawBitmap(locX+xOffset,locY,sadfaceBitmap,face_size_x,face_size_y,RED);
            // tft.drawCircle(locX+(50*sizePrecentage), locY+(105*sizePrecentage), 40*sizePrecentage, RED); //frown
            // tft.fillRect(locX, locY+(75*sizePrecentage), 100*sizePrecentage,40*sizePrecentage,BLACK);
            // tft.drawCircle(locX+(50*sizePrecentage), locY+(50*sizePrecentage),(50*sizePrecentage), RED); //head
            // tft.fillCircle(locX+(30*sizePrecentage), locY+(30*sizePrecentage), eyeRad, RED);
            // tft.fillCircle(locX+(70*sizePrecentage), locY+(30*sizePrecentage), eyeRad, RED);
            tft.setTextColor(RED);
    } 

    tft.setFont(&FreeSans9pt7b);
    tft.setCursor(locX-(10), locY+face_size_y+13);
    tft.print(plantName);

    tft.setCursor(locX-(10), locY+face_size_y+30);
 
    tft.print("Hydro: ");
    tft.println(capread);
}

void printMessage(int16_t locX, int16_t locY, String message, bool isErrorMessage) {
    tft.fillScreen(BLACK);
    tft.setTextColor(CYAN);
    tft.setCursor(locX, locY);

    tft.setFont(&FreeSansBold12pt7b);    
    if (isErrorMessage == true)
        tft.setTextColor(RED);   

    tft.println(message);
}

// void IRAM_ATTR handleTouchScreen() {
//     tsTouched = 1;

// }    
void showHome() {
    tft.fillRect(0, 0, home_size_x, home_size_y,BLACK);
    tft.drawBitmap(0, 0,homeBitmap,home_size_x,home_size_y, CYAN);
}


void showBack() {
    tft.fillRect(0, 0, home_size_x, home_size_y*2,BLACK);
    tft.drawBitmap(0, 0,homeBitmap,home_size_x,home_size_y, CYAN);
    tft.drawBitmap(41, 0,backBitmap,home_size_x,home_size_y, CYAN);
}

void showConfig(bool needsUpdate) {

    if (needsUpdate) {
    
        tft.fillScreen(BLACK);

        tft.setTextColor(CYAN);

        switch(selectedIdx) {
        case 0:
            showHome();    
            tft.setCursor(45, 30);
            tft.setFont(&FreeSansBold18pt7b);
            tft.print("CONFIGURATION");
            tft.setFont(&FreeSansBold12pt7b);
            tft.setCursor(15, 70);    
            tft.print("* WIFI SETTINGS");
            tft.setCursor(15, 110);    
            tft.print("* PLANTS & SENSORS SETUP");
            tft.setCursor(15, 150);    
            tft.print("* PLANT GROUP SETUP");
        break;
        case 1: //wifi settings
            showBack();    
            tft.setCursor(85, 30);
            tft.setFont(&FreeSansBold18pt7b);
            tft.print("CONFIGURATION>WIFI STATUS");
            printWifiStatus(50,true);
        break;
        case 2: //sensors and plants
            showBack();    
            tft.setCursor(85, 30);
            tft.print("CONFIGURATION>PLANTS & SENSORS");
            tft.fillRect(0,50,screenWidth,screenHeight-50,BLACK);        

        break;
        case 3: //plant groups
            showBack();    
            tft.setCursor(85, 30);
            tft.print("CONFIGURATION>PLANT GROUPS");
            tft.fillRect(0,50,screenWidth,screenHeight-50,BLACK);        
        break;


        }
    }
}




void showPlantDetails(bool needsUpdate) {
    if (needsUpdate) {
        tft.fillScreen(BLACK);
        showHome();
        for(auto ss : soilSensors) {
            if (ss.idx == selectedIdx) {

                break;
            }
        }
    }   
}
void showGroupDetails(bool needsUpdate) {
    if (needsUpdate) {
        tft.fillScreen(BLACK);
        showHome();
        for(auto pg : plantGroups) {
            if (pg.idx == selectedIdx) {
    


                break;
            }
        }
    }
}
void printWifiStatus(int16_t locY, bool showBackButton) {
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
    tft.setFont(&FreeSans9pt7b);
    tft.fillScreen(BLACK);
    //if you want the back button allow for 40x40 in the upper left corner.
    if (showBackButton) showBack(); 
    tft.setTextColor(CYAN);
    tft.setCursor(0, locY);
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
/*

Entries look like this
plant name;plant group;multiplexer address in hexidecimal;port that sensor is on
*/
bool readConfig() {
    if (!green_thumb_conf)
        green_thumb_conf = SD.open(CONF_FILE, FILE_READ);

    if (!green_thumb_conf.available()) {     
#ifdef DEBUG
        Serial.println("Config file is not available.");
#endif
        tft.println("Cannot open gtconf.ini from sd card.");

        return false;
    }

    uint8_t plantCount = 0;
    uint8_t groupCount = 0;

    while (green_thumb_conf.available()) {
        String line = green_thumb_conf.readStringUntil('\n');
//        uint8_t columnIdx = 0;
        uint16_t idx1,idx2;

       
        int mux_addr;
        uint8_t port_number;
        uint16_t greenThreshold,yellowThreshold,overWaterThreshold;
        line.trim();

#ifdef DEBUG
            Serial.println(line);
#endif
        if (line.charAt(0) == ';') 
            continue;

        if (line.indexOf("temperatureUnits=celcius") > -1) 
            useCelcius = true;
        
        if (line.indexOf("temperatureOffest=") > -1) { 
            String value = line.substring(line.indexOf('=')+1);
            temperatureOffset = (int8_t)value.toInt();
        }
        
        if (line.indexOf("soilsensor=") > -1) {
            //11
#ifdef DEBUG
            Serial.println("soilsensor");
#endif       

            idx1 = line.indexOf('=')+1;
            idx2 = line.indexOf(',');

            String name=line.substring(idx1,idx2);
            idx1=idx2+1;
            idx2 = line.indexOf(',',idx1);
            String groupname=line.substring(idx1,idx2);
            idx1=idx2+1;
            idx2 = line.indexOf(',',idx1);
            String value=line.substring(idx1,idx2);
            mux_addr = (int)strtol(value.c_str(), NULL, 0); 
            idx1=idx2+1;
            value=line.substring(idx1);
            port_number = (uint8_t)value.toInt();


#ifdef DEBUG
            Serial.printf("name=%s; plant group=%s; mux_addr=%x; port_number=%d;\n",name.c_str(),groupname.c_str(),mux_addr,port_number);
#endif

            if (mux_addr >= 0x70 && mux_addr <= 0x77 && port_number >= 0 && port_number <= 7) {
                soilSensors.push_back({plantCount++,name,groupname,Adafruit_seesaw(&Wire),mux_addr,port_number});
/*
                soilSensor newSensor;
                newSensor.name = name;
                newSensor.muxAddr = mux_addr;
                newSensor.port = port_number;
                newSensor.isOnline = 1;
                soilSensors.push_back(newSensor);
                soilSensors.back().sensor = Adafruit_seesaw(&Wire);
*/
                
                auto& lastSensor = soilSensors.back();
                pcaselect(lastSensor.muxAddr,lastSensor.port);

                if (lastSensor.sensor.begin(0x36))
                    lastSensor.isOnline = 1;
                else
                    lastSensor.isOnline = 0;
#ifdef DEBUG
                Serial.printf("name=%s; plant group=%s; mux_addr=%x; port_number=%u;is online=%d\n",name.c_str(),groupname.c_str(),mux_addr,port_number,soilSensors.back().isOnline);
#endif
            }
            else {
                //need error handling.
            }
            
        } // if (line.indexOf("soilsensor=") > -1) {
        if (line.indexOf("plantgroup=") > -1) {
#ifdef DEBUG
            Serial.println("plantgroup");
#endif             
            idx1 = line.indexOf('=')+1;
            idx2 = line.indexOf(',');

            String groupname=line.substring(idx1,idx2);
            
            idx1=idx2+1;
            idx2 = line.indexOf(',',idx1);
            String value=line.substring(idx1,idx2);
            greenThreshold = (uint16_t)value.toInt();
            
            idx1=idx2+1;
            idx2 = line.indexOf(',',idx1);
            value=line.substring(idx1,idx2);
            yellowThreshold = (uint16_t)value.toInt();

            idx1=idx2+1;
            value=line.substring(idx1);
            overWaterThreshold = (uint16_t)value.toInt();
 
 
 
#ifdef DEBUG
            Serial.printf("plant group=%s; green threshold=%u; yellow threshold=%u; ,over-water threshold=%u;\n",groupname.c_str(),greenThreshold,yellowThreshold,overWaterThreshold);
#endif
            

            if (overWaterThreshold > greenThreshold && greenThreshold>yellowThreshold && yellowThreshold > 0) {
                plantGroups.push_back({groupCount++,groupname,greenThreshold,yellowThreshold,overWaterThreshold});
            }
            else {
                //need error handling.
                
            }
            
        }//if (line.indexOf("plantgroup=") > -1)

    } //while (green_thumb_conf.available()) {
    green_thumb_conf.close();
    return true;
}

void pcaselect(int mux_addr, uint8_t port) {
    if (port > 7) return;
    Wire.beginTransmission(mux_addr); 
    Wire.write(0);
    Wire.write(1 << port);
    Wire.endTransmission();
}

