#include <SPI.h>
#include <FS.h>
#include <Wire.h>
#include <SD.h>
#include "M5Cardputer.h"
#include "Audio.h"  // https://github.com/schreibfaul1/ESP32-audioI2S   version 2.0.0
#include "font.h"
#include <ESP32Time.h>  // https://github.com/fbiego/ESP32Time  verison 2.0.6
M5Canvas sprite(&M5Cardputer.Display);
M5Canvas spr(&M5Cardputer.Display);

// microSD card
#define SD_SCK 40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS 12

// I2S
#define I2S_DOUT 42
#define I2S_BCLK 41
#define I2S_LRCK 43

#define MAX_FILES 100
Audio audio;


unsigned short grays[18];
unsigned short gray;
unsigned short light;
int currentTrack = 0;
int m = 0;
int volume = 11;
int bri = 0;
int brightness[5] = { 50, 100, 150, 200, 250 };
bool isPlaying = true;
bool isStopped = false;
bool nextS = 0;
bool volUp = 0;
int g[14] = { 0 };
int graphSpeed = 0;
int textPos = 60;
int sliderPos = 0;
bool shuffleMode = false;


// Task handle for audio task
TaskHandle_t handleAudioTask = NULL;

ESP32Time rtc(0);
const char *MUSIC_DIR = "/music";  // Default directory
// Audio file extensions to support
const char *SUPPORTED_EXTENSIONS[] = { ".mp3", ".aac", ".m4a" };
const int NUM_SUPPORTED_EXTENSIONS = sizeof(SUPPORTED_EXTENSIONS) / sizeof(SUPPORTED_EXTENSIONS[0]);

const unsigned short BORDER_COLOR = BLACK;
const int LIST_X_START = 8;       // Starting X position of the list
const int LIST_Y_START = 10;      // Starting Y position of the list
const int LIST_WIDTH = 120;       // Width available for the list
const int LIST_ITEM_HEIGHT = 10;  // Height of each list item
const int MAX_VISIBLE_ITEMS = 8;  // Number of visible items in the list
const int VISUALIZER_Y = LIST_Y_START + (MAX_VISIBLE_ITEMS * LIST_ITEM_HEIGHT) + 5;
const int VISUALIZER_HEIGHT = 25;
const int PLAYER_DETAILS_X = 148;
const int PLAY_STOP_X = PLAYER_DETAILS_X + 5;   // X position for play/stop indicators
const int PLAY_STOP_Y = 18;                     // Y position for play/stop indicators
const int TIME_INDICATOR_Y = PLAY_STOP_Y + 15;  // Y position for the clock and play icon
const int PLAY_ICON_X = PLAYER_DETAILS_X + 10;

const int MIN_VOLUME = 1;
const int MAX_VOLUME = 21;  // Maximum volume for the Audio library (usually 21)
const int VOLUME_STEP = 2;

const int NUM_VISUALIZER_BARS = 14;               // Define the number of bars
int visualizerData[NUM_VISUALIZER_BARS] = { 0 };  // Declare the array
// Array to store file names
String audioFiles[MAX_FILES];
int fileCount = 0;
int currentlyPlayingTrack = -1;  // Initialize to -1 to indicate no song is playing

