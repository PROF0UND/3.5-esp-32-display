/*
  TXT-only Touch File Explorer
  ESP32 + ILI9488 (TFT_eSPI) + XPT2046 touch
  - Lists folders and .txt files (root and subfolders)
  - Tap folder to enter, tap .txt to view
  - Back button to go up
  - Page Up/Down for list and for text viewer
*/

#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

// ====== USER SETTINGS ======
#define SD_CS 5 // <-- set your SD card CS pin here
// Paste your calibration from Touch_calibrate:
uint16_t calData[5] = { 300, 3800, 320, 3650, 1 }; // <-- REPLACE WITH YOUR VALUES
// ===========================

TFT_eSPI tft = TFT_eSPI();

// Screen layout (rotation = 1 -> 480x320)
const int SCREEN_W = 480;
const int SCREEN_H = 320;
const int TOPBAR_H = 28;
const int ROW_H    = 24;
const int MARGIN_X = 8;

// Tap zones
const int BACK_X1 = 0, BACK_Y1 = 0, BACK_X2 = 80, BACK_Y2 = TOPBAR_H;   // "< Back"
const int PG_W = 64;
const int PG_X1 = SCREEN_W - PG_W;
const int PGUP_Y1 = 0,              PGUP_Y2 = TOPBAR_H;                  // "Pg^"
const int PGDN_Y1 = SCREEN_H - TOPBAR_H, PGDN_Y2 = SCREEN_H;             // "Pgv"

struct Entry {
  String name;
  bool isDir;
  uint32_t size;
};
const int MAX_ENTRIES = 128;
Entry entries[MAX_ENTRIES];
int entryCount = 0;
int page = 0;

String currentPath = "/";

enum Mode { MODE_LIST, MODE_VIEW };
Mode mode = MODE_LIST;

// Text viewer state
File currentFile;
String currentFilePath;
std::vector<uint32_t> pageOffsets; // file positions at the start of each page
int currentPageIndex = 0;
bool hasNextPage = false, hasPrevPage = false;

// ---------- utils ----------
bool endsWithIgnoreCase(const String& s, const char* suf) {
  String a = s; a.toLowerCase();
  String b = String(suf); b.toLowerCase();
  return a.endsWith(b);
}
bool isTxt(const String& name) {
  return endsWithIgnoreCase(name, ".txt");
}
String joinPath(const String& base, const String& name) {
  if (base == "/") return "/" + name;
  return base + "/" + name;
}

void waitRelease() {
  uint16_t x, y;
  while (tft.getTouch(&x, &y)) delay(10);
}
bool getTouch(uint16_t &x, uint16_t &y) {
  static uint32_t last = 0;
  if (millis() - last < 20) return false;
  bool pressed = tft.getTouch(&x, &y);
  if (pressed) last = millis();
  return pressed;
}

