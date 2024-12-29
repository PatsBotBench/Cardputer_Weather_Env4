#include <M5Cardputer.h>
#include <M5GFX.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <JSON_Decoder.h>
#include <OpenWeather.h>
#include <SD.h>

#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h> 

#include <M5UnitENV.h>   // For ENV sensor (SHT30 & BMP280)
// Create sensor objects
SHT4X sht4;
BMP280 bmp;

// SD card pins
#define SD_SCK    40
#define SD_MISO   39
#define SD_MOSI   14
#define SD_SS     12

// SD SPI frequency
#define SD_SPI_FREQ 1000000  // Example frequency, adjust as needed

// Configuration variables
char api_key[64];
char latitude[16];
char longitude[16];
char units[16];
char language[16];
char wifi_ssid[32];
char wifi_password[32];

#define TIME_OFFSET 1UL * -4 * 3600UL
#define NUM_REPETITIONS 3
#define DELAY_BETWEEN_PAGES 5000

SPIClass* hspi = new SPIClass(HSPI);

OW_Weather ow;

#define DST_ACTIVE true // Set to true if DST is active, false otherwise
#define DST_OFFSET -3600 // DST offset: subtract 1 hour (3600 seconds)

float seaLevelPressure = 101325.0; // Default sea-level pressure in Pa
float startAltitude = 0.0; // Reference altitude

bool initializeSDCard() {
  // Begin the SPI interface
  hspi->begin(SD_SCK, SD_MISO, SD_MOSI, SD_SS);

  // Initialize the SD card
  if (!SD.begin(SD_SS, *hspi, SD_SPI_FREQ)) {
     Serial.println("SD card initialization failed");
    delay(800);
    return false;
  } else {
     Serial.println("SD card initialized successfully");
    delay(800);
    return true;
  }
}

