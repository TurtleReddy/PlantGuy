#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LovyanGFX.h>
#include <TJpg_Decoder.h>
#include "lgfx_config.h"
#include "spotify_api.h"
#include <WiFiClientSecure.h>
#include <vector>
#include <LittleFS.h>
#include <Wire.h>
#include <RTClib.h>
RTC_DS3231 rtc;

#define SPOTIFY_BUTTON 12
#define FACE_BUTTON 5
#define CLOCK_BUTTON 22

extern String currentSong;
extern String currentArtist;
extern String currentAlbum;
extern String albumArtURL;
extern bool currentlyPlaying;

enum DisplayMode {
  SPOTIFY_MODE,
  FACE_MODE,
  CLOCK_MODE
};

DisplayMode currentMode = FACE_MODE;
int currentFace = 1;

unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_TIME = 10;

// ===============================
// WiFi
// ===============================

const char* ssid = "CHANGE THIS TO YOUR WIFI NAME";
const char* password = "CHANGE THIS TO YOUR WIFI PASSWORD";

// ===============================
// Display
// ===============================

LGFX tft;

String lastTrackID = "";

String lastSong = "";

#define albumBackground TFT_BLACK

// ===============================
// Control Layout
// ===============================

const int PREV_X = 160;
const int PLAY_X = 240;
const int NEXT_X = 320;

const int CONTROL_Y = 220;

const int PLAY_RADIUS = 24;

const int ICON_HEIGHT = 30;
const int ICON_WIDTH = 25;
const int BAR_WIDTH = 6;

const int TOUCH_PADDING = 35;

// ===============================
// Spotify Credentials
// ===============================

const char* clientID = "CHANGE THIS TO YOUR CLIENT ID";
const char* clientSecret = "CHANGE THIS TO YOUR CLIENT SECRET ";

// ===============================
// Spotify Commands
// ===============================

void spotifyCommand(String command) {
  HTTPClient http;

  String url = "https://api.spotify.com/v1/me/player/" + command;

  Serial.println("Sending:");
  Serial.println(url);

  http.begin(url);

  http.addHeader(
    "Authorization",
    "Bearer " + spotifyAccessToken);

  http.addHeader(
    "Content-Length",
    "0");

  int response;

  if (command == "next" || command == "previous") {
    response = http.sendRequest("POST");
  } else {
    response = http.sendRequest("PUT");
  }

  Serial.print("Spotify response: ");
  Serial.println(response);

  if (response > 0) {
    Serial.println(http.getString());
  }

  http.end();
}

bool spotifyIsPlaying() {
  HTTPClient http;


  http.begin(
    "https://api.spotify.com/v1/me/player");


  http.addHeader(
    "Authorization",
    "Bearer " + spotifyAccessToken);


  int response =
    http.GET();


  if (response == 200) {
    WiFiClient* stream = http.getStreamPtr();
  }

  http.end();

  return false;
}

// ===============================
// Draw Buttons
// ===============================

void drawButton(
  int x,
  int y,
  int w,
  int h,
  String text) {
  tft.fillRect(
    PLAY_X - 7,
    CONTROL_Y - 10,
    5,
    20,
    albumBackground);

  tft.fillRect(
    PLAY_X + 2,
    CONTROL_Y - 10,
    5,
    20,
    albumBackground);


  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);


  int textWidth =
    text.length() * 12;


  tft.setCursor(
    x + (w - textWidth) / 2,
    y + 22);


  tft.print(text);
}