// ---------- UI ----------
void drawTopBar(const String& title, bool showBack) {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, TFT_DARKGREY);
  tft.drawFastHLine(0, TOPBAR_H-1, SCREEN_W, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  if (showBack) { tft.setCursor(6, 5); tft.print("< Back"); }

  // Truncate long titles
  String t = title;
  if (t.length() > 36) t = t.substring(0, 33) + "...";
  int cx = (SCREEN_W/2) - (t.length()*6);
  if (cx < 90) cx = 90;
  tft.setCursor(cx, 5);
  tft.print(t);
}
void drawButtons() {
  // Page Up
  tft.fillRect(PG_X1, 0, PG_W, TOPBAR_H, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(2);
  tft.setCursor(PG_X1 + 10, 5); tft.print("Pg^");
  // Page Down
  tft.fillRect(PG_X1, SCREEN_H - TOPBAR_H, PG_W, TOPBAR_H, TFT_NAVY);
  tft.setCursor(PG_X1 + 10, SCREEN_H - TOPBAR_H + 5); tft.print("Pgv");
}

// ---------- Directory listing ----------
void listDir(const String& path) {
  entryCount = 0;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;

  File f = dir.openNextFile();
  while (f && entryCount < MAX_ENTRIES) {
    bool dirFlag = f.isDirectory();
    String nm = f.name();
    // Filter: only dirs and .txt
    if (dirFlag || isTxt(nm)) {
      entries[entryCount].name = nm;
      entries[entryCount].isDir = dirFlag;
      entries[entryCount].size  = dirFlag ? 0 : f.size();
      entryCount++;
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
}

void drawList() {
  mode = MODE_LIST;
  tft.fillScreen(TFT_BLACK);
  drawTopBar(currentPath, currentPath != "/");
  drawButtons();

  tft.setTextSize(2);
  int rowsPerPage = (SCREEN_H - TOPBAR_H) / ROW_H;
  int start = page * rowsPerPage;
  int end = min(entryCount, start + rowsPerPage);

  for (int i = start, r = 0; i < end; ++i, ++r) {
    int y = TOPBAR_H + r * ROW_H;
    uint16_t bg = (r % 2 == 0) ? TFT_BLACK : (uint16_t)0x0841;
    tft.fillRect(0, y, SCREEN_W - PG_W, ROW_H, bg);
    tft.setCursor(MARGIN_X, y + 4);
    if (entries[i].isDir) {
      tft.setTextColor(TFT_YELLOW, bg);
      tft.print("[DIR] ");
      tft.print(entries[i].name);
    } else {
      tft.setTextColor(TFT_WHITE, bg);
      tft.print(entries[i].name);
      tft.setTextColor(TFT_CYAN, bg);
      tft.setCursor(SCREEN_W - PG_W - 140, y + 4);
      tft.print(entries[i].size); tft.print(" B");
    }
  }

  // Page indicator
  int maxPage = max(0, (entryCount - 1) / ((SCREEN_H - TOPBAR_H)/ROW_H));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(SCREEN_W - 110, (SCREEN_H/2) - 8);
  tft.printf("p %d/%d", page+1, max(1, maxPage+1));
}

// ---------- Text viewer ----------
void drawTextPageFromOffset(uint32_t offset) {
  mode = MODE_VIEW;
  hasPrevPage = (currentPageIndex > 0);
  hasNextPage = false;

  tft.fillScreen(TFT_BLACK);
  drawTopBar(currentFilePath, true);
  drawButtons();

  if (!currentFile) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(MARGIN_X, TOPBAR_H + 8);
    tft.println("Could not open file.");
    return;
  }

  currentFile.seek(offset);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  int x = MARGIN_X, y = TOPBAR_H + 6;
  const int rightMargin = SCREEN_W - PG_W - 8;
  const int bottom = SCREEN_H - 18; // leave a hint line

  while (currentFile.available()) {
    char c = currentFile.read();
    if (c == '\r') continue;

    if (c == '\n') {
      y += 18; x = MARGIN_X;
      if (y > bottom) { hasNextPage = true; break; }
      continue;
    }

    // wrap
    if (x > rightMargin - 10) {
      y += 18; x = MARGIN_X;
      if (y > bottom) { hasNextPage = true; break; }
    }

    tft.setCursor(x, y);
    tft.write(c);
    x += 12; // approx char width @ size 2
  }

  // record next page start if any
  if (hasNextPage) {
    uint32_t nextPos = currentFile.position();
    if (currentPageIndex + 1 == (int)pageOffsets.size()) {
      pageOffsets.push_back(nextPos);
    }
  }

  // Hint line
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(MARGIN_X, SCREEN_H - 18);
  tft.print("Pg^/Pgv scroll, < Back to return");
}

// ---------- Open handlers ----------
void openEntry(int idx) {
  const Entry& e = entries[idx];
  if (e.isDir) {
    currentPath = joinPath(currentPath, e.name);
    page = 0;
    listDir(currentPath);
    drawList();
    return;
  }

  // .txt file
  currentFilePath = joinPath(currentPath, e.name);
  currentFile = SD.open(currentFilePath, FILE_READ);
  pageOffsets.clear();
  pageOffsets.push_back(0);
  currentPageIndex = 0;
  drawTextPageFromOffset(0);
}

// ---------- setup/loop ----------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.setTouch(calData);

  tft.fillScreen(TFT_BLACK);
  drawTopBar("Mounting SD...", false);

  if (!SD.begin(SD_CS)) {
    tft.fillScreen(TFT_BLACK);
    drawTopBar("SD init failed", false);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(MARGIN_X, TOPBAR_H + 8);
    tft.println("Card Mount Failed");
    while (true) delay(100);
  }

  listDir(currentPath);
  drawList();
}

void loop() {
  uint16_t x, y;
  if (!getTouch(x, y)) return;

  // Back
  if (x >= BACK_X1 && x < BACK_X2 && y >= BACK_Y1 && y < BACK_Y2) {
    if (mode == MODE_VIEW) {
      if (currentFile) currentFile.close();
      mode = MODE_LIST;
      drawList();
    } else if (mode == MODE_LIST && currentPath != "/") {
      int pos = currentPath.lastIndexOf('/');
      currentPath = (pos <= 0) ? "/" : currentPath.substring(0, pos);
      page = 0;
      listDir(currentPath);
      drawList();
    }
    waitRelease();
    return;
  }

  // Page Up / Down
  if (x >= PG_X1 && y >= PGUP_Y1 && y < PGUP_Y2) {
    // Page Up
    if (mode == MODE_LIST) {
      if (page > 0) { page--; drawList(); }
    } else {
      if (currentPageIndex > 0) {
        currentPageIndex--;
        drawTextPageFromOffset(pageOffsets[currentPageIndex]);
      }
    }
    waitRelease();
    return;
  }
  if (x >= PG_X1 && y >= PGDN_Y1 && y < PGDN_Y2) {
    // Page Down
    if (mode == MODE_LIST) {
      int rowsPerPage = (SCREEN_H - TOPBAR_H) / ROW_H;
      int maxPage = max(0, (entryCount - 1) / rowsPerPage);
      if (page < maxPage) { page++; drawList(); }
    } else {
      if (hasNextPage) {
        currentPageIndex++;
        if (currentPageIndex >= (int)pageOffsets.size()) currentPageIndex = pageOffsets.size() - 1;
        drawTextPageFromOffset(pageOffsets[currentPageIndex]);
      }
    }
    waitRelease();
    return;
  }

  // List tap → open entry
  if (mode == MODE_LIST && y >= TOPBAR_H && y < SCREEN_H) {
    int rowsPerPage = (SCREEN_H - TOPBAR_H) / ROW_H;
    int row = (y - TOPBAR_H) / ROW_H;
    int idx = page * rowsPerPage + row;
    if (idx >= 0 && idx < entryCount) {
      openEntry(idx);
      waitRelease();
      return;
    }
  }

  waitRelease();
}