void resetClock() {
  rtc.setTime(0, 0, 0, 17, 1, 2021);
}
bool isSupportedFile(const String &filename) {
  String lowerCaseFilename = filename;
  lowerCaseFilename.toLowerCase();  // Convert to lowercase for case-insensitive comparison

  for (int i = 0; i < NUM_SUPPORTED_EXTENSIONS; i++) {
    if (lowerCaseFilename.endsWith(SUPPORTED_EXTENSIONS[i])) {
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial.println("hi");
  resetClock();

  // Initialize M5Cardputer and SD card
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(brightness[bri]);
  sprite.createSprite(240, 135);
  spr.createSprite(86, 16);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)) {
    Serial.println(F("ERROR: SD Mount Failed!"));
    while (1)
      ;  // Halt if SD card fails
  }
  Serial.println("Listing root directory:");
  File root = SD.open("/");  // Open the root directory
  if (root) {
    File file = root.openNextFile();
    while (file) {
      Serial.print("Found in root: ");
      Serial.println(file.name());  // Print EVERYTHING in the root
      file = root.openNextFile();
    }
    root.close();
  } else {
    Serial.println("Failed to open root directory!");
  }

  if (!SD.exists(MUSIC_DIR)) {
    SD.mkdir(MUSIC_DIR);
    Serial.println("Created Music Directory");
  }
  listFiles(SD, MUSIC_DIR);

  // Initialize audio output
  audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  audio.setVolume(volume);  // 0...21
  audio.connecttoFS(SD, audioFiles[currentTrack].c_str());

  int co = 214;
  for (int i = 0; i < 18; i++) {
    grays[i] = M5Cardputer.Display.color565(co, co, co + 40);
    co = co - 13;
  }

  // Create tasks and pin them to different cores
  xTaskCreatePinnedToCore(Task_TFT, "Task_TFT", 20480, NULL, 2, NULL, 0);                  // Core 0
  xTaskCreatePinnedToCore(Task_Audio, "Task_Audio", 10240, NULL, 3, &handleAudioTask, 1);  // Core 1
}

void loop() {
}

