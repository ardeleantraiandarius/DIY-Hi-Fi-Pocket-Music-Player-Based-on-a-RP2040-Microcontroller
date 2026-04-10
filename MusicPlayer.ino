#ifndef F_CPU
#define F_CPU 133000000L
#endif

#include <Arduino.h>
#include <I2S.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h" 
#include "AudioGeneratorWAV.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorFLAC.h"
#include "AudioOutputI2S.h"

//PINS CONFIGURATION
#define SD_CS 17
#define I2S_BCLK 10
#define I2S_LRCK 11
#define I2S_DOUT 12

#define BTN_PLAY 20
#define BTN_NEXT 26 
#define BTN_BACK 21

#define OLED_SDA 4
#define OLED_SCL 5

//VARIABLES
Adafruit_SSD1306 display(128, 32, &Wire, -1);

AudioFileSourceSD *file = NULL;
AudioFileSourceBuffer *buff = NULL; 
AudioGenerator *decoder = NULL; 
AudioOutputI2S *out = NULL;

#define MAX_TRACKS 50
String playlist[MAX_TRACKS];
int currentTrack = 0;
int totalTracks = 0; 
int fixedVolume = 30; 

volatile bool isPlaying = false;
volatile bool isPaused = false;
volatile int buttonCommand = 0; // 1=Play/Pause, 2=Next, 3=Back
volatile bool core0Ready = false;

//GUI
String playerState = "";
String displayText = "";
int textX = 0;            
int textWidth = 0;       
unsigned long lastScrollTime = 0;
unsigned long lastDebounce = 0;
String oldState = "";

//COUNTER VARIABLES
unsigned long startMillis = 0;
unsigned long pauseMillis = 0;
int elapsedSeconds = 0;
String lastTimeUpdate = "";

//M:SS format
String formatTime(int totalSeconds) {
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  char buf[10];
  sprintf(buf, "%d:%02d", minutes, seconds);
  return String(buf);
}

//DISPLAY FUNCTION WITH REAL SYMBOLS
void updateDisplay() {
  display.clearDisplay();
  
  //Title
  display.setCursor(0, 0);
  display.print("Music Player"); 
  
  //Status
  if (playerState == "PLAYING") {
    display.fillTriangle(0, 12, 0, 20, 8, 16, SSD1306_WHITE);
    display.setCursor(15, 12);
    display.print("PLAYING");
  } 
  else if (playerState == "PAUSED") {
    display.fillRect(0, 12, 3, 8, SSD1306_WHITE);
    display.fillRect(5, 12, 3, 8, SSD1306_WHITE);
    display.setCursor(15, 12);
    display.print("PAUSED");
  }
  else if (playerState == "FINISHED" || playerState == "READY") {
    display.fillRect(0, 12, 8, 8, SSD1306_WHITE);
    display.setCursor(15, 12);
    display.print(playerState);
  }
  else {
    //SD ERROR, NO MUSIC or FILE ERROR
    display.setCursor(0, 12);
    display.print(playerState);
  }

  //COUNTER
  if (isPlaying || isPaused) {
    String timeStr = formatTime(elapsedSeconds);
    int timeX = 128 - (timeStr.length() * 6);
    display.setCursor(timeX, 12);
    display.print(timeStr);
  }
  
  //Scrolling File Name
  display.setCursor(textX, 24);
  display.print(displayText); 
  
  display.display();
}

//MUSIC FUNCTIONS

void loadMusicFromCard() {
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break; 
    
    if (!entry.isDirectory()) {
      String fileName = String(entry.name());
      String ext = fileName;
      ext.toLowerCase();
      
      if (ext.endsWith(".mp3") || ext.endsWith(".wav") || ext.endsWith(".flac")) {
        if (totalTracks < MAX_TRACKS) {
          playlist[totalTracks] = fileName.startsWith("/") ? fileName : ("/" + fileName);
          totalTracks++;
        }
      }
    }
    entry.close();
  }
}

void stopAudio() {
  if (decoder) {
    if (decoder->isRunning()) decoder->stop();
    delete decoder; 
    decoder = NULL;
  }
  if (buff) { delete buff; buff = NULL; } 
  if (file) { delete file; file = NULL; }
  isPlaying = false;
  isPaused = false; 
  elapsedSeconds = 0;
}

