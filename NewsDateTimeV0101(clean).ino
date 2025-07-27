/* Make sure you enter your details before building:
Wifi SSID
Wifi Password
OpenWeather API String
Your Longtitude and Latitude
Modify the RSS feeds you want */



#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// === WiFi Credentials ===
const char* ssid = "YOUR-SSID";
const char* password = "YOUR-WIFI-PASSW";

// === Weather API Key & Location ===
String openWeatherApiKey = "YOUR-API-KEY";
float latitude = YOUR-LAT;
float longitude = YOUR-LONG;

// === RSS Feeds ===
const char* newsUrls[] = {
  "https://www.nu.nl/rss/Algemeen",
  "https://feeds.bbci.co.uk/news/rss.xml",
  "https://feeds.skynews.com/feeds/rss/home.xml",
  "https://www.reutersagency.com/feed/?best-types=reuters-news-first&post_type=best"
};
const int numFeeds = 4;
int currentFeed = 0;

// === Display & Time Clients ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// === Scroll Buffers ===
String headlines[40];
int numHeadlines = 0;
String weatherInfo = "";
String timeWeatherLine = "";
float rssOffset = 128, infoOffset = 128;
const float rssSpeed = 2.5, infoSpeed = 2;  //rssSpeed = 3.2, infoSpeed = 2.6;  // Boosted for larger font

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(800);
    Serial.println("Connecting to WiFi...");
  }

  u8g2.begin();
  u8g2.setFont(u8g2_font_courR14_tf);  // Restored large monospaced font
  timeClient.begin();
  fetchHeadlines();
  fetchWeather();
}

void loop() {
  timeClient.update();
  String timeStr = timeClient.getFormattedTime();
  timeWeatherLine = timeStr + " | " + weatherInfo;

  u8g2.clearBuffer();

  // RSS Line - placed at lower midline
  int x = rssOffset;
  for (int i = 0; i < numHeadlines; i++) {
    u8g2.drawUTF8(x, 42, headlines[i].c_str());  // Vertical position adjusted for large font
    x += u8g2.getUTF8Width(headlines[i].c_str()) + 15;  // Extra spacing for visibility
  }
  rssOffset -= rssSpeed;
  if (rssOffset < -getRSSWidth()) {
    rssOffset = 128;
    currentFeed = (currentFeed + 1) % numFeeds;
    fetchHeadlines();
  }

  // Time + Weather Line - final row
  u8g2.drawUTF8(infoOffset, 64, timeWeatherLine.c_str());
  infoOffset -= infoSpeed;
  if (infoOffset < -u8g2.getUTF8Width(timeWeatherLine.c_str())) {
    infoOffset = 128;
    fetchWeather();
  }

  u8g2.sendBuffer();
  delay(15);  // Snappy refresh
}

// === Utilities ===
int getRSSWidth() {
  int w = 0;
  for (int i = 0; i < numHeadlines; i++) {
    w += u8g2.getUTF8Width(headlines[i].c_str()) + 15;
  }
  return w;
}

String convertToUTF8(String str) {
  str.replace("£", "\xC2\xA3");
  str.replace("é", "\xC3\xA9");
  str.replace("ë", "\xC3\xAB");
  str.replace("ï", "\xC3\xAF");
  return str;
}

// === Data Fetchers ===
void fetchHeadlines() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (http.begin(client, newsUrls[currentFeed])) {
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      parseRSS(payload);
    }
    http.end();
  }
}

void parseRSS(String xml) {
  numHeadlines = 0;
  int pos = 0;
  while ((pos = xml.indexOf("<title>", pos)) >= 0 && numHeadlines < 40) {
    int endPos = xml.indexOf("</title>", pos);
    if (endPos > pos) {
      String headline = xml.substring(pos + 7, endPos);
      headline.replace("<![CDATA[", "");
      headline.replace("]]>", "");
      headline = convertToUTF8(headline);
      headlines[numHeadlines] = (numHeadlines > 0) ? "-|- " + headline : headline;
      numHeadlines++;
    }
    pos = endPos + 8;
  }
}

void fetchWeather() {
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 2) +
               "&lon=" + String(longitude, 2) +
               "&units=metric&appid=" + openWeatherApiKey;

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        float temp = doc["main"]["temp"];
        int humidity = doc["main"]["humidity"];
        const char* condition = doc["weather"][0]["description"];
        weatherInfo = "🌡 " + String(temp, 1) + "°C " + String(condition) + " 💧" + String(humidity) + "%";
        http.end();
        return;
      }
    }
    weatherInfo = "⚠️ Weather unavailable";
    http.end();
  }
}