String getFilenameFromPath(const String &path, int maxLength = 15) {  // Added maxLength
  int lastSlash = path.lastIndexOf('/');
  String filename = (lastSlash == -1) ? path : path.substring(lastSlash + 1);

  if (filename.length() > maxLength) {
    return filename.substring(0, maxLength) + "..";  // Truncate and add "..."
  }
  return filename;
}
void draw() {
  if (graphSpeed == 0) {
    gray = grays[15];
    light = grays[11];
    sprite.fillRect(0, 0, 240, 135, gray);
    sprite.fillRect(4, 8, 130, 122, BLACK);

    sprite.fillRect(129, 8, 5, 122, 0x0841);

    sliderPos = map(currentTrack, 0, fileCount, 8, 110);
    sprite.fillRect(129, sliderPos, 5, 20, grays[2]);
    sprite.fillRect(131, sliderPos + 4, 1, 12, grays[16]);

    sprite.fillRect(4, 2, 50, 2, CYAN);
    sprite.fillRect(84, 2, 50, 2, CYAN);
    sprite.fillRect(190, 6, 45, 3, grays[4]);
    sprite.drawFastVLine(3, 9, 120, light);
    sprite.drawFastVLine(134, 9, 120, light);
    sprite.drawFastHLine(3, 129, 130, light);

    sprite.drawFastHLine(0, 0, 240, light);
    sprite.drawFastHLine(0, 134, 240, light);

    sprite.fillRect(139, 0, 3, 135, BLACK);
    sprite.fillRect(148, 14, 86, 42, BLACK);
    sprite.fillRect(148, 59, 86, 16, BLACK);

    sprite.fillTriangle(162, 18, 162, 26, 168, 22, GREEN);
    sprite.drawFastVLine(143, 0, 135, light);

    sprite.drawFastVLine(238, 0, 135, light);
    sprite.drawFastVLine(138, 0, 135, light);
    sprite.drawFastVLine(148, 14, 42, light);
    sprite.drawFastHLine(148, 14, 86, light);

    //buttons
    for (int i = 0; i < 4; i++)
      sprite.fillRoundRect(148 + (i * 22), 94, 18, 18, 3, grays[4]);

    //button icons
    sprite.fillRect(220, 104, 8, 2, grays[13]);
    sprite.fillRect(220, 108, 8, 2, grays[13]);
    sprite.fillTriangle(228, 102, 228, 106, 231, 105, grays[13]);
    sprite.fillTriangle(220, 106, 220, 110, 217, 109, grays[13]);

    if (!isStopped) {
      sprite.fillRect(152, 104, 3, 6, grays[13]);
      sprite.fillRect(157, 104, 3, 6, grays[13]);
    } else {
      sprite.fillTriangle(156, 102, 156, 110, 160, 106, grays[13]);
    }

    //volume bar
    sprite.fillRoundRect(172, 82, 60, 3, 2, MAGENTA);
    sprite.fillRoundRect(155 + ((volume / 5) * 17), 80, 10, 8, 2, grays[2]);
    sprite.fillRoundRect(157 + ((volume / 5) * 17), 82, 6, 4, 2, grays[10]);

    // brightness
    sprite.fillRoundRect(172, 124, 30, 3, 2, MAGENTA);
    sprite.fillRoundRect(172 + (bri * 5), 122, 10, 8, 2, grays[2]);
    sprite.fillRoundRect(174 + (bri * 5), 124, 6, 4, 2, grays[10]);

    //BATTERY
    sprite.drawRect(206, 119, 28, 12, GREEN);
    sprite.fillRect(234, 122, 3, 6, GREEN);
    //Setting fonts and layout
    sprite.setTextFont(0);
    sprite.setTextDatum(0);
    int startItem = max(0, currentTrack - (MAX_VISIBLE_ITEMS / 2));
    int endItem = min(fileCount, startItem + MAX_VISIBLE_ITEMS);

    for (int i = startItem; i < endItem; i++) {
      int yPos = LIST_Y_START + ((i - startItem) * LIST_ITEM_HEIGHT);
      String filename = getFilenameFromPath(audioFiles[i]);
      int textWidth = sprite.textWidth(filename);

      if (textWidth > LIST_WIDTH) {
        // *** Corrected Truncation Logic ***
        int charsToFit = LIST_WIDTH * filename.length() / textWidth;  // Calculate chars to fit
        filename = filename.substring(0, charsToFit - 2) + "..";      // Truncate and add ".."
        textWidth = sprite.textWidth(filename);
      }
      int xPos = LIST_X_START;
      if (textWidth < LIST_WIDTH)
        xPos = LIST_X_START + (LIST_WIDTH - textWidth) / 2;

      if (i == currentTrack) sprite.setTextColor(WHITE, BLACK);
      else sprite.setTextColor(CYAN, BLACK);

      sprite.drawString(filename, xPos, yPos);
    }
    sprite.drawFastHLine(LIST_X_START - 1, VISUALIZER_Y - 3, LIST_WIDTH + 2, BORDER_COLOR);

    //graph
    sprite.fillRect(4, VISUALIZER_Y, 130, VISUALIZER_HEIGHT, light);  // Clear visualizer area
    for (int i = 0; i < 14; i++) {
      if (!isStopped)
        visualizerData[i] = random(1, 5);
      for (int j = 0; j < visualizerData[i]; j++)
        sprite.fillRect(4 + (i * 9), VISUALIZER_Y + VISUALIZER_HEIGHT - j * 3, 6, 2, CYAN);
    }

    sprite.setTextColor(grays[1], gray);
    sprite.drawString("CMUS Audio", 156, 4);
    sprite.setTextColor(grays[2], gray);
    sprite.drawString("FILES", 58, 0);
    sprite.setTextColor(grays[4], gray);
    sprite.drawString("VOL", 150, 80);
    sprite.drawString("BRI", 150, 122);

    sprite.setTextColor(grays[8], BLACK);
    if (!isStopped) {
      sprite.drawString("PLAY", PLAY_STOP_X, PLAY_STOP_Y);
    } else {
      sprite.drawString("STOP", PLAY_STOP_X, PLAY_STOP_Y);
    }

    // Play Icon/Square and Clock (Below Play/Stop Text)
    if (!isStopped) {
      sprite.fillTriangle(PLAY_ICON_X, TIME_INDICATOR_Y + 8, PLAY_ICON_X, TIME_INDICATOR_Y + 16, PLAY_ICON_X + 8, TIME_INDICATOR_Y + 12, GREEN);
    } else {
      sprite.fillRect(PLAY_ICON_X, TIME_INDICATOR_Y + 8, 8, 8, RED);
    }

    sprite.setFreeFont(&DSEG7_Classic_Mini_Regular_16);
    sprite.setTextColor(WHITE, BLACK);
    if (!isStopped)
      sprite.drawString(rtc.getTime().substring(3, 8), PLAY_ICON_X + 10, TIME_INDICATOR_Y + 2);
    sprite.setTextFont(0);
    int percent = 0;
    if (analogRead(10) > 2390)
      percent = 100;
    else if (analogRead(10) < 1400)
      percent = 1;
    else
      percent = map(analogRead(10), 1400, 2390, 1, 100);

    sprite.setTextDatum(3);
    sprite.drawString(String(percent) + "%", 220, 121);

    sprite.setTextColor(BLACK, grays[4]);
    sprite.drawString("S", 220, 96);
    sprite.drawString("N", 198, 96);
    sprite.drawString("P", 176, 96);
    sprite.drawString("A", 154, 96);

    sprite.setTextColor(BLACK, grays[5]);
    sprite.drawString(">>", 202, 103);
    sprite.drawString("<<", 180, 103);
    spr.fillSprite(BLACK);
    spr.setTextColor(GREEN, BLACK);
    if (!isStopped && fileCount > 0) {
      spr.drawString(getFilenameFromPath(audioFiles[currentTrack], 12), textPos, 4);  //max length 12
      textPos -= 2;
      if (textPos < -300) textPos = 90;
    }
    spr.pushSprite(&sprite, PLAYER_DETAILS_X, 59);
    sprite.pushSprite(0, 0);
    //   if (!isStopped && fileCount > 0) {
    //     if (currentTrack != currentlyPlayingTrack) {
    //         textPos = PLAYER_DETAILS_X + 86;
    //         currentlyPlayingTrack = currentTrack;
    //     }
    //     spr.fillSprite(BLACK);
    //     spr.setTextColor(GREEN, BLACK);
    //     spr.drawString(getFilenameFromPath(audioFiles[currentTrack], 12), textPos, 4);
    //     textPos -= 2;
    //     if (textPos < -300) textPos = PLAYER_DETAILS_X + 86;
    //     spr.pushSprite(&sprite, PLAYER_DETAILS_X, 59);
    // }
    // sprite.pushSprite(0, 0);
  }
  graphSpeed++;
  if (graphSpeed == 4) graphSpeed = 0;
}

