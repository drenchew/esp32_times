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
}

bool noWiFiMode =false;

void loop() {

  

  if (WiFi.status() != WL_CONNECTED) {
  noWiFiMode = true;
  displayNoWiFi(); 
  Serial.println("WiFi lost, reconnecting...");
  reconnectWiFi();  
} else {
  noWiFiMode = false;
}

if (noWiFiMode) {
  handleNoConnection();
} else {
  handleWithConnection();
  calculateTimeUntilMidnight();
}



  
}

void handleNoConnection(){
  Serial.println("NO WIFI MODE");
  delay(1000);
}

void handleWithConnection(){
  String currentDate = getCurrentDate();
  digitalWrite(BUZZER_PIN,HIGH);

  // fetch the new times if new day
  if (currentDate != lastUpdateDate && currentDate != "") {
    Serial.println("New day detected! Fetching prayer times...");
    String apiUrl = "https://api.aladhan.com/v1/timingsByCity/" + currentDate + "?city=Sofia&country=Bulgaria&method=13";
    fetchPrayerTimes(apiUrl);
    lastUpdateDate = currentDate;
  }

  Serial.println("Current Date:");
  Serial.println(currentDate);

  displayPrayerTime(currentPrayer);

  String str = currentDate + prayerTimes[currentPrayer];
  Serial.println("Current prayer time: ");
  Serial.println(str);

  currentPrayer++;
  if (currentPrayer >= 5){
    currentPrayer = 0;
  }

  delay(3000);

  





}


void displayPrayerTime(const int currentPrayer){

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(prayerNames[currentPrayer]);
  display.setCursor(0, 30);
  display.println(prayerTimes[currentPrayer]);
  display.display();
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

time_t getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }

  time_t now = mktime(&timeinfo); // Converts struct tm to time_t
  return now;
}

time_t parsePrayerTimeToTimestamp(const int prayer){

  Serial.println("Parsing Prayer time to timestamp: ");


  String tm =  prayerTimes[prayer];
  Serial.print("Prayer :");
  Serial.print(prayerNames[prayer]);
  Serial.println(tm);



  return time(nullptr);
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

int reconnectWiFi() {
  
  for (auto& pair : NetworkProviders) {
    Serial.print("Connecting to WiFi network: ");
    Serial.println(pair.first.c_str());
    
    display.clearDisplay();

    display.setCursor(0, 0);
    display.println("Connecting");
    display.setCursor(0, 30);
    display.println(pair.first.c_str());
    display.display();

    WiFi.begin(pair.first.c_str(), pair.second.c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Reconnected!");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Connected");

      delay(1000);
      display.clearDisplay();
      
      return 1;
    }
  }
  Serial.println("Failed to reconnect to WiFi");
  return 0;
}


void calculateTimeUntilMidnight() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int secondsNow = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    timeUntilMidnight = (86400 - secondsNow) * 1000UL;
    bootTimeMillis = millis();
    Serial.printf("Next update in %lu ms\n", timeUntilMidnight);
  } else {
    Serial.println("Failed to calculate time until midnight.");
  }
}

void displayNoWiFi() {
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(100, 0);  
  display.println("No WiFi");
  display.display();
}
