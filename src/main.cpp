#define DEBUG
//#define DEBUG2


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

void handleTouchScreen();
void printWifiStatus();
void digitalClockDisplay(time_t now);
void printDigits(int digits);
bool postDataToFeed(uint8_t tempC,uint16_t capread);
//bool updateThresholds(uint16_t &green, uint16_t &yellow);
bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay);
void printMessage(int16_t locX, int16_t locY, String message, bool isErrorMessage);
void drawFaceStatus(int16_t locX, int16_t locY, float sizePrecentage, char lightStatus_l, String plantName, float temp, uint16_t capread );
bool readConfig();
void pcaselect(int mux_addr, uint8_t port);
void getSensorData();

//Adafruit_seesaw soilSensor;
using namespace std;

File green_thumb_conf;

Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
Adafruit_TSC2007 ts = Adafruit_TSC2007();
int16_t min_x, max_x, min_y, max_y;
volatile int16_t tsWhere_x,tsWhere_y,tsPressure;
volatile bool tsTouched = false;


//const int ss_addr = 0x36;
const int ts_addr = 0x48;
//const int mx_addr = 0x70;
const uint8_t ledGreen = A5;
const uint8_t ledYellow = A4;
const uint8_t ledRed = A3;


const uint8_t postThreshold = 5;

volatile uint8_t onLED;

uint8_t brightnessLevel = 128;
uint16_t delayTime = 600;

// volatile uint8_t lastTempC=255;
// volatile uint16_t lastCapread=4000;
volatile uint8_t lastHour = 0;
volatile uint8_t lastMinute = 0;
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
    String name;
    String group;
    Adafruit_seesaw sensor;
    int muxAddr;
    uint8_t port;
    bool isOnline;
    float tempC;
    uint16_t capread;
    char status;
} ;

struct plantGroup {
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


    if (! ts.begin(0x48, &Wire)) {
    #ifdef DEBUG
        Serial.println("Couldn't start TSC2007 touchscreen controller");
    #endif
        while (1) delay(100);
    }
    min_x = TSC_TS_MINX; max_x = TSC_TS_MAXX;
    min_y = TSC_TS_MINY; max_y = TSC_TS_MAXY;

    pinMode(TSC_IRQ, INPUT);
    #ifdef DEBUG
        Serial.println("Touchscreen started");
    #endif
 
    tft.begin();

    tft.setFont(&FreeSansBold12pt7b);    


    tft.setRotation(3);
    screenWidth = tft.width();
    screenHeight = tft.height();
    tft.fillScreen(BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(5, 17);
    tft.println("GREEN THUMB - Ver 20260424c");
#ifdef DEBUG    
    Serial.println("GREEN THUMB - Ver 20260424c");
#endif

    tft.setFont(&FreeSans9pt7b);    
    // WiFi.mode(WIFI_STA);
    // WiFi.setHostname("GreenThumbProject");

#ifdef DEBUG    
    Serial.print("Initializing SD card...");
#endif
        // Initialize the SD card
    if (!SD.begin(SD_CS)) {
#ifdef DEBUG   
        Serial.println("initialization failed!");
#endif
        return;
    }
#ifdef DEBUG   
    Serial.println("initialization done.");
#endif

    tft.print("Initializing Soil Sensors:");
    // soilSensors.reserve(10);
    // plantGroups.reserve(10);
    readConfig();
    Serial.println("initialization done.");

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

    attachInterrupt(TSC_IRQ,handleTouchScreen,FALLING);

    fadeInLights(ledGreen, brightnessLevel, delayTime);
    fadeOutLights(ledGreen, brightnessLevel, delayTime);
    fadeInLights(ledYellow, brightnessLevel, delayTime);
    fadeOutLights(ledYellow, brightnessLevel, delayTime);
    fadeInLights(ledRed, brightnessLevel, delayTime);   
    fadeOutLights(ledRed, brightnessLevel, delayTime);

}

void loop() {
#ifdef DEBUG2
    Serial.print("Loop");
    delay(200);
#endif

    struct tm timeinfo;
    getLocalTime(&timeinfo);
    uint8_t thisHour = timeinfo.tm_hour;
    uint8_t thisMinute = timeinfo.tm_min;
    thisSecond = timeinfo.tm_sec;
    thisYear = timeinfo.tm_year+1900;
    thisMonth = timeinfo.tm_mon+1;
    thisDay = timeinfo.tm_mday;
    int isDST = timeinfo.tm_isdst;

    if (tsTouched == 1) {
#ifdef DEBUG
        Serial.printf("Touched at point %d,%d; Pressure: %d \n", tsWhere_x, tsWhere_y, tsPressure);
#endif
    }


    if (lastMinute != thisMinute) {

#ifdef DEBUG
        Serial.println();
        Serial.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);
#endif

        getSensorData();


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
    }