void drawSongInfo() {

  // Clear text area
  tft.fillRect(
    240,
    40,
    220,
    150,
    albumBackground);


  // ===============================
  // Song Title
  // ===============================

  tft.setFont(&fonts::Font4);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);

  int x = 250;
  int y = 70;

  String title = currentSong;

  int maxChars = 15;


  // Count title lines
  int titleLines = 1;
  String tempTitle = title;

  while (tempTitle.length() > maxChars) {
    int split = tempTitle.lastIndexOf(' ', maxChars);

    if (split == -1) {
      split = maxChars;
    }

    tempTitle = tempTitle.substring(split + 1);

    titleLines++;
  }


  // Move long titles upward
  y -= (titleLines - 1) * 10;

  if (y < 20) {
    y = 20;
  }


  // Actually draw title
  while (title.length() > maxChars) {
    int split = title.lastIndexOf(' ', maxChars);

    if (split == -1) {
      split = maxChars;
    }

    tft.setCursor(x, y);
    tft.println(title.substring(0, split));

    title = title.substring(split + 1);

    y += 30;
  }


  tft.setCursor(x, y);
  tft.println(title);



  // ===============================
  // Artist
  // ===============================

  y += 40;

  tft.setFont(&fonts::Font2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY);

  String artist = currentArtist;


  while (artist.length() > maxChars) {
    int split = artist.lastIndexOf(' ', maxChars);

    if (split == -1) {
      split = maxChars;
    }

    tft.setCursor(x, y);
    tft.println(artist.substring(0, split));

    artist = artist.substring(split + 1);

    y += 25;
  }


  tft.setCursor(x, y);
  tft.println(artist);


  // Reset
  tft.setFont(nullptr);
  tft.setTextColor(TFT_WHITE);
}

void updatePlayButton() {
  tft.fillCircle(
    PLAY_X,
    CONTROL_Y,
    PLAY_RADIUS + 3,
    albumBackground);

  tft.fillCircle(
    PLAY_X,
    CONTROL_Y,
    PLAY_RADIUS,
    TFT_WHITE);

  if (currentlyPlaying) {
    tft.fillRoundRect(
      PLAY_X - 8,
      CONTROL_Y - 10,
      5,
      20,
      2,
      albumBackground);

    tft.fillRoundRect(
      PLAY_X + 3,
      CONTROL_Y - 10,
      5,
      20,
      2,
      albumBackground);
  } else {
    tft.fillTriangle(
      PLAY_X - 7,
      CONTROL_Y - 11,

      PLAY_X - 7,
      CONTROL_Y + 11,

      PLAY_X + 9,
      CONTROL_Y,

      albumBackground);
  }
}

String formatTime(int milliseconds) {
  int totalSeconds = milliseconds / 1000;

  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;

  String result = String(minutes) + ":";

  if (seconds < 10) {
    result += "0";
  }

  result += String(seconds);

  return result;
}

