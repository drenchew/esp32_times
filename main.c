#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <string>
#include <unordered_map>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

std::unordered_map<std::string, std::string> NetworkProviders {
  {"iPhone", "123456789"},
  {"MIRO.NET", "alekalek1"}
};

// Wi-Fi and Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * 2;  // GMT+2 Sofia
const int   daylightOffset_sec = 3600;

String fajr, dhuhr, asr, maghrib, isha;
String lastUpdateDate = "";

unsigned long previousMillis = 0;
const long interval = 3000;  // 3 seconds between prayer times
int currentPrayer = 0;

String prayerNames[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
String prayerTimes[5];

void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  
  for (auto& pair : NetworkProviders) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting:");
    display.setCursor(0, 30);
    display.println(pair.first.c_str());
    display.display();

    WiFi.begin(pair.first.c_str(), pair.second.c_str());
    Serial.print("Connecting to WiFi...");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to any WiFi");
    while (true);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  String currentDate = getCurrentDate();
  lastUpdateDate = currentDate;
  String apiUrl = "https://api.aladhan.com/v1/timingsByCity/" + currentDate + "?city=Sofia&country=Bulgaria&method=13";
  fetchPrayerTimes(apiUrl);
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    displayNoWiFi(); 
    Serial.println("WiFi lost, reconnecting...");
    reconnectWiFi();  
  } else {
   
    display.clearDisplay();
  }

  String currentDate = getCurrentDate();

  if (currentDate != lastUpdateDate && currentDate != "") {
    Serial.println("New day detected! Fetching prayer times...");
    String apiUrl = "https://api.aladhan.com/v1/timingsByCity/" + currentDate + "?city=Sofia&country=Bulgaria&method=13";
    fetchPrayerTimes(apiUrl);
    lastUpdateDate = currentDate;
  }

  unsigned long currentMillis = millis();
  if ((long)(currentMillis - previousMillis) >= interval) {
    previousMillis = currentMillis;

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(prayerNames[currentPrayer]);
    display.setCursor(0, 30);
    display.println(prayerTimes[currentPrayer]);
    display.display();

    currentPrayer++;
    if (currentPrayer >= 5) currentPrayer = 0;
  }
}

String getCurrentDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }
  char dateBuffer[11];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeinfo);
  return String(dateBuffer);
}

void fetchPrayerTimes(const String& apiUrl) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  Serial.println("Fetching Prayer Times...");

  if (https.begin(client, apiUrl)) {
    int httpCode = https.GET();
    Serial.printf("HTTP Code: %d\n", httpCode);

    if (httpCode > 0) {
      String payload = https.getString();
      Serial.println("Payload received.");

      parsePrayerTimes(payload);
    } else {
      Serial.printf("GET request failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("Unable to connect");
  }
}

void parsePrayerTimes(const String& payload) {
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  fajr = String(doc["data"]["timings"]["Fajr"]);
  dhuhr = String(doc["data"]["timings"]["Dhuhr"]);
  asr = String(doc["data"]["timings"]["Asr"]);
  maghrib = String(doc["data"]["timings"]["Maghrib"]);
  isha = String(doc["data"]["timings"]["Isha"]);

  // Fill array
  prayerTimes[0] = fajr;
  prayerTimes[1] = dhuhr;
  prayerTimes[2] = asr;
  prayerTimes[3] = maghrib;
  prayerTimes[4] = isha;

  Serial.println("Updated Prayer Times:");
  Serial.println("Fajr: " + fajr);
  Serial.println("Dhuhr: " + dhuhr);
  Serial.println("Asr: " + asr);
  Serial.println("Maghrib: " + maghrib);
  Serial.println("Isha: " + isha);
}

void reconnectWiFi() {
  for (auto& pair : NetworkProviders) {
    Serial.print("Reconnecting to WiFi network: ");
    Serial.println(pair.first.c_str());
    WiFi.begin(pair.first.c_str(), pair.second.c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Reconnected!");
      return;
    }
  }
  Serial.println("Failed to reconnect to WiFi");
}


void displayNoWiFi() {
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(100, 0);  
  display.println("No WiFi");
  display.display();
}
