#include <M5Unified.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>  // Required for NTPClient
#include <NTPClient.h> // NTP time sync
#include "EGR425_Phase1_weather_bitmap_images.h"
#include <WiFi.h>

////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////

// OpenWeather API variables
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "e969174f41509785bfde66d63dee09ae";  // Replace with your own API key

// WiFi Credentials
String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "L@ncerN@tion";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;  // Refresh weather every 5 seconds

// Temperature mode (Fahrenheit or Celsius)
bool isFahrenheit = true;

// WiFi UDP for time syncing
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -21600, 60000); // ✅ Fixed missing semicolon!

// Store last sync timestamp
String lastSyncTime = "--:--:--";

// LCD dimensions
int sWidth;
int sHeight;

// Weather data variables
String strWeatherIcon;
String strWeatherDesc;
String cityName;
double tempNow;
double tempMin;
double tempMax;

// Function declarations
String httpGETRequest(const char* serverName);
void drawWeatherImage(String iconId, int resizeMult);
void fetchWeatherDetails();
void drawWeatherDisplay();

///////////////////////////////////////////////////////////////
// Setup: Runs once at startup
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();
    
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

    // Reset back to F using Button B
    // if (M5.BtnB.wasPressed()) {
    //     isFahrenheit = true;
    //     drawWeatherDisplay();
    //     delay(300);
    // }

    if ((millis() - lastTime) > timerDelay) {
        if (WiFi.status() == WL_CONNECTED) {
            fetchWeatherDetails();  // Get new weather data
            drawWeatherDisplay();   // Update the screen
        } else {
            Serial.println("WiFi Disconnected");
        }
        lastTime = millis(); // Update timer
    }
}

/////////////////////////////////////////////////////////////////
// Fetch weather details from OpenWeather API and update variables
/////////////////////////////////////////////////////////////////
void fetchWeatherDetails() {
    // API request URL (fetches weather for Des Moines, IA)
    String serverURL = urlOpenWeather + "q=des+moines,ia,usa&units=imperial&appid=" + apiKey;
    
    // Perform HTTP GET request
    String response = httpGETRequest(serverURL.c_str());

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

    // ✅ Get updated time from NTP
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
    int pad = 10;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.setTextSize(3);

    // Convert temperatures if needed
    float tempNowDisplay = isFahrenheit ? tempNow : (tempNow - 32) * 5.0 / 9.0;
    float tempMinDisplay = isFahrenheit ? tempMin : (tempMin - 32) * 5.0 / 9.0;
    float tempMaxDisplay = isFahrenheit ? tempMax : (tempMax - 32) * 5.0 / 9.0;
    String unit = isFahrenheit ? "F" : "C";

    // Display temperatures and city name
    M5.Lcd.printf("LO:%0.1f%s\n", tempMinDisplay, unit.c_str());
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(10);
    M5.Lcd.printf("%0.1f%s\n", tempNowDisplay, unit.c_str());
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("HI:%0.1f%s\n", tempMaxDisplay, unit.c_str());
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.printf("%s\n", cityName.c_str());

    // Show last sync timestamp at the bottom of the screen
    M5.Lcd.setCursor(10, sHeight - 30);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Last Sync: %s", lastSyncTime.c_str());
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

// Added missing drawWeatherImage function
void drawWeatherImage(String iconId, int resizeMult) {
    Serial.printf("Drawing weather icon: %s with scale %d\n", iconId.c_str(), resizeMult);
}