void drawClock() {

  DateTime now = rtc.now();

  int hour = now.hour();

  if (hour == 0) {
    hour = 12;
  } else if (hour > 12) {
    hour -= 12;
  }
  int minute = now.minute();

  // ===============================
  // Background
  // ===============================

  uint16_t clockBackground = tft.color565(30, 35, 179);

  tft.fillScreen(clockBackground);


  // ===============================
  // Digit dimensions
  // ===============================

  int widths[10] = {
    110,  // 0
    60,   // 1
    85,   // 2
    90,   // 3
    115,  // 4
    100,  // 5
    92,   // 6
    75,   // 7
    80,   // 8
    95    // 9
  };

  int heights[10] = {
    150,  // 0
    140,  // 1
    145,  // 2
    140,  // 3
    150,  // 4
    145,  // 5
    144,  // 6
    140,  // 7
    140,  // 8
    145   // 9
  };


  // ===============================
  // TIME SETTINGS
  // ===============================

  int timeScale = 1;
  int timeCenterX = 240;
  int timeBaseline = 160;
  int timeGap = 0;


  // ===============================
  // Get time digits
  // ===============================

  int digits[4];

  digits[0] = hour / 10;
  digits[1] = hour % 10;
  digits[2] = minute / 10;
  digits[3] = minute % 10;


  // ===============================
  // Calculate time width
  // ===============================

  int timeWidth = 0;

  timeWidth += widths[digits[0]] / timeScale;
  timeWidth += widths[digits[1]] / timeScale;
  timeWidth += 35 / timeScale;
  timeWidth += widths[digits[2]] / timeScale;
  timeWidth += widths[digits[3]] / timeScale;

  timeWidth += timeGap * 4;


  // Start time at centered position

  int x = timeCenterX - timeWidth / 2;


  // ===============================
  // Draw hour tens
  // ===============================

  int digit = digits[0];

  String path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.setJpgScale(timeScale);

  TJpgDec.drawFsJpg(
    x,
    timeBaseline - heights[digit] / timeScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / timeScale + timeGap;


  // ===============================
  // Draw hour ones
  // ===============================

  digit = digits[1];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    timeBaseline - heights[digit] / timeScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / timeScale + timeGap;


  // ===============================
  // Draw colon
  // ===============================

  TJpgDec.drawFsJpg(
    x,
    timeBaseline - 100 / timeScale,
    "/clock/colon.jpg",
    LittleFS);

  x += 35 / timeScale + timeGap;


  // ===============================
  // Draw minute tens
  // ===============================

  digit = digits[2];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    timeBaseline - heights[digit] / timeScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / timeScale + timeGap;


  // ===============================
  // Draw minute ones
  // ===============================

  digit = digits[3];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    timeBaseline - heights[digit] / timeScale,
    path.c_str(),
    LittleFS);


  // ===============================
  // DATE SETTINGS
  // ===============================

  int dateScale = 4;
  int dateCenterX = 240;
  int dateBaseline = 280;
  int dateGap = 0;


  // ===============================
  // Get date digits
  // ===============================

  int dateDigits[6];

  dateDigits[0] = now.month() / 10;
  dateDigits[1] = now.month() % 10;

  dateDigits[2] = now.day() / 10;
  dateDigits[3] = now.day() % 10;

  int year = now.year() % 100;

  dateDigits[4] = year / 10;
  dateDigits[5] = year % 10;


  // ===============================
  // Calculate date width
  // ===============================

  int dateWidth = 0;

  for (int i = 0; i < 6; i++) {
    dateWidth += widths[dateDigits[i]] / dateScale;
  }

  dateWidth += (65 / dateScale) * 2;
  dateWidth += dateGap * 7;


  // Start date at centered position

  x = dateCenterX - dateWidth / 2;


  // ===============================
  // Draw month tens
  // ===============================

  digit = dateDigits[0];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.setJpgScale(dateScale);

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - heights[digit] / dateScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / dateScale + dateGap;


  // ===============================
  // Draw month ones
  // ===============================

  digit = dateDigits[1];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - heights[digit] / dateScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / dateScale + dateGap;


  // ===============================
  // Draw first slash
  // ===============================

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - 140 / dateScale,
    "/clock/slash.jpg",
    LittleFS);

  x += 65 / dateScale + dateGap;


  // ===============================
  // Draw day tens
  // ===============================

  digit = dateDigits[2];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - heights[digit] / dateScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / dateScale + dateGap;


  // ===============================
  // Draw day ones
  // ===============================

  digit = dateDigits[3];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - heights[digit] / dateScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / dateScale + dateGap;


  // ===============================
  // Draw second slash
  // ===============================

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - 140 / dateScale,
    "/clock/slash.jpg",
    LittleFS);

  x += 65 / dateScale + dateGap;


  // ===============================
  // Draw year tens
  // ===============================

  digit = dateDigits[4];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - heights[digit] / dateScale,
    path.c_str(),
    LittleFS);

  x += widths[digit] / dateScale + dateGap;


  // ===============================
  // Draw year ones
  // ===============================

  digit = dateDigits[5];

  path = "/clock/" + String(digit) + ".jpg";

  TJpgDec.drawFsJpg(
    x,
    dateBaseline - heights[digit] / dateScale,
    path.c_str(),
    LittleFS);


  // ===============================
  // Serial output
  // ===============================

  Serial.print("Clock: ");
  Serial.print(hour);
  Serial.print(":");

  if (minute < 10) {
    Serial.print("0");
  }

  Serial.print(minute);

  Serial.print("  Date: ");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.day());
  Serial.print("/");
  Serial.println(now.year() % 100);
}