    switch(lightStatus) {
    case 'G':
        fadeOutLights(onLED, brightnessLevel, delayTime);
        onLED= ledRed;
        fadeInLights(onLED, brightnessLevel, delayTime);
         break;
    case 'Y':
        fadeOutLights(onLED, brightnessLevel, delayTime);
        onLED= ledYellow;
        fadeInLights(onLED, brightnessLevel, delayTime); 
        break;
    default:
        fadeOutLights(onLED, brightnessLevel, delayTime);
        onLED= ledGreen;
        fadeInLights(onLED, brightnessLevel, delayTime);
    }


}


bool updateDisplay(uint8_t thisHour, uint8_t thisMinute, uint16_t thisYear ,uint8_t thisMonth,uint8_t thisDay) {
    uint16_t currentX , currentY, spacingX, spacingY, eachFace;
    tft.fillScreen(BLACK);
    tft.setTextColor(CYAN);
    tft.setCursor(5, 30);
  
    tft.setFont(&FreeSansBold18pt7b);
    tft.printf("%d-%02d-%02d %02d:%02d\n", thisYear, thisMonth, thisDay, thisHour, thisMinute);
    
    currentX = 10;
    currentY= 100;
    eachFace = 130;
    spacingX=screenWidth/eachFace;
    spacingY=(screenHeight/eachFace)+25;

//    tft.setFont(&FreeSansBold12pt7b);    
    lightStatus = 'G';
    for(auto ss : soilSensors) {
        if (ss.isOnline) {

            float temp = ss.tempC;

            if ( ! useCelcius ) {
                temp = (ss.tempC*1.8) + 32 + temperatureOffset;
            }

            if (ss.status == 'R') lightStatus = 'R';
            else if (lightStatus != 'R' && ss.status == 'Y') lightStatus = 'Y';

    #ifdef DEBUG
            Serial.print("plantName: ");
            Serial.print(ss.name);
            Serial.print("; ss.status: ");
            Serial.print(ss.status);
            Serial.print("; ss.capread: ");
            Serial.print(ss.capread);
            Serial.print("; ss.tempC: ");
            Serial.println(ss.tempC);
            Serial.print("currentX: ");
            Serial.print(currentX);
            Serial.print("; currentY: ");
            Serial.print(currentY);
            Serial.print("; screenWidth: ");
            Serial.print(screenWidth);
            Serial.print("; screenHeight: ");
            Serial.println(screenHeight);
            
    #endif
            drawFaceStatus(currentX,currentY,1,ss.status,ss.name,temp,ss.capread);
            currentX += eachFace;
            if (currentX+eachFace+10 > screenWidth) {
                currentX = 10;
                currentY += eachFace;
            }
        }
    }


    return true;
}

