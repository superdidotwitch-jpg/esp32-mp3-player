// Spider: ESP32 MP3 player, two-knob version
// Parts: ESP32 dev board, DFPlayer Mini, 0.96" SSD1306 OLED (I2C), two KY-040
// rotary encoders, two slide switches (lock and shuffle).
// Board settings in the Arduino IDE: ESP32 Dev Module, Flash Mode DIO.
// Wiring and the story behind it: see docs/ in this repo.
//
// Knob 1: turn = track, click = play / pause (resumes mid-song)
// Knob 2: turn = volume, click = next EQ preset
// Lock switch: knobs do nothing. Shuffle switch: random next track.

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFRobotDFPlayerMini.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial dfSerial(2);
DFRobotDFPlayerMini myDFPlayer;
WebServer server(80);
Preferences prefs;

#define ENC1_CLK 32
#define ENC1_DT  33
#define ENC1_SW  25

#define ENC2_CLK 34
#define ENC2_DT  27
#define ENC2_SW  26

#define LOCK_SW    13
#define SHUFFLE_SW 14

int enc1LastCLK, enc2LastCLK;

int track = 1;
int volume = 15;
bool playing = false;
bool pausedMidSong = false;
int maxTrack = 99;
bool dfReady = false;

const char* eqNames[] = {"Normal", "Pop", "Rock", "Jazz", "Classic", "Bass"};
const int eqCount = 6;
int eqIndex = 0;

bool locked = false;
bool shuffleOn = false;
bool shuffleLast = false;
unsigned long lastFinishedAt = 0;

// Put your own WiFi name and password here. Leave them as they are and the
// player still works: it looks for WiFi for 4 seconds, then carries on without it.
const char* wifiSSID = "YOUR_WIFI_NAME";
const char* wifiPassword = "YOUR_WIFI_PASSWORD";
const char* ntpServer = "pool.ntp.org";
const long gmtOffsetSec = 3600;
const int daylightOffsetSec = 0;

bool wifiOn = false;
unsigned long wifiLostSince = 0;

char timeString[9] = "--:--:--";

// ---------- Screensaver ----------
const unsigned long screensaverDelay = 5000;  // no input for this long = screensaver (ms)
const unsigned long nameTime = 5000;          // how long the name stays up (ms)
const int kickSpeed = 4;                      // pixels moved per frame (bigger = faster)
const int frameTime = 30;                     // ms per frame (bigger = slower)

bool screensaverOn = false;
unsigned long lastActivity = 0;
int ssPhase = 0;                 // 0 = showing name, 1 = kick sliding
unsigned long ssPhaseStart = 0;
unsigned long ssLastFrame = 0;
int ssKickX = 0;

// Your flying kick, 60 x 46 pixels
#define KICK_W 60
#define KICK_H 46
const unsigned char kickPic[] PROGMEM = {
  0x00, 0x1F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x7F, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0xC0,
  0x03, 0xFF, 0xFF, 0xFF, 0xE0, 0x03, 0xE7, 0xE0, 0x0F, 0xFF, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xE0,
  0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
  0x3F, 0xFF, 0xFF, 0x83, 0xFF, 0xFF, 0xFE, 0xF0, 0x7F, 0xFF, 0xFF, 0x80, 0xFF, 0xFF, 0xF8, 0x60,
  0x7C, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xF0, 0x00, 0x70, 0x3F, 0xFF, 0xE7, 0xFF, 0xFF, 0xF0, 0x00,
  0xF0, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0xF0, 0x1F, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
  0xE0, 0x1F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00, 0xE0, 0x0F, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
  0xE0, 0x0F, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xE0, 0x07, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0x00,
  0xE0, 0x07, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0xE0, 0x03, 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00,
  0xE0, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xF0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xE3, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xE7, 0xF0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xFF, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0x80, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};

void saveState() {
  prefs.putInt("track", track);
  prefs.putInt("volume", volume);
  prefs.putBool("playing", playing);
  prefs.putInt("eq", eqIndex);
}