void drawProgressBar() {
  int barX = 65;
  int barY = 270;
  int barWidth = 350;
  int barHeight = 10;


  // Clear old bar
  tft.fillRoundRect(
    barX,
    barY,
    barWidth,
    barHeight,
    3,
    TFT_DARKGREY);


  int progressWidth = 0;

  if (songDuration > 0) {
    progressWidth =
      (currentProgress * barWidth) / songDuration;
  }


  // Draw progress
  tft.fillRoundRect(
    barX,
    barY,
    progressWidth,
    barHeight,
    3,
    TFT_WHITE);

  // Time labels
  tft.setFont(&fonts::Font2);
  tft.setTextSize(1);


  tft.setTextColor(TFT_LIGHTGREY);


  String currentTime = formatTime(currentProgress);
  String totalTime = formatTime(songDuration);


  tft.fillRect(
    barX - 45,
    barY - 10,
    45,
    20,
    albumBackground);

  tft.fillRect(
    barX + barWidth + 10,
    barY - 10,
    50,
    20,
    albumBackground);


  // Draw current time
  tft.setCursor(
    barX - 40,
    barY - 3);

  tft.print(currentTime);


  // Draw total time
  tft.setCursor(
    barX + barWidth + 15,
    barY - 3);

  tft.print(totalTime);
}

void downloadAlbumArt() {
  Serial.println("Downloading album art...");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  http.begin(
    client,
    albumArtURL);

  int response = http.GET();

  Serial.print("Image response: ");
  Serial.println(response);

  if (response == 200) {

    int length = http.getSize();

    Serial.print("Image size: ");
    Serial.println(length);

    if (length > 0 && length < 200000) {

      uint8_t* jpgBuffer = new uint8_t[length];

      if (jpgBuffer == nullptr) {
        Serial.println("Memory allocation failed!");
        http.end();
        return;
      }

      WiFiClient* stream = http.getStreamPtr();

      int bytesRead = 0;

      while (bytesRead < length) {

        int available = stream->available();

        if (available) {
          bytesRead += stream->read(
            jpgBuffer + bytesRead,
            length - bytesRead);
        }

        delay(1);
      }


      Serial.print("Downloaded bytes: ");
      Serial.println(bytesRead);


      tft.fillScreen(TFT_BLACK);


      Serial.println("Drawing album art");

      TJpgDec.setJpgScale(2);

      TJpgDec.drawJpg(
        40,
        20,
        jpgBuffer,
        bytesRead);


      drawSongInfo();
      drawPreviousIcon(PREV_X, CONTROL_Y);
      updatePlayButton();
      drawNextIcon(NEXT_X, CONTROL_Y);
      drawProgressBar();


      Serial.println("Album art drawn");


      delete[] jpgBuffer;

    } else {
      Serial.println("Invalid image size");
    }
  }

  http.end();
}

bool tft_output(
  int16_t x,
  int16_t y,
  uint16_t w,
  uint16_t h,
  uint16_t* bitmap) {

  tft.pushImage(
    x,
    y,
    w,
    h,
    bitmap);

  return true;
}


// ===============================
// Spotify Style Icons
// ===============================

void drawPlayIcon(int x, int y) {
  tft.fillTriangle(
    x,
    y,
    x,
    y + 40,
    x + 35,
    y + 20,
    TFT_WHITE);
}


void drawPauseIcon(int x, int y) {
  tft.fillRoundRect(
    x,
    y,
    12,
    40,
    3,
    TFT_WHITE);

  tft.fillRoundRect(
    x + 20,
    y,
    12,
    40,
    3,
    TFT_WHITE);
}