void shufflePlaylist() {
  if (fileCount <= 1) return;  // Nothing to shuffle if there's only one or zero files

  for (int i = fileCount - 1; i > 1; i--) {   // Start from the end, excluding the first element
    int j = random(1, i + 1);                 // Generate a random index between 1 and i (inclusive)
    std::swap(audioFiles[i], audioFiles[j]);  // Swap the elements
  }
}

void Task_TFT(void *pvParameters) {
  bool nPressed = false;
  bool pPressed = false;
  bool enterPressed = false;
  bool shPressed = false;
  bool rPressed = false;
  bool plusPressed = false;
  bool minusPressed = false;
  bool aPressed = false;
  bool upPressed = false;
  bool downPressed = false;
  while (1) {
    M5Cardputer.update();
    // Check for key press events
    if (M5Cardputer.Keyboard.isKeyPressed('a') && !aPressed) {
      aPressed = true;
      isPlaying = !isPlaying;
      isStopped = !isStopped;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('a')) {
      aPressed = false;
    }  // Toggle the playback state

    if (M5Cardputer.Keyboard.isKeyPressed('=') && !plusPressed) {
      plusPressed = true;
      volume += VOLUME_STEP;
      if (volume > MAX_VOLUME) {
        volume = MAX_VOLUME;
      }
      audio.setVolume(volume);
    } else if (!M5Cardputer.Keyboard.isKeyPressed('=')) {
      plusPressed = false;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('-') && !minusPressed) {
      minusPressed = true;
      volume -= VOLUME_STEP;
      if (volume < MIN_VOLUME) {
        volume = MIN_VOLUME;
      }
      audio.setVolume(volume);
    } else if (!M5Cardputer.Keyboard.isKeyPressed('-')) {
      minusPressed = false;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('b')) {
      bri++;
      if (bri == 5) bri = 0;
      M5Cardputer.Display.setBrightness(brightness[bri]);
    }

    if (M5Cardputer.Keyboard.isKeyPressed('n') && !nPressed) {
      nPressed = true;
      resetClock();
      isPlaying = false;
      textPos = 90;
      currentTrack++;
      if (currentTrack >= fileCount) currentTrack = 0;
      nextS = 1;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('n')) {
      nPressed = false;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('n')) {
      nPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed('p') && !pPressed) {
      pPressed = true;
      resetClock();
      isPlaying = false;
      textPos = 90;
      currentTrack--;
      if (currentTrack < 0) currentTrack = fileCount - 1;
      nextS = 1;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('p')) {
      pPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(';') && !upPressed) {
      upPressed = true;
      currentTrack--;
      if (currentTrack >= fileCount)
        currentTrack = 0;
    } else if (!M5Cardputer.Keyboard.isKeyPressed(';')) {
      upPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed('.') && !downPressed) {
      downPressed = true;
      currentTrack++;
      if (currentTrack < 0)
        currentTrack = fileCount - 1;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('.')) {
      downPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) && !enterPressed) {
      enterPressed = true;
      resetClock();
      isStopped = false;
      isPlaying = false;
      textPos = 90;
      nextS = 1;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('KEY_ENTER')) {
      enterPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed('r') && !rPressed) {
      rPressed = true;
      resetClock();
      isPlaying = false;
      textPos = 90;
      currentTrack = random(0, fileCount);
      nextS = 1;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('r')) {
      rPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed('s') && !shPressed) {
      shPressed = true;
      shuffleMode = !shuffleMode;
      if (shuffleMode) {
        String currentSong = audioFiles[currentTrack];
        shufflePlaylist();
        for (int i = 0; i < fileCount; i++) {
          if (getFilenameFromPath(audioFiles[i]) == getFilenameFromPath(currentSong)) {
            std::swap(audioFiles[0], audioFiles[i]);
            break;
          }
        }
        currentTrack = 0;
      } else {
        listFiles(SD, MUSIC_DIR);
      }
    } else if (!M5Cardputer.Keyboard.isKeyPressed('s')) {
      shPressed = false;
    }
    draw();
    vTaskDelay(40 / portTICK_PERIOD_MS);  // Adjust the delay for responsiveness
  }
}

