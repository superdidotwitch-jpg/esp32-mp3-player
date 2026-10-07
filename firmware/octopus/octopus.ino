// Octopus: the one-knob version of the ESP32 MP3 player.
// One rotary encoder does everything:
//   turn        = change the value of the current mode
//   short click = play / pause
//   hold 0.6 s  = go to the next mode (volume, track, EQ, shuffle)
//
// Before uploading, put your own WiFi name and password in the two
// lines marked YOUR_WIFI_NAME and YOUR_WIFI_PASSWORD below.
// The player works without WiFi too. It just has no clock, no web page
// and no wireless updates.

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

// ---------- Pins (same as Spider's Encoder 1) ----------
#define ENC_CLK 32
#define ENC_DT  33
#define ENC_SW  25
// OLED: SDA = GPIO21, SCL = GPIO22
// DFPlayer: its TX -> GPIO16 (RX2), its RX -> GPIO17 (TX2)

int encLastCLK;

// ---------- Knob modes ----------
// Turn the knob = change the value of the current mode
// Short click   = play / pause
// Hold ~0.6s    = go to the next mode
enum { MODE_VOL, MODE_TRACK, MODE_EQ, MODE_SHUFFLE };
const int modeCount = 4;
int mode = MODE_VOL;
const unsigned long longPressTime = 600;   // ms to count as a hold

bool btnDown = false;
bool longDone = false;
unsigned long btnDownAt = 0;

int track = 1;
int volume = 15;
bool playing = false;
bool pausedMidSong = false;
int maxTrack = 99;
bool dfReady = false;

const char* eqNames[] = {"Normal", "Pop", "Rock", "Jazz", "Classic", "Bass"};
const int eqCount = 6;
int eqIndex = 0;

bool shuffleOn = false;
unsigned long lastFinishedAt = 0;

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
const char* ssText = "IR15";                  // the text that gets typed out
const unsigned long typeSpeed = 400;          // ms between each letter appearing
const unsigned long typeHold = 1000;          // ms the full text stays before the guitar
const unsigned long guitarTime = 6000;        // ms of guitar + flying notes
const int frameTime = 60;                     // ms per animation frame

bool screensaverOn = false;
unsigned long lastActivity = 0;
int ssPhase = 0;                 // 0 = typing IR15, 1 = guitar with notes
unsigned long ssPhaseStart = 0;
unsigned long ssLastFrame = 0;

void saveState() {
  prefs.putInt("track", track);
  prefs.putInt("volume", volume);
  prefs.putBool("playing", playing);
  prefs.putInt("eq", eqIndex);
  prefs.putBool("shuffle", shuffleOn);
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
  display.println("Octopus");
  display.println();
  display.println(line1);
  display.println(line2);
  display.display();
}

// Prints one screen line, with a ">" in front if it's the knob's current mode
void printLine(int row, bool selected, const char* label, String value) {
  display.setCursor(0, row * 8);
  display.print(selected ? ">" : " ");
  display.setCursor(8, row * 8);
  display.print(label);
  display.print(value);
}

