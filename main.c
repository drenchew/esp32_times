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
  {"MIRO.NET", "alekalek1"},
  {"A1_A57BEB","aa955af8"}

};

#define BUZZER_PIN 18

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

unsigned long timeUntilMidnight = 0;
unsigned long bootTimeMillis = 0;
bool fetchedToday = false;

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

  while(true){
    if (reconnectWiFi()){
      break;
    }
  }
 

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  pinMode(BUZZER_PIN,OUTPUT);
  digitalWrite(BUZZER_PIN,0);

  String currentDate = getCurrentDate();
  lastUpdateDate = currentDate;
  String apiUrl = "https://api.aladhan.com/v1/timingsByCity/" + currentDate + "?city=Sofia&country=Bulgaria&method=13";

  fetchPrayerTimes(apiUrl);

  calculateTimeUntilMidnight();

  disconnectWiFi();
  
}

bool noWiFiMode =false;

void loop() {
  if ((millis() - bootTimeMillis) >= timeUntilMidnight) {
    Serial.println("Midnight reached. Fetching new prayer times...");

    while (!reconnectWiFi()) {
      Serial.println("Error connecting to internet, times MUST be fetched!");
    }

    String newDate = getCurrentDate();
    lastUpdateDate = newDate;

    fetchPrayerTimes(newDate);  // Fetch without tracking success flag

    calculateTimeUntilMidnight();  // Recalculate for next day
    disconnectWiFi();
  }

  displayPrayerTimes();
  
}


void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected.");
}



void displayPrayerTimes(){

  
  display.setTextSize(2);
  display.setCursor(0, 0);
  
  digitalWrite(BUZZER_PIN,HIGH);

  for(int i =0;i <5 ;++i){
    display.clearDisplay();
    
   display.println(prayerNames[i]);
   display.setCursor(0, 30);
   display.println(prayerTimes[i]);
   display.display();

   delay(3000); 
  }
  digitalWrite(BUZZER_PIN,0);

 
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

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fetching..");

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

int reconnectWiFi() {
  for (auto& pair : NetworkProviders) {
    Serial.print("Connecting to WiFi network: ");
    Serial.println(pair.first.c_str());

    WiFi.disconnect(true);       // Ensure previous state is cleared
    delay(100);                  // Let it settle
    WiFi.mode(WIFI_STA);         // Set to station mode
    WiFi.begin(pair.first.c_str(), pair.second.c_str());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting");
    display.setCursor(0, 30);
    display.println(pair.first.c_str());
    display.display();

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(350);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");

      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Connected");
      display.display();
      delay(1000);
      display.clearDisplay();
      return 1;
    }
  }
  Serial.println("Failed to reconnect to WiFi");
  return 0;
}



time_t calculateTimeUntilMidnight() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int secondsNow = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    timeUntilMidnight = (86400 - secondsNow) * 1000UL;
    bootTimeMillis = millis();
    Serial.printf("Time now: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    Serial.printf("Next update in %lu ms (%.2f hours)\n", timeUntilMidnight, timeUntilMidnight / 3600000.0);
    return timeUntilMidnight;
  } else {
    Serial.println("Failed to calculate time until midnight.");
    return 0;
  }
}




void displayNoWiFi() {
  display.setCursor(100, 0);  
  display.println("No WiFi");
  display.display();
}