void drawPreviousIcon(int cx, int cy) {
  // vertical bar
  tft.fillRect(
    cx - 18,
    cy - ICON_HEIGHT / 2,
    BAR_WIDTH,
    ICON_HEIGHT,
    TFT_WHITE);

  // triangle
  tft.fillTriangle(
    cx + 6,
    cy - ICON_HEIGHT / 2,

    cx + 6,
    cy + ICON_HEIGHT / 2,

    cx - 14,
    cy,

    TFT_WHITE);
}


void drawNextIcon(int cx, int cy) {
  tft.fillTriangle(
    cx - 6,
    cy - ICON_HEIGHT / 2,

    cx - 6,
    cy + ICON_HEIGHT / 2,

    cx + 14,
    cy,

    TFT_WHITE);

  tft.fillRect(
    cx + 16,
    cy - ICON_HEIGHT / 2,
    BAR_WIDTH,
    ICON_HEIGHT,
    TFT_WHITE);
}

void checkModeButtons() {

  // Face button
  if (digitalRead(FACE_BUTTON) == LOW) {

    delay(10);  // debounce

    if (digitalRead(FACE_BUTTON) == LOW) {

      // Switch to FACE mode if we aren't already there
      if (currentMode != FACE_MODE) {
        currentMode = FACE_MODE;
        currentFace = 1;

        Serial.println("Switched to FACE mode");

        drawFace(currentFace);
      }

      // If we're already in FACE mode, change to the next face
      else {
        currentFace++;

        if (currentFace > 3) {
          currentFace = 1;
        }

        Serial.print("Changed to face ");
        Serial.println(currentFace);

        drawFace(currentFace);
      }

      // Wait for button to be released
      while (digitalRead(FACE_BUTTON) == LOW) {
        delay(10);
      }
    }
  }


  // Spotify button
  if (digitalRead(SPOTIFY_BUTTON) == LOW) {

    delay(10);  // debounce

    if (digitalRead(SPOTIFY_BUTTON) == LOW) {

      if (currentMode != SPOTIFY_MODE) {
        currentMode = SPOTIFY_MODE;

        Serial.println("Switched to SPOTIFY mode");

        TJpgDec.setJpgScale(2);

        downloadAlbumArt();

        drawSongInfo();
        drawPreviousIcon(PREV_X, CONTROL_Y);
        updatePlayButton();
        drawNextIcon(NEXT_X, CONTROL_Y);
        drawProgressBar();
      }

      // Wait for button to be released
      while (digitalRead(SPOTIFY_BUTTON) == LOW) {
        delay(10);
      }
    }
  }

  // Clock button
  if (digitalRead(CLOCK_BUTTON) == LOW) {

    delay(10);  // debounce

    if (digitalRead(CLOCK_BUTTON) == LOW) {

      if (currentMode != CLOCK_MODE) {
        currentMode = CLOCK_MODE;

        Serial.println("Switched to CLOCK mode");

        drawClock();
      }

      // Wait for button to be released
      while (digitalRead(CLOCK_BUTTON) == LOW) {
        delay(10);
      }
    }
  }
}

void drawFace(int faceNumber) {
  String path = "/faces/face" + String(faceNumber) + ".jpg";

  Serial.print("Loading face: ");
  Serial.println(path);

  tft.fillScreen(TFT_BLACK);

  if (!LittleFS.exists(path)) {
    Serial.println("Face file does not exist!");
    return;
  }

  // Faces are 480x320, so draw them at full size
  TJpgDec.setJpgScale(1);

  TJpgDec.drawFsJpg(
    0,
    0,
    path.c_str(),
    LittleFS);

  Serial.println("Face drawn!");
}

void listLittleFS() {
  Serial.println("=== LittleFS files ===");

  File root = LittleFS.open("/");
  File file = root.openNextFile();

  while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());

    file = root.openNextFile();
  }

  Serial.println("=== End files ===");
}


