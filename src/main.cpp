#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>  // Required for NTPClient
#include <NTPClient.h> // NTP time sync
#include "EGR425_Phase1_weather_bitmap_images.h"
#include <WiFi.h>
#include "../include/I2C_RW.h"


///////////////////////////////////////////////////////////////
// Register defines
///////////////////////////////////////////////////////////////
#define VCNL_I2C_ADDRESS 0x60
#define VCNL_REG_PROX_DATA 0x08
#define VCNL_REG_ALS_DATA 0x09
#define VCNL_REG_WHITE_DATA 0x0A

#define VCNL_REG_PS_CONFIG 0x03
#define VCNL_REG_ALS_CONFIG 0x00
#define VCNL_REG_WHITE_CONFIG 0x04

/////////////////////////////////////////////
// OpenWeather API variables
/////////////////////////////////////////////
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "e969174f41509785bfde66d63dee09ae";
String defaultZipCode = "93314";
String zipcodeInput = "";
enum screenState {NORMAL, ZIP_CODE};
int screenWidth = M5.Lcd.width();
int screenHeight = M5.Lcd.height();

/////////////////////////////////////////////
// Initialize variables
/////////////////////////////////////////////
static screenState currentState = NORMAL;
static bool stateChangedThisLoop = false;
int firstNumberWidth = screenWidth / 4;
int numberHeight = screenHeight * .15;
int num[5] = {0, 0, 0, 0, 0};
int arraySize = 5;
int arrowSize = 40;
int spacing = 20;

/////////////////////////////////////////////
// Variables for version 3 
/////////////////////////////////////////////
int const PIN_SDA = 32;
int const PIN_SCL = 33;

String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "L@ncerN@tion";
/////////////////////////////////////////////
// Timer variables
/////////////////////////////////////////////
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;  // Refresh weather every 5 seconds

/////////////////////////////////////////////
// Temperature mode (Fahrenheit or Celsius)
/////////////////////////////////////////////
bool isFahrenheit = true;

/////////////////////////////////////////////
// WiFi UDP for time syncing
/////////////////////////////////////////////
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -21600, 60000); 

/////////////////////////////////////////////
// Store last sync timestamp
/////////////////////////////////////////////
String lastSyncTime = "--:--:--";

/////////////////////////////////////////////
// LCD dimensions
/////////////////////////////////////////////
int sWidth;
int sHeight;

/////////////////////////////////////////////
// Weather data variables
/////////////////////////////////////////////
String strWeatherIcon;
String strWeatherDesc;
String cityName;
double tempNow;
double tempMin;
double tempMax;

/////////////////////////////////////////////
// Function declarations
/////////////////////////////////////////////
String httpGETRequest(const char* serverName);
void drawWeatherImage(String iconId, int resizeMult);
void fetchWeatherDetails();
void drawWeatherDisplay();
void drawZipCodeDisplay();
void handleTouch();
void getRoomHumidityAndTemp();
void getAbmientLightAndProximity();
void changeLCDProperties(int prox, int als);

///////////////////////////////////////////////////////////////
// Setup: Runs once at startup
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();

    // Initialize I2C
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
    delay(1000);
    //I2C_RW::scanI2cLinesForAddresses(true);

    // Write registers to initialize/enable VCNL sensors
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_WHITE_CONFIG, 2, 0x0000, " to enable raw white light sensor", true);
    
    // Get screen width and height
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    // Connect to WiFi
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize NTP time sync
    timeClient.begin();
    timeClient.update(); // Get initial time

    zipcodeInput = defaultZipCode;

    
}