void drawFaceStatus(int16_t locX, int16_t locY, float sizePrecentage, char lightStatus_l, String plantName, float temp, uint16_t capread ) {
    uint16_t eyeRad = 6;

    if (sizePrecentage > .4) 
        sizePrecentage = .4;
    
    if (eyeRad * sizePrecentage > 2) 
        eyeRad = eyeRad * sizePrecentage;
    
    tft.fillRect(locX-(50*sizePrecentage), locY+(50*sizePrecentage), 120*sizePrecentage,120*sizePrecentage,BLACK);
     
    switch(lightStatus_l) {
        case 'G':
            tft.drawCircle(locX, locY, 32*sizePrecentage, GREEN); //smile
            tft.fillRect(locX-(50*sizePrecentage), locY+(50*sizePrecentage), 100*sizePrecentage,66*sizePrecentage,BLACK);
            tft.drawCircle(locX, locY,(50*sizePrecentage), GREEN); //head
            tft.fillCircle(locX-(20*sizePrecentage), locY+(25*sizePrecentage), eyeRad, GREEN);
            tft.fillCircle(locX+(20*sizePrecentage), locY+(25*sizePrecentage), eyeRad, GREEN);
            tft.setTextColor(GREEN);
            break;
        case 'Y':
            tft.drawLine(locX-(20*sizePrecentage),locY-(25*sizePrecentage), locX+(20*sizePrecentage),locY-(25*sizePrecentage), YELLOW);
            tft.drawCircle(locX, locY,50*sizePrecentage, YELLOW);
            tft.fillCircle(locX-(20*sizePrecentage), locY+(25*sizePrecentage), eyeRad, YELLOW);
            tft.fillCircle(locX+(20*sizePrecentage), locY+(25*sizePrecentage), eyeRad, YELLOW);
            tft.setTextColor(YELLOW);
            break;
        default:
            tft.drawCircle(locX, locY-(75*sizePrecentage), 32*sizePrecentage, RED); //frown
            tft.fillRect(locX-(50*sizePrecentage), locY-(32*sizePrecentage), 100*sizePrecentage,64*sizePrecentage,BLACK);
            tft.drawCircle(locX, locY,(50*sizePrecentage), RED); //head
            tft.fillCircle(locX-(20*sizePrecentage), locY+(25*sizePrecentage), eyeRad, RED);
            tft.fillCircle(locX+(20*sizePrecentage), locY+(25*sizePrecentage), eyeRad, RED);
            tft.setTextColor(RED);
    } 

    tft.setFont(&FreeSans9pt7b);
    tft.setCursor(locX-(10*sizePrecentage), locY-(50*sizePrecentage)+3);
    tft.print(plantName);

    tft.setCursor(locX-(10*sizePrecentage), locY-(50*sizePrecentage)+10);
    tft.print("Temp: ");
    tft.print(temp);
    if (useCelcius) tft.print("°C");
    else tft.print("°F");
    tft.setCursor(locX-(10*sizePrecentage), locY-(50*sizePrecentage)+17);

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

/*
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
*/

/*
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
*/

void handleTouchScreen() {
    TS_Point p = ts.getPoint();
    tsWhere_x = map(p.x, TSC_TS_MINX, TSC_TS_MAXX, 0, screenWidth);
    tsWhere_y = map(p.y, TSC_TS_MINY, TSC_TS_MAXY, 0, screenHeight);
    tsPressure = p.z;
    tsTouched = 1;
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
    tft.setFont(&FreeSans9pt7b);
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
            // String name="";
            // String groupname="";
            // String value="";     

//            columnIdx = 0;
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

//             for (int i =11; i<line.length(); i++) {



//                 if (line.charAt(i) == ',') {
                  
//                     if (columnIdx == 0) 
//                         name = line.substring(idx1,i);
//                     else if (columnIdx == 1) 
//                         groupname = line.substring(idx1,i);
//                     else if (columnIdx == 2) {
//                         value = line.substring(idx1,i);
//                         mux_addr = (int)strtol(value.c_str(), NULL, 0); 
//                     }
//                     idx1 = i+1;
//                     columnIdx++;
//                 }
// #ifdef DEBUG2
//                 Serial.printf("idx1=%d; i=%d; columnIdx=%d;\n",idx1,i,columnIdx);
// #endif
//             } 
//             if (columnIdx == 3) {
//                 value = line.substring(idx1);
//                 port_number = (uint8_t)value.toInt();
//             }
#ifdef DEBUG
            Serial.printf("name=%s; plant group=%s; mux_addr=%x; port_number=%d;\n",name.c_str(),groupname.c_str(),mux_addr,port_number);
#endif

            if (mux_addr >= 0x70 && mux_addr <= 0x77 && port_number >= 0 && port_number <= 7) {
                soilSensors.push_back({name,groupname,Adafruit_seesaw(&Wire),mux_addr,port_number});
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
#endif             //;plantgroup=plant group name,green threshold,yellow threshold,over-water threshold
            //columnIdx = 0;
            //idx1 = 11;
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
 
 
 
//             for (int i =11; i<line.length(); i++) {
//                 if (line.charAt(i) == ',') {
//                     if (columnIdx == 0) 
//                         groupname = line.substring(idx1,i);
//                     else if (columnIdx == 1) {
//                         value = line.substring(idx1,i);
//                         greenThreshold = (uint16_t)value.toInt();
//                     }
//                     else if (columnIdx == 2) {
//                         value = line.substring(idx1,i);
//                         yellowThreshold = (uint16_t)value.toInt();
//                     }                    
//                     idx1 = i+1;
//                     columnIdx++;
//                 }
// #ifdef DEBUG2
//                 Serial.printf("idx1=%d; i=%d; columnIdx=%d;\n",idx1,i,columnIdx);
// #endif
//             } 
//             if (columnIdx == 3) {
//                 value = line.substring(idx1);
//                 overWaterThreshold = (uint16_t)value.toInt();
//             }
#ifdef DEBUG
            Serial.printf("plant group=%s; green threshold=%u; yellow threshold=%u; ,over-water threshold=%u;\n",groupname.c_str(),greenThreshold,yellowThreshold,overWaterThreshold);
#endif
            

            if (overWaterThreshold > greenThreshold && greenThreshold>yellowThreshold && yellowThreshold > 0) {
                plantGroups.push_back({groupname,greenThreshold,yellowThreshold,overWaterThreshold});
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




void getSensorData() {
    //   uint8_t tempC = (soilSensor.getTemp()*1.8)+27;
    //   uint16_t capread = soilSensor.touchRead(0);
    uint16_t greenThreshold,yellowThreshold,overWaterThreshold;
    for(auto ss : soilSensors) {
        if (ss.isOnline) {
            pcaselect(ss.muxAddr, ss.port);
            ss.capread = ss.sensor.touchRead(0);
            ss.tempC = ss.sensor.getTemp();

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

            
            if (ss.capread < yellowThreshold ) {
                ss.status = 'R';
                if (lightStatus != 'R')
                    lightStatus = 'R';
            }
            else if (ss.capread < greenThreshold ) {
                ss.status = 'Y';
                if (lightStatus != 'Y' && lightStatus != 'R')
                    lightStatus = 'Y';
            }
            else if (ss.capread > overWaterThreshold) {
                ss.status = 'R';
                if (lightStatus != 'Y' && lightStatus != 'R')
                    lightStatus = 'Y';
            }
            else {
                ss.status = 'G';
                if (lightStatus != 'Y' && lightStatus != 'R') 
                    lightStatus = 'G';
            }

// #ifdef DEBUG
//             Serial.print("ss.name: ");
//             Serial.print(ss.name);
//             Serial.print("; ss.status: ");
//             Serial.print(ss.status);
//             Serial.print("; ss.capread: ");
//             Serial.print(ss.capread);
//             Serial.print("; ss.tempC: ");
//             Serial.println(ss.tempC);
// #endif            
        }
    }
}