int randomTrack() {
  if (maxTrack <= 1) return 1;
  int t = track;
  while (t == track) {
    t = random(1, maxTrack + 1);
  }
  return t;
}

void playTrack() {
  myDFPlayer.play(track);
  pausedMidSong = false;
}

void turnWifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiOn = false;
  Serial.println("WiFi switched off to save battery");
}

void showBootMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Spider");
  display.println();
  display.println(line1);
  display.println(line2);
  display.display();
}

void updateDisplay() {
  if (screensaverOn) return;   // screensaver owns the screen

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Spider");

  if (locked) {
    display.setCursor(80, 0);
    display.print("LCK");
  }
  if (shuffleOn) {
    display.setCursor(104, 0);
    display.print("SHF");
  }

  display.setCursor(0, 8);
  display.print("Track: ");
  display.print(track);
  display.print("/");
  display.println(maxTrack);
  display.print("Vol:   ");
  display.println(volume);
  display.print("State: ");
  display.println(playing ? "Playing" : "Paused");
  display.print("Time:  ");
  display.println(timeString);
  display.print("EQ:    ");
  display.println(eqNames[eqIndex]);

  if (!dfReady) {
    display.println("No DFPlayer!");
  }

  if (playing) {
    int barBase = 58;
    int barX = 90;
    for (int i = 0; i < 4; i++) {
      int seed = (millis() / 150) + i * 3;
      int h = 2 + (seed % 6);
      display.fillRect(barX + i * 6, barBase - h, 4, h, SSD1306_WHITE);
    }
  }

  display.display();
}

// ---------- Screensaver drawing ----------
void showName() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Yellow strip
  display.setTextSize(2);
  display.setCursor(28, 0);
  display.print("FOLLOW");

  // Blue part
  display.setTextSize(3);
  display.setCursor(10, 16);
  display.print("@Super");
  display.setCursor(1, 40);
  display.print("DidZero");

  display.display();
}

void startScreensaver() {
  screensaverOn = true;
  ssPhase = 0;
  ssPhaseStart = millis();
  showName();
}

// Draws the next step of the screensaver without stopping the music
void runScreensaver() {
  if (ssPhase == 0) {
    if (millis() - ssPhaseStart >= nameTime) {
      ssPhase = 1;
      ssKickX = -KICK_W;
      ssLastFrame = 0;
    }
  } else {
    if (millis() - ssLastFrame >= (unsigned long)frameTime) {
      ssLastFrame = millis();
      display.clearDisplay();
      display.drawBitmap(ssKickX, 17, kickPic, KICK_W, KICK_H, SSD1306_WHITE);
      display.display();
      ssKickX += kickSpeed;
      if (ssKickX > 128) {
        ssPhase = 0;
        ssPhaseStart = millis();
        showName();
      }
    }
  }
}

// Call on any knob or switch input.
// Returns true if the screen was asleep, so that first touch only wakes it up.
bool wakeUp() {
  lastActivity = millis();
  if (screensaverOn) {
    screensaverOn = false;
    updateDisplay();
    return true;
  }
  return false;
}