void prepareNewTrackDisplay() {
  String f = playlist[currentTrack];
  displayText = f.startsWith("/") ? f.substring(1) : f;
  textWidth = displayText.length() * 6;
  textX = 0;
}

void playAudio() {
  stopAudio();
  if (totalTracks == 0) return; 
  
  String fileName = playlist[currentTrack];

  file = new AudioFileSourceSD(fileName.c_str());
  buff = new AudioFileSourceBuffer(file, 8192);

  String ext = fileName.substring(fileName.lastIndexOf('.'));
  ext.toLowerCase();

  if (ext == ".mp3") decoder = new AudioGeneratorMP3();
  else if (ext == ".flac") decoder = new AudioGeneratorFLAC();
  else decoder = new AudioGeneratorWAV(); 

  if (decoder && decoder->begin(buff, out)) {
    isPlaying = true;
    isPaused = false;
    playerState = "PLAYING";
    startMillis = millis();
    elapsedSeconds = 0;
  } else {
    stopAudio();
    playerState = "FILE ERROR";
  }
  prepareNewTrackDisplay();
}

//(Audio Engine) SETUP & LOOP

void setup() {
  Serial.begin(115200);
  
  SPI.setRX(16); SPI.setTX(19); SPI.setSCK(18);
  if (!SD.begin(SD_CS)) {
    playerState = "SD ERROR";
    core0Ready = true; 
    while (1) { delay(10); } 
  }

  loadMusicFromCard();

  if (totalTracks == 0) {
    playerState = "NO MUSIC";
    displayText = "SD Card has no music";
    textWidth = displayText.length() * 6;
  } else {
    playerState = "READY";
    prepareNewTrackDisplay();
  }

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  out->SetGain((float)fixedVolume / 100.0);
  
  core0Ready = true; 
}

void loop() {
  if (isPlaying && !isPaused && decoder && decoder->isRunning()) {
    if (!decoder->loop()) {
      stopAudio();
      playerState = "FINISHED";
    }
    //Update counter
    elapsedSeconds = (millis() - startMillis) / 1000;
  }

  if (buttonCommand != 0) {
    int cmd = buttonCommand;
    buttonCommand = 0; 
    
    if (cmd == 1) { 
      if (!isPlaying) {
        playAudio();
      } else {
        //Toggle Pause/Resume
        isPaused = !isPaused;
        if (isPaused) {
          playerState = "PAUSED";
          pauseMillis = millis(); // Record pause time
        } else {
          playerState = "PLAYING";
          startMillis += (millis() - pauseMillis); // Adjust start time for duration of pause
        }
      }
    } 
    else if (cmd == 2) { 
      currentTrack = (currentTrack + 1) % totalTracks;
      playAudio(); 
    }
    else if (cmd == 3) { 
      currentTrack = (currentTrack - 1 + totalTracks) % totalTracks;
      playAudio(); 
    }
  }
}

//GRAPHICS AND BUTTONS
void setup1() {
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  Wire.setSDA(OLED_SDA); Wire.setSCL(OLED_SCL);
  Wire.begin(); 
  Wire.setClock(400000); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false); 

  while (!core0Ready) { delay(10); }
  updateDisplay();
}

void loop1() {
  bool needsRedraw = false;

  if (playerState != oldState) {
    oldState = playerState;
    needsRedraw = true;
  }

  //Redraw if time string changes
  if (isPlaying || isPaused) {
    String currentTime = formatTime(elapsedSeconds);
    if (currentTime != lastTimeUpdate) {
      lastTimeUpdate = currentTime;
      needsRedraw = true;
    }
  }

  if (textWidth > 128) {
    if (millis() - lastScrollTime > 150) {
      textX -= 2; 
      if (textX < -textWidth) { 
        textX = 128; 
      }
      needsRedraw = true;
      lastScrollTime = millis();
    }
  }

  if (needsRedraw) {
    updateDisplay();
  }

  if (millis() - lastDebounce > 50) {
    if (digitalRead(BTN_PLAY) == LOW && totalTracks > 0) {
      buttonCommand = 1;
      delay(250); 
    }
    else if (digitalRead(BTN_NEXT) == LOW && totalTracks > 0) {
      buttonCommand = 2;
      delay(250);
    }
    else if (digitalRead(BTN_BACK) == LOW && totalTracks > 0) {
      buttonCommand = 3;
      delay(250);
    }
    lastDebounce = millis();
  }
}