void setup() { 
  Serial.begin(250000);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.clear();
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(WHITE);

  if (initializeSDCard()) {
    loadConfigFromSD(); // Load configuration from SD card
     Serial.print("WiFi SSID: ");
     Serial.println(wifi_ssid);
     Serial.print("WiFi Password: ");
     Serial.println(wifi_password);

    M5Cardputer.Display.println("Connecting to WiFi");
    M5Cardputer.Display.print("->");

    WiFi.begin(wifi_ssid, wifi_password);
    
    while (WiFi.status() != WL_CONNECTED) {
      M5Cardputer.Display.print(".");
      delay(500);
    }

    M5Cardputer.Display.println("Connected");
  }
  Wire.begin(G2, G1); // Initialize the Cardputer Grove port as I2C
  M5Cardputer.Display.println("Getting Data");

  // Initialize the SHT30 sensor
  if (!sht4.begin(&Wire, 0x44, G2, G1, 100000U)) {
    M5Cardputer.Display.println("SHT40 Error");
    while (1);
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  // Initialize the BMP280 sensor
  if (!bmp.begin(&Wire, 0x76, G2, G1, 100000U)) {
    M5Cardputer.Display.println("BMP280 Error");
    while (1);
  }
  bmp.setSampling(BMP280::MODE_NORMAL, BMP280::SAMPLING_X2, BMP280::SAMPLING_X16, BMP280::FILTER_X16, BMP280::STANDBY_MS_500);

  // Measure the starting altitude
  startAltitude = bmp.readAltitude(1013.25);
  Serial.print("Start Altitude: ");
  Serial.println(startAltitude);

  delay(2000);
}

void loop() {
  printCurrentWeather();
}

void loadConfigFromSD() {
  const char* configFileName = "/OWeather.txt";
  File configFile = SD.open(configFileName, FILE_READ);
  if (configFile) {
     Serial.println("Config file opened successfully.");
    char line[64];
    while (configFile.available()) {
      memset(line, 0, sizeof(line));
      configFile.readBytesUntil('\n', line, sizeof(line) - 1);
      
      char* key = strtok(line, "=");
      char* value = strtok(NULL, "\r\n");

      if (key && value) {
        if (strcmp(key, "API_KEY") == 0) {
          strcpy(api_key, value);
        } else if (strcmp(key, "LATITUDE") == 0) {
          strcpy(latitude, value);
        } else if (strcmp(key, "LONGITUDE") == 0) {
          strcpy(longitude, value);
        } else if (strcmp(key, "UNITS") == 0) {
          strcpy(units, value);
        } else if (strcmp(key, "LANGUAGE") == 0) {
          strcpy(language, value);
        } else if (strcmp(key, "WIFI_SSID") == 0) {
          strcpy(wifi_ssid, value);
        } else if (strcmp(key, "WIFI_PASSWORD") == 0) {
          strcpy(wifi_password, value);
        }
      }
    }
    configFile.close();
  }
}

void printCurrentWeather() {
    HTTPClient http;
    String url = "https://api.openweathermap.org/data/3.0/onecall?";
    url += "lat=" + String(latitude) + "&lon=" + String(longitude);
    url += "&appid=" + String(api_key);
    url += "&units=" + String(units);
    url += "&lang=" + String(language);

    http.begin(url);
    Serial.println("API URL: " + url); // Debug: Log the API URL
    int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Raw API Response:");
        Serial.println(payload); // Debug: Log the raw API response
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            auto current = doc["current"];
            seaLevelPressure = current["pressure"].as<float>() * 100;

            for (int rep = 0; rep < NUM_REPETITIONS; rep++) {
                for (int i = 0; i < 8; i++) {
                    CPText_hdr();
                    M5Cardputer.Display.println(strTime(current["dt"].as<long>()));
                    M5Cardputer.Display.setCursor(0, 25);
                    M5Cardputer.Display.setTextSize(1.9);
                    M5Cardputer.Display.setTextColor(WHITE);

                    switch (i) {
                        case 0:
                            M5Cardputer.Display.print("Temp('F): ");
                            M5Cardputer.Display.println(current["temp"].as<float>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Feels Like('F): ");
                            M5Cardputer.Display.println(current["feels_like"].as<float>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Humidity(%): ");
                            M5Cardputer.Display.println(current["humidity"].as<int>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Dew Point('F): ");
                            M5Cardputer.Display.println(current["dew_point"].as<float>());
                            M5Cardputer.Display.println();
                            break;
                        case 1:
                            M5Cardputer.Display.print("Pressure: ");
                            M5Cardputer.Display.println(current["pressure"].as<int>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("UV Index: ");
                            M5Cardputer.Display.println(current["uvi"].as<float>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Clouds: ");
                            M5Cardputer.Display.println(current["clouds"].as<int>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Visibility(Ft): ");
                            M5Cardputer.Display.println(current["visibility"].as<int>());
                            M5Cardputer.Display.println();
                            break;
                        case 2:
                            M5Cardputer.Display.print("Wind Spd(M/H): ");
                            M5Cardputer.Display.println(current["wind_speed"].as<float>());
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Wind Gst(M/H): ");
                            if (current.containsKey("wind_gust")) {
                                M5Cardputer.Display.println(current["wind_gust"].as<float>());
                            } else {
                                M5Cardputer.Display.println("N/A");
                            }
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Wind Deg: ");
                            M5Cardputer.Display.println(current["wind_deg"].as<int>());
                            M5Cardputer.Display.println();
                            break;
                        case 3:
                            // Display Rain Data
                            M5Cardputer.Display.print("Rain: ");
                            if (current.containsKey("rain")) {
                                M5Cardputer.Display.println(current["rain"]["1h"].as<float>());
                            } else {
                                M5Cardputer.Display.println("0");
                            }
                            M5Cardputer.Display.println();

                            // Display Snow Data
                            M5Cardputer.Display.print("Snow: ");
                            if (current.containsKey("snow")) {
                                M5Cardputer.Display.println(current["snow"]["1h"].as<float>());
                            } else {
                                M5Cardputer.Display.println("0");
                            }
                            M5Cardputer.Display.println();

                            // Display Main Weather and Description
                            M5Cardputer.Display.print("Main: ");
                            if (current.containsKey("weather") && current["weather"].size() > 0) {
                                const char* mainWeather = current["weather"][0]["main"].as<const char*>();
                                const char* description = current["weather"][0]["description"].as<const char*>();

                                // Debug output to ensure parsing works
                                Serial.print("Main weather: ");
                                Serial.println(mainWeather);
                                Serial.print("Weather description: ");
                                Serial.println(description);

                                M5Cardputer.Display.println(mainWeather);
                                M5Cardputer.Display.print("Desc: ");
                                M5Cardputer.Display.println(description);
                            } else {
                                M5Cardputer.Display.println("N/A");
                            }
                            break;

                        case 4:
                            M5Cardputer.Display.println("Sunrise: ");
                            M5Cardputer.Display.println(strTime(current["sunrise"].as<long>()));
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.println("Sunset: ");
                            M5Cardputer.Display.println(strTime(current["sunset"].as<long>()));
                            break;
                        case 5:
                            M5Cardputer.Display.print("Latitude: ");
                            M5Cardputer.Display.println(latitude);
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.print("Longitude: ");
                            M5Cardputer.Display.println(longitude);
                            M5Cardputer.Display.println();
                            M5Cardputer.Display.println("Timezone: ");
                            M5Cardputer.Display.println(doc["timezone"].as<const char*>());
                            break;
                        case 6:
                            if (sht4.update()) {
                                M5Cardputer.Display.println("---M5-Env4-SHT4---");
                                M5Cardputer.Display.println();
                                M5Cardputer.Display.print("Temperature: ");
                                M5Cardputer.Display.println((sht4.cTemp)*9/5 + 32);
                                M5Cardputer.Display.println();
                                M5Cardputer.Display.print("Humidity: ");
                                M5Cardputer.Display.println(sht4.humidity);
                            }
                            break;
                        case 7:
                            if (bmp.update()) {
                                M5Cardputer.Display.println("---M5-Env4-BMP280---");
                                M5Cardputer.Display.println();
                                M5Cardputer.Display.print("Temperature: ");
                                M5Cardputer.Display.println((bmp.cTemp)*9/5 + 32);
                                M5Cardputer.Display.println();
                                M5Cardputer.Display.print("Pressure: ");
                                M5Cardputer.Display.println(bmp.pressure);
                                M5Cardputer.Display.println();
                                M5Cardputer.Display.print("Approx alt: ");
                                float altitude = bmp.readAltitude(1013.25) - startAltitude;
                                M5Cardputer.Display.println(altitude);
                            }
                            break;
                    }
                    delay(DELAY_BETWEEN_PAGES);
                }
            }
        } else {
            Serial.println("JSON Parse Error: " + String(error.c_str()));
        }
    } else {
        Serial.println("HTTP Error: " + String(httpCode));
    }
    http.end();
}

void CPText_hdr() {
  M5Cardputer.Display.clear();
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextColor(YELLOW);
}

String strTime(time_t unixTime) {
    struct tm *timeinfo;
    char buffer[80];

    unixTime += TIME_OFFSET;
    if (DST_ACTIVE) {
        unixTime += DST_OFFSET;
    }

    timeinfo = localtime(&unixTime);
    strftime(buffer, 80, "%H:%M:%S %m-%d-%Y", timeinfo);
    return String(buffer);
}
