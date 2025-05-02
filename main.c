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

#define BUZZER_PIN 18

std::unordered_map<std::string, std::string> NetworkProviders {
  {"iPhone", "123456789"},
  {"MIRO.NET", "alekalek1"},
  {"A1_A57BEB","aa955af8"}
};

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * 2;  // GMT+2 Sofia
const int   daylightOffset_sec = 3600;

String fajr, dhuhr, asr, maghrib, isha;
String lastUpdateDate = "";

String prayerNames[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
String prayerTimes[5];
time_t prayerTimestamps[5];
bool alertPlayed[5] = {false, false, false, false, false};

int currentPrayerIndex = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long displayInterval = 4000;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  while (!reconnectWiFi()) {
    delay(2000);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  String currentDate = getCurrentDate();
  lastUpdateDate = currentDate;
  String apiUrl = "https://api.aladhan.com/v1/timingsByCity/" + currentDate + "?city=Sofia&country=Bulgaria&method=13";

  showMessage("Fetching...");
  fetchPrayerTimes(apiUrl);
  disconnectWiFi();
}

void loop() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();

    String currentDate = getCurrentDate();
    if (currentDate != lastUpdateDate) {
      Serial.println("New day detected. Fetching prayer times...");

      // Try reconnecting to WiFi
      bool connected = reconnectWiFi();

      if (connected) {
        String newDate = getCurrentDate();
        lastUpdateDate = newDate;
        String apiUrl = "https://api.aladhan.com/v1/timingsByCity/" + newDate + "?city=Sofia&country=Bulgaria&method=13";
        fetchPrayerTimes(apiUrl);
        disconnectWiFi();
      } else {
        Serial.println("WARNING: WiFi unavailable. Using yesterday’s prayer times.");
        adjustFajrFallback();
      }
    }
  }

  checkPrayerAlerts();
  updateDisplay();
  delay(3000);
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected.");
}

bool reconnectWiFi() {
  for (auto& pair : NetworkProviders) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(pair.first.c_str(), pair.second.c_str());

    int retries = 0;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting");
    display.setCursor(0, 30);
    display.println(pair.first.c_str());
    display.display();

    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(350);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi Connected!");
      return true;
    }
  }
  Serial.println("Failed to connect to WiFi.");
  return false;
}

void fetchPrayerTimes(const String& apiUrl) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (https.begin(client, apiUrl)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      String payload = https.getString();
      parsePrayerTimes(payload);
    }
    https.end();
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

  prayerTimes[0] = fajr;
  prayerTimes[1] = dhuhr;
  prayerTimes[2] = asr;
  prayerTimes[3] = maghrib;
  prayerTimes[4] = isha;

  for (int i = 0; i < 5; ++i) {
    prayerTimestamps[i] = stringToTime(prayerTimes[i].c_str());
    alertPlayed[i] = false;
  }

  Serial.println("Updated Prayer Times:");
  for (int i = 0; i < 5; i++) {
    Serial.println(prayerNames[i] + ": " + prayerTimes[i]);
  }
}

void adjustFajrFallback() {
  int hour, minute;
  if (sscanf(fajr.c_str(), "%d:%d", &hour, &minute) == 2) {
    minute -= 1;
    if (minute < 0) {
      minute = 59;
      hour = (hour == 0) ? 23 : hour - 1;
    }
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    fajr = String(buffer);
    prayerTimes[0] = fajr;

    Serial.println("Adjusted Fajr time (fallback): " + fajr);
  } else {
    Serial.println("Failed to parse Fajr time for fallback.");
  }
}

time_t stringToTime(const char *str) {
  int hour, minute;
  sscanf(str, "%d:%d", &hour, &minute);
  time_t now = time(NULL);
  struct tm tm_info = *localtime(&now);
  tm_info.tm_hour = hour;
  tm_info.tm_min = minute;
  tm_info.tm_sec = 0;
  return mktime(&tm_info);
}

void checkPrayerAlerts() {
  time_t now = time(NULL);
  for (int i = 0; i < 5; ++i) {
    if (!alertPlayed[i] && now >= prayerTimestamps[i] && now <= (prayerTimestamps[i] + 60)) {
      Serial.println("Time for " + prayerNames[i] + "! Playing buzzer.");
      playBuzzer(5000);  // 3 seconds
      alertPlayed[i] = true;
    }
  }
}

void playBuzzer(int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(prayerNames[currentPrayerIndex]);
  display.setCursor(0, 30);
  display.println(prayerTimes[currentPrayerIndex]);
  display.display();

  Serial.printf("%s at %s \n", prayerNames[currentPrayerIndex], prayerTimes[currentPrayerIndex]);

  currentPrayerIndex = (currentPrayerIndex + 1) % 5;
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

void showMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(msg);
  display.display();
}