///////////////////////////////////////////////////////////////
// Loop: Runs continuously
///////////////////////////////////////////////////////////////
void loop() {
    M5.update();

    // To go from F to C using Button C
    if (M5.BtnC.wasPressed()) {  
        isFahrenheit = !isFahrenheit;
        drawWeatherDisplay();
    }
    
    if (M5.BtnA.wasPressed()) {
        Serial.println("Button A Pressed");
        stateChangedThisLoop = true;
        if (currentState == NORMAL && stateChangedThisLoop) {
            currentState = ZIP_CODE;
            lastTime = millis();
            drawZipCodeDisplay();
        } else {
            zipcodeInput = "";
            for (int i = 0; i < arraySize; i++) {
                zipcodeInput += String(num[i]);
            }
            
            Serial.printf("New zipcode: %d",zipcodeInput);

            currentState = NORMAL;
            fetchWeatherDetails();
            drawWeatherDisplay();
        }
        
    }
    handleTouch();
    if (currentState == NORMAL)
    {

        // Only execute every so often
        if ((millis() - lastTime) > timerDelay)
        {
            if (WiFi.status() == WL_CONNECTED)
            {

                fetchWeatherDetails();
                drawWeatherDisplay();
            }
            else
            {
                Serial.println("WiFi Disconnected");
            }

            // Update the last time to NOW
            lastTime = millis();
        }
    }
    getAbmientLightAndProximity();

}

/////////////////////////////////////////////////////////////////
// Fetch weather details from OpenWeather API and update variables
/////////////////////////////////////////////////////////////////
void fetchWeatherDetails() {

    //////////////////////////////////////////////////////////////////
    // Hardcode the specific city,state,country into the query
    // Examples: https://api.openweathermap.org/data/2.5/weather?q=riverside,ca,usa&units=imperial&appid=YOUR_API_KEY
    //////////////////////////////////////////////////////////////////
    String serverURL = urlOpenWeather + "zip=" + zipcodeInput +",US&units=imperial&appid=" + apiKey;
    
    Serial.println(serverURL); // Debug print

    //////////////////////////////////////////////////////////////////
    // Make GET request and store reponse
    //////////////////////////////////////////////////////////////////
    String response = httpGETRequest(serverURL.c_str());
    //Serial.print(response); // Debug print
    // Allocate JSON buffer for parsing response
    const size_t jsonCapacity = 768 + 250;
    DynamicJsonDocument objResponse(jsonCapacity);

    // Deserialize JSON
    DeserializationError error = deserializeJson(objResponse, response);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // Parse weather details
    JsonArray arrWeather = objResponse["weather"];
    JsonObject objWeather0 = arrWeather[0];
    strWeatherDesc = objWeather0["main"].as<String>();
    strWeatherIcon = objWeather0["icon"].as<String>();
    cityName = objResponse["name"].as<String>();

    // Parse temperature details
    JsonObject objMain = objResponse["main"];
    tempNow = objMain["temp"];
    tempMin = objMain["temp_min"];
    tempMax = objMain["temp_max"];

    Serial.printf("NOW: %.1f F and %s\tMIN: %.1f F\tMAX: %.1f F\n", tempNow, strWeatherDesc, tempMin, tempMax);

    // Get updated time from NTP
    timeClient.update();
    lastSyncTime = timeClient.getFormattedTime();
}

/////////////////////////////////////////////////////////////////
// Draw weather data on screen
/////////////////////////////////////////////////////////////////
void drawWeatherDisplay() {
    // Set background color based on day or night
    uint16_t primaryTextColor;
    if (strWeatherIcon.indexOf("d") >= 0) {
        M5.Lcd.fillScreen(TFT_CYAN);
        primaryTextColor = TFT_DARKGREY;
    } else {
        M5.Lcd.fillScreen(TFT_NAVY);
        primaryTextColor = TFT_WHITE;
    }

    drawWeatherImage(strWeatherIcon, 3);

    // Padding for text display
    int pad = 20;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(4);
    M5.Lcd.printf("%s\n", cityName.c_str());

    // Convert temperatures if needed
    float tempNowDisplay = isFahrenheit ? tempNow : (tempNow - 32) * 5.0 / 9.0;
    float tempMinDisplay = isFahrenheit ? tempMin : (tempMin - 32) * 5.0 / 9.0;
    float tempMaxDisplay = isFahrenheit ? tempMax : (tempMax - 32) * 5.0 / 9.0;
    String unit = isFahrenheit ? "F" : "C";

    // Display temperatures and city name
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(4);
    M5.Lcd.printf("%0.1f%s\n", tempNowDisplay, unit.c_str());
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("HI:%0.1f%s\n", tempMaxDisplay, unit.c_str());
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("LO:%0.1f%s\n", tempMinDisplay, unit.c_str());

    // Show last sync timestamp at the bottom of the screen
    M5.Lcd.setCursor(10, sHeight - 30);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Last Sync: %s", lastSyncTime.c_str());
}