void updateTimeString() {
  struct tm timeinfo;
  // 0 = don't wait; if the time was never set, the dashes stay
  if (getLocalTime(&timeinfo, 0)) {
    sprintf(timeString, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
}

void handleRoot() {
  String html = "<html><head><meta http-equiv='refresh' content='2'>";
  html += "<title>Spider</title></head><body style='font-family:sans-serif'>";
  html += "<h1>Spider</h1>";
  html += "<p><b>Track:</b> " + String(track) + " of " + String(maxTrack) + "</p>";
  html += "<p><b>Volume:</b> " + String(volume) + "</p>";
  html += "<p><b>State:</b> " + String(playing ? "Playing" : "Paused") + "</p>";
  html += "<p><b>EQ:</b> " + String(eqNames[eqIndex]) + "</p>";
  html += "<p><b>Lock:</b> " + String(locked ? "On" : "Off") + "</p>";
  html += "<p><b>Shuffle:</b> " + String(shuffleOn ? "On" : "Off") + "</p>";
  html += "<p><b>Time:</b> " + String(timeString) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  // Give the screen and DFPlayer time to power up after the switch is flipped
  delay(1500);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showBootMessage("Starting...", "Waiting for DFPlayer");

  // Keep trying until the DFPlayer answers (up to about 10 seconds)
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);
  for (int i = 0; i < 10 && !dfReady; i++) {
    dfReady = myDFPlayer.begin(dfSerial);
    if (!dfReady) {
      Serial.println("DFPlayer not ready yet, retrying...");
      delay(1000);
    }
  }
  Serial.println(dfReady ? "DFPlayer ready" : "DFPlayer not found");

  // Ask for the song count, retrying until the card has been read
  int fileCount = -1;
  if (dfReady) {
    showBootMessage("DFPlayer found", "Reading SD card...");
    for (int i = 0; i < 5 && fileCount <= 0; i++) {
      delay(500);
      fileCount = myDFPlayer.readFileCounts();
    }
  }
  if (fileCount > 0) {
    maxTrack = fileCount;
  }
  Serial.print("Tracks on card: ");
  Serial.println(fileCount > 0 ? fileCount : 0);

  prefs.begin("spider", false);
  track = prefs.getInt("track", 1);
  volume = prefs.getInt("volume", 15);
  playing = prefs.getBool("playing", false);
  eqIndex = prefs.getInt("eq", 0);
  if (track > maxTrack) track = 1;
  if (eqIndex < 0 || eqIndex >= eqCount) eqIndex = 0;

  myDFPlayer.volume(volume);
  delay(100);
  myDFPlayer.EQ(eqIndex);

  pinMode(ENC1_CLK, INPUT);
  pinMode(ENC1_DT, INPUT);
  pinMode(ENC1_SW, INPUT_PULLUP);

  pinMode(ENC2_CLK, INPUT);
  pinMode(ENC2_DT, INPUT);
  pinMode(ENC2_SW, INPUT_PULLUP);

  pinMode(LOCK_SW, INPUT_PULLUP);
  pinMode(SHUFFLE_SW, INPUT_PULLUP);

  enc1LastCLK = digitalRead(ENC1_CLK);
  enc2LastCLK = digitalRead(ENC2_CLK);

  locked = digitalRead(LOCK_SW) == LOW;
  shuffleOn = digitalRead(SHUFFLE_SW) == LOW;
  shuffleLast = shuffleOn;

  // Look for home WiFi for about 4 seconds only
  showBootMessage("Looking for WiFi...", "");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 8) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiOn = true;
    Serial.println("\nWiFi connected");
    Serial.print("Spider's IP address: ");
    Serial.println(WiFi.localIP());
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

    server.on("/", handleRoot);
    server.begin();
    Serial.println("Web dashboard started");

    ArduinoOTA.setHostname("spider-mp3");
    ArduinoOTA.onStart([]() {
      Serial.println("OTA update starting...");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("OTA update finished");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA error [%u]\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready");
  } else {
    Serial.println("\nHome WiFi not found");
    turnWifiOff();
  }

  if (playing) {
    playTrack();
  }

  updateDisplay();
  lastActivity = millis();
  Serial.println("Ready.");
}

unsigned long lastTimeUpdate = 0;
unsigned long lastVisualizerUpdate = 0;

void loop() {
  if (wifiOn) {
    server.handleClient();
    ArduinoOTA.handle();

    // Left home? After 30 seconds without WiFi, switch it off
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiLostSince == 0) {
        wifiLostSince = millis();
      } else if (millis() - wifiLostSince > 30000) {
        turnWifiOff();
      }
    } else {
      wifiLostSince = 0;
    }
  }

  // Screensaver: start after a few seconds without input, keep it animating
  if (!screensaverOn && millis() - lastActivity > screensaverDelay) {
    startScreensaver();
  }
  if (screensaverOn) {
    runScreensaver();
  }

  // Switches (always applied, and they wake the screen)
  bool lockNow = digitalRead(LOCK_SW) == LOW;
  if (lockNow != locked) {
    locked = lockNow;
    Serial.println(locked ? "Lock ON" : "Lock OFF");
    wakeUp();
    updateDisplay();
  }

  shuffleOn = digitalRead(SHUFFLE_SW) == LOW;
  if (shuffleOn != shuffleLast) {
    Serial.println(shuffleOn ? "Shuffle ON" : "Shuffle OFF");
    if (shuffleOn) {
      track = randomTrack();
      pausedMidSong = false;
      if (playing) playTrack();
      saveState();
    }
    shuffleLast = shuffleOn;
    wakeUp();
    updateDisplay();
  }

  // Song ended: random next track if shuffle is on, otherwise next in order
  if (myDFPlayer.available()) {
    if (myDFPlayer.readType() == DFPlayerPlayFinished && millis() - lastFinishedAt > 1000) {
      lastFinishedAt = millis();
      if (shuffleOn) {
        track = randomTrack();
      } else {
        track++;
        if (track > maxTrack) track = 1;
      }
      playTrack();
      saveState();
      updateDisplay();
    }
  }

  if (millis() - lastTimeUpdate > 1000) {
    updateTimeString();
    updateDisplay();
    lastTimeUpdate = millis();
  }

  if (playing && millis() - lastVisualizerUpdate > 150) {
    updateDisplay();
    lastVisualizerUpdate = millis();
  }

  // Encoders: read positions always, but only act when unlocked
  int enc1CLK = digitalRead(ENC1_CLK);
  int enc2CLK = digitalRead(ENC2_CLK);

  if (locked) {
    // Locked: knobs do nothing, but touching them still wakes the screen
    if (enc1CLK != enc1LastCLK || enc2CLK != enc2LastCLK ||
        digitalRead(ENC1_SW) == LOW || digitalRead(ENC2_SW) == LOW) {
      wakeUp();
    }
    enc1LastCLK = enc1CLK;
    enc2LastCLK = enc2CLK;
    return;
  }

  // Encoder 1, turn: change track
  if (enc1CLK != enc1LastCLK && enc1CLK == HIGH) {
    if (!wakeUp()) {
      bool cw = digitalRead(ENC1_DT) != enc1CLK;
      track += cw ? 1 : -1;
      if (track < 1) track = 1;
      if (track > maxTrack) track = maxTrack;
      pausedMidSong = false;
      if (playing) playTrack();
      saveState();
      updateDisplay();
    }
  }
  enc1LastCLK = enc1CLK;

  // Encoder 1, click: play / pause (resumes where it stopped)
  if (digitalRead(ENC1_SW) == LOW) {
    if (!wakeUp()) {
      if (playing) {
        myDFPlayer.pause();
        playing = false;
        pausedMidSong = true;
      } else {
        if (pausedMidSong) {
          myDFPlayer.start();
          pausedMidSong = false;
        } else {
          playTrack();
        }
        playing = true;
      }
      saveState();
      updateDisplay();
    }
    delay(300);
  }

  // Encoder 2, turn: volume
  if (enc2CLK != enc2LastCLK && enc2CLK == HIGH) {
    if (!wakeUp()) {
      bool cw = digitalRead(ENC2_DT) != enc2CLK;
      volume += cw ? 1 : -1;
      if (volume < 0) volume = 0;
      if (volume > 30) volume = 30;
      myDFPlayer.volume(volume);
      saveState();
      updateDisplay();
    }
  }
  enc2LastCLK = enc2CLK;

  // Encoder 2, click: next EQ preset
  if (digitalRead(ENC2_SW) == LOW) {
    if (!wakeUp()) {
      eqIndex = (eqIndex + 1) % eqCount;
      myDFPlayer.EQ(eqIndex);
      saveState();
      updateDisplay();
    }
    delay(300);
  }
}