// ===============================
// Setup
// ===============================

void setup() {
  Serial.begin(115200);

  // DS3231

  Wire.begin(25, 26);

if (!rtc.begin()) {
  Serial.println("Couldn't find DS3231!");
  while (1) {
    delay(10);
  }
}

  Serial.println("DS3231 found!");

  //rtc.adjust(DateTime(2026, 11, 19, 22, 56, 0));

  DateTime now = rtc.now();

  Serial.print("RTC time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  pinMode(12, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP);

  // Display

  tft.init();
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }

  listLittleFS();

  Serial.println("LittleFS mounted!");

  tft.setRotation(2);

  tft.setBrightness(255);

  Serial.println("Setting JPEG callback");

  TJpgDec.setJpgScale(2);
  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true);

  Serial.println("JPEG callback set");

  tft.fillScreen(albumBackground);

  drawPreviousIcon(PREV_X, CONTROL_Y);
  drawNextIcon(NEXT_X, CONTROL_Y);
  updatePlayButton();

  // WiFi

  WiFi.begin(
    ssid,
    password);


  Serial.print("Connecting");

  while (
    WiFi.status() != WL_CONNECTED) {
    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "WiFi Connected!");


  Serial.println(
    WiFi.localIP());


  // Spotify login refresh

  refreshSpotifyToken();

  drawFace(1);

  Serial.println(
    "Ready!");
}

bool touchedPrevious(int x, int y) {
  return (
    x > PREV_X - ICON_WIDTH - TOUCH_PADDING && x < PREV_X + ICON_WIDTH + TOUCH_PADDING && y > CONTROL_Y - ICON_HEIGHT - TOUCH_PADDING && y < CONTROL_Y + ICON_HEIGHT + TOUCH_PADDING);
}

bool touchedNext(int x, int y) {
  return (
    x > NEXT_X - ICON_WIDTH - TOUCH_PADDING && x < NEXT_X + ICON_WIDTH + TOUCH_PADDING && y > CONTROL_Y - ICON_HEIGHT - TOUCH_PADDING && y < CONTROL_Y + ICON_HEIGHT + TOUCH_PADDING);
}

bool touchedPlay(int x, int y) {
  return (
    x > PLAY_X - PLAY_RADIUS - TOUCH_PADDING && x < PLAY_X + PLAY_RADIUS + TOUCH_PADDING && y > CONTROL_Y - PLAY_RADIUS - TOUCH_PADDING && y < CONTROL_Y + PLAY_RADIUS + TOUCH_PADDING);
}

// ===============================
// Loop
// ===============================

void loop() {

  checkModeButtons();

  if (currentMode == CLOCK_MODE) {

    static int lastMinute = -1;

    DateTime now = rtc.now();

    if (now.minute() != lastMinute) {
      lastMinute = now.minute();

      drawClock();
    }

    return;
  }

  uint16_t x;
  uint16_t y;

  if (tft.getTouch(&x, &y)) {
    Serial.print("Touch: ");
    Serial.print(x);
    Serial.print(",");
    Serial.println(y);

    if (touchedPrevious(x, y)) {
      Serial.println("Previous");
      spotifyCommand("previous");
    }

    if (touchedPlay(x, y)) {
      Serial.println("Play/Pause");

      if (currentlyPlaying) {
        spotifyCommand("pause");
      } else {
        spotifyCommand("play");
      }
    }

    if (touchedNext(x, y)) {
      Serial.println("Next");
      spotifyCommand("next");
    }

    while (tft.getTouch(&x, &y)) {
      delay(10);
    }
  }

  if (currentMode == SPOTIFY_MODE) {

    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate > 2000) {

      lastUpdate = millis();

      getCurrentSong();

      if (currentTrackID != lastTrackID) {
        lastTrackID = currentTrackID;
        downloadAlbumArt();
      }

      drawSongInfo();
      updatePlayButton();
      drawProgressBar();
    }
  }
}