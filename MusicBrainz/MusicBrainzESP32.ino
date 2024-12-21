#include "rm67162.h"

#include <TFT_eSPI.h>             

#include <TJpg_Decoder.h>

#include "FS.h"
#include <LittleFS.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "List_LittleFS.h"
#include "Web_Fetch.h"

// WIFI 
#define WIFI_SSID "XXXXX"
#define PASSWORD "XXXXX"

// Screen
#define WIDTH  536
#define HEIGHT 240

// Token for faster musicbrainz resoponses (your api-key)
const char* TOKEN = "XXXXX";
// MusicBrainz Username
const char* username = "XXXXX";


TFT_eSPI tft = TFT_eSPI();         
TFT_eSprite sprite = TFT_eSprite(&tft);

HTTPClient http;
JsonDocument doc;
JsonDocument doc2;

int httpCode=0;
String last_track_name = "";
String track_name = "";
String release_name = "";
String artist_name = "";
String device = "";
String release_mbid = "";
String payload="";
String payload2="";
String IdUrl="";
String CoverUrl="";


String convertSpacesToPlus(String input) {
  String result = "";
  for (int i = 0; i < input.length(); i++) {
    if (input.charAt(i) == ' ') {
      result += '+';
    } else {
      result += input.charAt(i);
    }
  }
  return result;
}

String getMetadataIdUrl(String track_name, String artist_name) {
  String MetaUrl = "https://api.listenbrainz.org/1/metadata/lookup/?recording_name=";
  MetaUrl += convertSpacesToPlus(track_name);
  MetaUrl += "&artist_name=";
  MetaUrl += convertSpacesToPlus(artist_name);
  
  return MetaUrl;
}

void getReleaseId() {
  if ((WiFi.status() == WL_CONNECTED)) {
    String url = getMetadataIdUrl(track_name, artist_name);  
    // Serial.println(url);
    http.begin(url);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.addHeader("Authorization", "Token " + String(TOKEN));
    httpCode = http.GET();  // Make the request
    if (httpCode > 0) {
      payload2 = http.getString();
      // Serial.println("Payload2 received:");
      // Serial.println(payload2); 
      deserializeJson(doc2, payload2);
      const char* rmbid = doc2["release_mbid"];
      release_mbid = rmbid;
      
      // Serial.print("release_mbid: ");
      // Serial.println(release_mbid);
    }
    http.end(); 
  }
}

String getDataUrl(String username) {
  String DataUrl = "https://api.listenbrainz.org/1/user/";
  DataUrl += username;
  DataUrl += "/playing-now";
  
  return DataUrl;
}

void getData() {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(getDataUrl(username));
    http.addHeader("Authorization", "Token " + String(TOKEN));
    int httpCode = http.GET();
    if (httpCode > 0) {
      payload = http.getString();
      // Serial.println("Payload received:");
      // Serial.println(payload);  

      deserializeJson(doc, payload);

      if (doc.containsKey("payload") && doc["payload"].containsKey("listens") && doc["payload"]["listens"].size() > 0 &&
          doc["payload"]["listens"][0]["track_metadata"].containsKey("track_name") &&
          doc["payload"]["listens"][0]["track_metadata"].containsKey("release_name") &&
          doc["payload"]["listens"][0]["track_metadata"].containsKey("artist_name") &&
          doc["payload"]["listens"][0]["track_metadata"]["additional_info"].containsKey("submission_client")) {

        const char* tn = doc["payload"]["listens"][0]["track_metadata"]["track_name"];
        const char* rn = doc["payload"]["listens"][0]["track_metadata"]["release_name"];
        const char* an = doc["payload"]["listens"][0]["track_metadata"]["artist_name"];
        const char* dv = doc["payload"]["listens"][0]["track_metadata"]["additional_info"]["submission_client"];

        track_name = tn;
        release_name = rn;
        artist_name = an;
        device = dv;

        // Serial.println();
        // Serial.println("===Recieved now-playing info===");
        // Serial.print("Track Name: ");
        // Serial.println(track_name);
        // Serial.print("Release Name: ");
        // Serial.println(release_name);
        // Serial.print("Artist Name: ");
        // Serial.println(artist_name);
        // Serial.print("Submission Client: ");
        // Serial.println(device);
      } else {
        Serial.println("Payload doesn't contain expected keys or is empty");
      }
    } else {
      Serial.println("Error fetching data: " + http.errorToString(httpCode));
    }
    http.end();
  }
}