void Task_Audio(void *pvParameters) {
  while (1) {

    if (volUp) {
      audio.setVolume(volume);
      isPlaying = 1;
      volUp = 0;
    }

    if (nextS) {
      audio.stopSong();
      audio.connecttoFS(SD, audioFiles[currentTrack].c_str());
      isPlaying = 1;
      nextS = 0;
    }

    if (isPlaying) {
      while (isPlaying) {
        if (!isStopped)
          audio.loop();
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Add a small delay to prevent task hogging
      }
    } else {
      isPlaying = true;
    }
  }
}

void listFiles(fs::FS &fs, const char *dirname) {
  Serial.print("Attempting to open directory: ");
  Serial.println(dirname);
  File root = fs.open(dirname);
  if (!root) {
    Serial.print("Failed to open directory: ");
    Serial.println(dirname);
    return;
  }
  if (!root.isDirectory()) {
    Serial.print(dirname);
    Serial.println(" is not a directory!");
    root.close();
    return;
  }

  fileCount = 0;  // Reset file count before listing
  File file = root.openNextFile();
  while (file && fileCount < MAX_FILES) {
    if (!file.isDirectory()) {
      String filename = String(file.name());
      if (isSupportedFile(filename)) {  // Check if the file is supported
        audioFiles[fileCount] = String(dirname) + "/" + filename;
        Serial.print("Found File: ");
        Serial.println(audioFiles[fileCount]);
        fileCount++;
      }
    }
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("Found %d audio files\currentTrack", fileCount);
}

void audio_eof_mp3(const char *info) {
  Serial.print("eof_mp3: ");
  Serial.println(info);
  if (fileCount > 0) {  // Check if there are any files
    currentTrack++;
    resetClock();
    if (currentTrack >= fileCount) currentTrack = 0;
    audio.connecttoFS(SD, audioFiles[currentTrack].c_str());
    isPlaying = true;
    textPos = PLAYER_DETAILS_X + 86;
    currentlyPlayingTrack = currentTrack;
  } else {
    isPlaying = false;  // Stop if no more files
    isStopped = true;
    Serial.println("No more files to play.");
  }
}