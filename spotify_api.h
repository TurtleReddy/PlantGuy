#ifndef SPOTIFY_API_H
#define SPOTIFY_API_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Base64.h>

String spotifyAccessToken = "";

String currentSong = "";
String currentArtist = "";
String currentAlbum = "";
String albumArtURL = "";
String currentTrackID = "";

bool currentlyPlaying = false;

int currentProgress = 0;
int songDuration = 0;

const char* refreshToken = "CHANGE THIS TO YOUR REFRESH TOKEN";

extern const char* clientID;
extern const char* clientSecret;


bool refreshSpotifyToken() {
  HTTPClient http;

  http.begin("https://accounts.spotify.com/api/token");

  http.addHeader(
    "Content-Type",
    "application/x-www-form-urlencoded");


  String auth =
    String(clientID) + ":" + String(clientSecret);


  String encoded =
    base64::encode(auth);

  http.addHeader(
    "Authorization",
    "Basic " + encoded);


  String body =
    "grant_type=refresh_token&refresh_token="
    + String(refreshToken);


  int response =
    http.POST(body);


  if (response != 200) {
    Serial.println("Token refresh failed");
    Serial.println(response);
    Serial.println(http.getString());

    http.end();
    return false;
  }


  String payload =
    http.getString();


  DynamicJsonDocument doc(4096);

  deserializeJson(doc, payload);


  spotifyAccessToken =
    doc["access_token"].as<String>();


  Serial.println("Spotify token refreshed!");

  http.end();

  return true;
}



void getCurrentSong() {
  HTTPClient http;

  http.begin(
    "https://api.spotify.com/v1/me/player/currently-playing");

  http.addHeader(
    "Authorization",
    "Bearer " + spotifyAccessToken);

  Serial.println("Requesting current song...");

  int response = http.GET();

  Serial.print("Response code: ");
  Serial.println(response);

  if (response == 200) {
    String payload = http.getString();

    DynamicJsonDocument doc(8192);

    deserializeJson(doc, payload);


    currentSong =
      doc["item"]["name"].as<String>();

    currentTrackID =
      doc["item"]["id"].as<String>();

    currentArtist =
      doc["item"]["artists"][0]["name"].as<String>();

    currentAlbum =
      doc["item"]["album"]["name"].as<String>();

    albumArtURL =
      doc["item"]["album"]["images"][1]["url"].as<String>();

    currentlyPlaying =
      doc["is_playing"];

    currentProgress =
      doc["progress_ms"];

    songDuration =
      doc["item"]["duration_ms"];

    Serial.println("================");
    Serial.println(currentSong);
    Serial.println(currentArtist);
    Serial.println(currentAlbum);
    Serial.println(albumArtURL);
    Serial.print("Playing: ");
    Serial.println(currentlyPlaying);
    Serial.println("================");
  }

  else {
    Serial.print("Spotify error: ");
    Serial.println(response);

    delay(100);
  }

  http.end();
}


#endif