void drawData()
{
    sprite.fillSprite(TFT_BLACK);
    sprite.drawString(track_name,252,0,4);
    sprite.drawString(artist_name,252,20,4);
    sprite.drawString(release_name,252,40,4);
    sprite.drawString(device,252,60,4);
    // sprite.drawString(release_mbid,252,80,4);
}

String getCoverUrl(String releasembid) {
  String CoverUrl = "http://coverartarchive.org/release/";
  CoverUrl += releasembid;
  CoverUrl += "/front-250";

  // Serial.println("CoverURL: ");
  // Serial.print(CoverUrl);

  return CoverUrl;
}




void drawCover()
{
  // List files stored in LittleFS
  // listLittleFS();

  if (LittleFS.exists("/Cover.jpg") == true) {
    Serial.println("===removing file===");
    LittleFS.remove("/Cover.jpg");
  }

  // Time recorded for test purposes
  uint32_t t = millis();

  // Get release_mbid url
  String coverUrl = getCoverUrl(release_mbid); 
  // Serial.println(coverUrl);

  // Fetch the jpg file from the MusicBrainz Archive
  bool loaded_ok = getFile(coverUrl, "/Cover.jpg", TOKEN);

  t = millis() - t;
  if (loaded_ok) { Serial.print(t); Serial.println(" ms to download"); }

  // List files stored in LittleFS, should have the file now
  // listLittleFS();

  t = millis();

  // Drawing the LittleFS file
  TJpgDec.drawFsJpg(0, 0, "/Cover.jpg", LittleFS);
  t = millis() - t;
  Serial.print(t); Serial.println(" ms to draw to TFT");

}

void seekChanges() {
  // Getting data from api
  getData();

  if (last_track_name != track_name) {
    sprite.fillSprite(TFT_BLACK);
    // Getting mbid for the rest of functions
    getReleaseId();
    // Drawing new data
    drawData();
    // Cover placeholder
    sprite.drawString("COVER",95,110,4);
    // Pushing color so the title is displayed
    lcd_PushColors(0, 0, 536, 240, (uint16_t*)sprite.getPointer());
    // Drawing cover (includes downloading file)
    drawCover();
    last_track_name = track_name;
    lcd_PushColors(0, 0, 536, 240, (uint16_t*)sprite.getPointer());
  } else {
    // No changes in track, do nothing
    Serial.println("===No changes in track===");
  }
}

// TJpg function for image to tft
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= sprite.height() ) return 0;
  // This function will clip the image block rendering automatically at the TFT boundaries
  sprite.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

void setup()
{
  // Serial init
  Serial.begin(115200);
  Serial.println("\nHello");

  // Amoled screen init
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  rm67162_init();
  lcd_setRotation(1);
  sprite.createSprite(WIDTH, HEIGHT);

  delay(1000);

  // LittleFS init
  Serial.println("===Mounting LittleFS filesystem...===");
  // Set (true) to ensure the partition is formated corectly (spent way too much time debuging it)
  if(!LittleFS.begin(true)){
    Serial.println("#ERROR: LittleFS Mount Failed!#");
    return;
  }
  Serial.println("===Mounted!===");

  // Removing only our file
  if (LittleFS.exists("/Cover.jpg") == true) {
    Serial.println("===Removing file===");
    LittleFS.remove("/Cover.jpg");
  }

  // TFT init
  sprite.createSprite(WIDTH, HEIGHT);
  sprite.setSwapBytes(1);  //swap bytes already in TJpg part
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);

  // TJpg init
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tft_output);

  // WIFI init
  WiFi.begin(WIFI_SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("connected to wifi");

}

void loop()
{
  seekChanges();
  // Our interval for checking musicbrainz API
  delay(5000);
}