/////////////////////////////////////////////////////////////////
// Draw the zip code display
/////////////////////////////////////////////////////////////////
void drawZipCodeDisplay() {
    uint16_t primaryTextColor = TFT_WHITE;
    M5.Lcd.fillScreen(TFT_BLUE); 
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(20);
    for (int i = 0; i < arraySize; i ++) {
        int xpos = firstNumberWidth * i + firstNumberWidth / 2;
        M5.Lcd.setCursor(xpos, numberHeight);
        M5.Lcd.printf("^");
        M5.Lcd.setCursor(xpos, numberHeight + 40);
        M5.Lcd.printf("%d", num[i]);
        M5.Lcd.setCursor(xpos, numberHeight + 90);
        M5.Lcd.printf("v");
    }
    stateChangedThisLoop = false;
}

void handleTouch() {
    if (M5.Touch.ispressed()) {
        TouchPoint_t touch = M5.Touch.getPressPoint();

        for (int i = 0; i < arraySize; i++) {
            int xpos = firstNumberWidth * i;
            int ypos = numberHeight;
            if (touch.x > xpos && touch.x < xpos + firstNumberWidth && touch.y > ypos && touch.y < ypos + arrowSize) {
                num[i] = (num[i] + 1) % 10;
                delay(50);
                drawZipCodeDisplay();
                break;
            }
            ypos = numberHeight + 90;
            if (touch.x > xpos && touch.x < xpos + firstNumberWidth && touch.y > ypos && touch.y < ypos + arrowSize) {
                num[i] = (num[i] - 1) % 10;
                if (num[i] < 0) {
                    num[i] = 9;
                }
                drawZipCodeDisplay();
                delay(50);
                break;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////
// Perform HTTP GET request and return response
/////////////////////////////////////////////////////////////////
String httpGETRequest(const char* serverURL) {
    HTTPClient http;
    http.begin(serverURL);
    
    int httpResponseCode = http.GET();
    String response = http.getString();

    if (httpResponseCode > 0)
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    else {
        Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
        Serial.printf("Server Response: %s\n", response.c_str());
    }

    http.end();
    return response;
}

void drawWeatherImage(String iconId, int resizeMult) {

    // Get the corresponding byte array
    const uint16_t * weatherBitmap = getWeatherBitmap(iconId);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
    int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
    //int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally
    
    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++) {
        for (int x = 0; x < imgSqDim; x++) {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = weatherBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0) {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++) {
                    for (int j = 0; j < resizeMult; j++) {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}

void getRoomHumidityAndTemp() {

}

///////////////////////////////////////////////////////////////
// Get ambient light and proximity data
///////////////////////////////////////////////////////////////
void getAbmientLightAndProximity() {
    // Read proximity data
    int prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
    Serial.printf("Proximity: %d\n", prox);
    
    // Read ambient light data
    int als = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
    als = als * 0.1; // See pg 12 of datasheet - we are using ALS_IT (7:6)=(0,0)=80ms integration time = 0.10 lux/step for a max range of 6553.5 lux
    Serial.printf("Ambient Light: %d\n", als);

    changeLCDProperties(prox, als);

}

void changeLCDProperties(int prox, int als) {
    
    // change brightness based off of ambient light
    if (als > 300) {
        M5.Axp.SetLcdVoltage(2900);
    } else if (als <= 300 && als > 150) {
        M5.Axp.SetLcdVoltage(2800);
    } else if (als <= 150) {
        M5.Axp.SetLcdVoltage(2700);
    }

    // Change brightness based off of proximity
    if (prox > 150) {
        uint8_t brightness = 1;
        M5.Lcd.sleep();
    } else {
        uint8_t brightness = 255;
        M5.Lcd.wakeup();
    } 
    
}