void updateDisplay() {
  if (screensaverOn) return;   // screensaver owns the screen

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Octopus");
  if (shuffleOn) {
    display.setCursor(104, 0);
    display.print("SHF");
  }

  printLine(1, mode == MODE_TRACK, "Track: ", String(track) + "/" + String(maxTrack));
  printLine(2, mode == MODE_VOL,   "Vol:   ", String(volume));
  printLine(3, false,              "State: ", playing ? "Playing" : "Paused");
  printLine(4, false,              "Time:  ", timeString);
  printLine(5, mode == MODE_EQ,    "EQ:    ", eqNames[eqIndex]);
  printLine(6, mode == MODE_SHUFFLE, "Shuf:  ", shuffleOn ? "On" : "Off");

  if (!dfReady) {
    display.setCursor(0, 56);
    display.print("No DFPlayer!");
  }

  if (playing) {
    int barBase = 63;
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

// One music note with its head at (x, y). double = two notes joined by a beam
void drawNote(int x, int y, bool isDouble) {
  display.fillCircle(x, y, 2, SSD1306_WHITE);
  display.drawLine(x + 2, y, x + 2, y - 9, SSD1306_WHITE);
  if (isDouble) {
    display.fillCircle(x + 7, y - 1, 2, SSD1306_WHITE);
    display.drawLine(x + 9, y - 1, x + 9, y - 10, SSD1306_WHITE);
    display.drawLine(x + 2, y - 9, x + 9, y - 10, SSD1306_WHITE);
    display.drawLine(x + 2, y - 8, x + 9, y - 9, SSD1306_WHITE);
  } else {
    display.drawLine(x + 2, y - 9, x + 5, y - 6, SSD1306_WHITE);
    display.drawLine(x + 3, y - 9, x + 6, y - 6, SSD1306_WHITE);
  }
}

// A guitar lying sideways along the bottom. strum = which way the string wobbles
void drawGuitar(bool strum) {
  display.fillCircle(22, 46, 14, SSD1306_WHITE);      // big part of the body
  display.fillCircle(40, 46, 10, SSD1306_WHITE);      // small part of the body
  display.fillCircle(35, 46, 5, SSD1306_BLACK);       // sound hole
  display.fillRect(48, 44, 57, 5, SSD1306_WHITE);     // neck
  display.fillRect(104, 42, 13, 9, SSD1306_WHITE);    // headstock
  for (int px = 107; px <= 115; px += 4) {            // tuning pegs
    display.drawPixel(px, 40, SSD1306_WHITE);
    display.drawPixel(px, 52, SSD1306_WHITE);
  }
  display.fillRect(14, 44, 3, 5, SSD1306_BLACK);      // bridge
  display.drawLine(17, 45, 104, 45, SSD1306_BLACK);   // strings
  display.drawLine(17, 47, 104, 47, SSD1306_BLACK);
  int off = strum ? 1 : -1;                           // middle string vibrating
  display.drawLine(20, 46, 30, 46 + off, SSD1306_BLACK);
  display.drawLine(30, 46 + off, 40, 46, SSD1306_BLACK);
}

void startScreensaver() {
  screensaverOn = true;
  mode = MODE_VOL;   // knob goes back to volume when you leave it alone
  ssPhase = 0;
  ssPhaseStart = millis();
  ssLastFrame = 0;
}

// Draws the next frame of the screensaver without stopping the music
void runScreensaver() {
  if (millis() - ssLastFrame < (unsigned long)frameTime) return;
  ssLastFrame = millis();
  unsigned long t = millis() - ssPhaseStart;
  int len = strlen(ssText);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (ssPhase == 0) {
    // Phase 1: IR15 typed one character at a time, with a blinking cursor
    int shown = 1 + t / typeSpeed;
    if (shown > len) shown = len;
    int x0 = (128 - len * 24) / 2;
    display.setTextSize(4);
    display.setCursor(x0, 20);
    for (int i = 0; i < shown; i++) display.print(ssText[i]);
    bool typing = t < (unsigned long)len * typeSpeed;
    if (typing && (t / 200) % 2 == 0) {
      display.fillRect(x0 + shown * 24, 44, 20, 4, SSD1306_WHITE);
    }
    if (t >= len * typeSpeed + typeHold) {
      ssPhase = 1;
      ssPhaseStart = millis();
    }
  } else {
    // Phase 2: small IR15 in the yellow strip, guitar strumming, notes flying out
    display.setTextSize(2);
    display.setCursor((128 - len * 12) / 2, 0);
    display.print(ssText);
    drawGuitar((t / 120) % 2 == 0);
    for (int k = 0; k < 6; k++) {
      if (t < (unsigned long)k * 600) continue;
      unsigned long tt = (t - k * 600) % 3000;
      int x = 34 + tt / 16;
      int y = 40 - tt / 50 + (int)(3 * sin(tt / 180.0 + k));
      if (y > -12 && x < 130) drawNote(x, y, k % 2 == 1);
    }
    if (t >= guitarTime) {
      ssPhase = 0;
      ssPhaseStart = millis();
    }
  }
  display.display();
}

// Call on any knob input.
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

void togglePlay() {
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

// What turning the knob does depends on the mode
void turnKnob(int dir) {
  if (mode == MODE_VOL) {
    volume += dir;
    if (volume < 0) volume = 0;
    if (volume > 30) volume = 30;
    myDFPlayer.volume(volume);
  } else if (mode == MODE_TRACK) {
    track += dir;
    if (track < 1) track = 1;
    if (track > maxTrack) track = maxTrack;
    pausedMidSong = false;
    if (playing) playTrack();
  } else if (mode == MODE_EQ) {
    eqIndex = (eqIndex + dir + eqCount) % eqCount;
    myDFPlayer.EQ(eqIndex);
  } else if (mode == MODE_SHUFFLE) {
    shuffleOn = !shuffleOn;
    Serial.println(shuffleOn ? "Shuffle ON" : "Shuffle OFF");
    if (shuffleOn) {
      track = randomTrack();
      pausedMidSong = false;
      if (playing) playTrack();
    }
  }
  saveState();
  updateDisplay();
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
  html += "<title>Octopus</title></head><body style='font-family:sans-serif'>";
  html += "<h1>Octopus</h1>";
  html += "<p><b>Track:</b> " + String(track) + " of " + String(maxTrack) + "</p>";
  html += "<p><b>Volume:</b> " + String(volume) + "</p>";
  html += "<p><b>State:</b> " + String(playing ? "Playing" : "Paused") + "</p>";
  html += "<p><b>EQ:</b> " + String(eqNames[eqIndex]) + "</p>";
  html += "<p><b>Shuffle:</b> " + String(shuffleOn ? "On" : "Off") + "</p>";
  html += "<p><b>Time:</b> " + String(timeString) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  // Give the screen and DFPlayer time to power up
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

  prefs.begin("octopus", false);
  track = prefs.getInt("track", 1);
  volume = prefs.getInt("volume", 15);
  playing = prefs.getBool("playing", false);
  eqIndex = prefs.getInt("eq", 0);
  shuffleOn = prefs.getBool("shuffle", false);
  if (track > maxTrack) track = 1;
  if (eqIndex < 0 || eqIndex >= eqCount) eqIndex = 0;

  myDFPlayer.volume(volume);
  delay(100);
  myDFPlayer.EQ(eqIndex);

  pinMode(ENC_CLK, INPUT);
  pinMode(ENC_DT, INPUT);
  pinMode(ENC_SW, INPUT_PULLUP);
  encLastCLK = digitalRead(ENC_CLK);

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
    Serial.print("Octopus's IP address: ");
    Serial.println(WiFi.localIP());
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

    server.on("/", handleRoot);
    server.begin();
    Serial.println("Web dashboard started");

    ArduinoOTA.setHostname("octopus-mp3");
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

  // Knob turn: one step per click
  int encCLK = digitalRead(ENC_CLK);
  if (encCLK != encLastCLK && encCLK == HIGH) {
    if (!wakeUp()) {
      bool cw = digitalRead(ENC_DT) != encCLK;
      turnKnob(cw ? 1 : -1);
    }
  }
  encLastCLK = encCLK;

  // Knob button: short click = play/pause, hold = next mode
  bool pressed = digitalRead(ENC_SW) == LOW;
  if (pressed && !btnDown) {
    btnDown = true;
    longDone = false;
    btnDownAt = millis();
  }
  if (pressed && btnDown && !longDone && millis() - btnDownAt >= longPressTime) {
    longDone = true;
    if (!wakeUp()) {
      mode = (mode + 1) % modeCount;
      updateDisplay();
    }
  }
  if (!pressed && btnDown) {
    btnDown = false;
    if (!longDone && millis() - btnDownAt > 30) {   // ignore tiny bounces
      if (!wakeUp()) {
        togglePlay();
      }
    }
  }
}
