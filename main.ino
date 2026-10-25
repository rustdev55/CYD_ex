#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <vector>
#include <math.h>
#if __has_include("esp_bt.h")
#include "esp_bt.h"
#endif

// ======== EDIT THESE ========
// WIFI_SSID / WIFI_PASS are only used as the very first saved network.
// After that, WiFi is managed from  Gemini > SET > WiFi  and stored in flash.
const char* WIFI_SSID    = "WIFI_SSID";
const char* WIFI_PASS    = "WIFI_PASS";
const char* BOT_TOKEN    = "TOKEN";
const char* GEMINI_MODEL = "gemini-3-flash-preview";
#define REPLY_SCALE 1
#define MAX_DRAFT   300
#define SECRET_COMBO "1*678"
// ============================

#define PIN_SCLK 14
#define PIN_MISO 12
#define PIN_MOSI 13
#define PIN_CS   15
#define PIN_DC    2
#define T_CS     33
#define BL_PIN_A 21
#define BL_PIN_B 27
#define BL_ON    HIGH
#define Z_THRESHOLD 150

#define SCR_W 320
#define SCR_H 240
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_GREY  0xBDF7
#define C_GREEN 0x07E0
#define C_BTN   0x2945
#define C_OP    0x0012
#define C_FUNC  0x3186
#define C_RED   0x9000
#define C_EQ    0x0480
#define C_ORNG  0xFD20
#define C_CYAN  0x07FF

#define DEF_PEN     2
#define DEF_ERASER  6

// ---- types (must stay above all functions: Arduino auto-prototypes need them) ----
struct KeyDef { int x, y, w, h; char lab[6]; };

struct Stroke {
  uint8_t kind;                  // 0 = pen, 1 = eraser, 2 = clear
  std::vector<uint8_t> pts;      // x,y pairs in canvas coords
};

struct APInfo {
  String ssid;
  int rssi;
  bool secure;
  bool manual;                   // the "type a hidden name" entry
};

struct NetCred {
  String ssid;
  String pass;
};

SPIClass spi(HSPI);
static const SPISettings dispSet(20000000, MSBFIRST, SPI_MODE0);
static const SPISettings touchSet(1000000, MSBFIRST, SPI_MODE0);
Preferences prefs;

const uint8_t font5x7[] PROGMEM = {
0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x56,0x20,0x50, 0x00,0x08,0x07,0x03,0x00,
0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x2A,0x1C,0x7F,0x1C,0x2A, 0x08,0x08,0x3E,0x08,0x08,
0x00,0x80,0x70,0x30,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x00,0x60,0x60,0x00, 0x20,0x10,0x08,0x04,0x02,
0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x72,0x49,0x49,0x49,0x46, 0x21,0x41,0x49,0x4D,0x33,
0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x31, 0x41,0x21,0x11,0x09,0x07,
0x36,0x49,0x49,0x49,0x36, 0x46,0x49,0x49,0x29,0x1E, 0x00,0x00,0x14,0x00,0x00, 0x00,0x40,0x34,0x00,0x00,
0x00,0x08,0x14,0x22,0x41, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x59,0x09,0x06,
0x3E,0x41,0x5D,0x59,0x4E, 0x7C,0x12,0x11,0x12,0x7C, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x41,0x3E, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x41,0x51,0x73,
0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x1C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x26,0x49,0x49,0x49,0x32,
0x03,0x01,0x7F,0x01,0x03, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,0x08,0x14,0x63, 0x03,0x04,0x78,0x04,0x03, 0x61,0x59,0x49,0x4D,0x43, 0x00,0x7F,0x41,0x41,0x41,
0x02,0x04,0x08,0x10,0x20, 0x00,0x41,0x41,0x41,0x7F, 0x04,0x02,0x01,0x02,0x04, 0x40,0x40,0x40,0x40,0x40,
0x00,0x03,0x07,0x08,0x00, 0x20,0x54,0x54,0x78,0x40, 0x7F,0x28,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x28,
0x38,0x44,0x44,0x28,0x7F, 0x38,0x54,0x54,0x54,0x18, 0x00,0x08,0x7E,0x09,0x02, 0x18,0xA4,0xA4,0x9C,0x78,
0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x40,0x3D,0x00, 0x7F,0x10,0x28,0x44,0x00,
0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x78,0x04,0x78, 0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38,
0xFC,0x18,0x24,0x24,0x18, 0x18,0x24,0x24,0x18,0xFC, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x24,
0x04,0x04,0x3F,0x44,0x24, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C, 0x3C,0x40,0x30,0x40,0x3C,
0x44,0x28,0x10,0x28,0x44, 0x4C,0x90,0x90,0x90,0x7C, 0x44,0x64,0x54,0x4C,0x44, 0x00,0x08,0x36,0x41,0x00,
0x00,0x00,0x77,0x00,0x00, 0x00,0x41,0x36,0x08,0x00, 0x02,0x01,0x02,0x04,0x02
};

// ---------- Display ----------
void cmdd(uint8_t c, std::initializer_list<uint8_t> d) {
  spi.beginTransaction(dispSet);
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, LOW);
  spi.transfer(c);
  digitalWrite(PIN_DC, HIGH);
  for (auto b : d) spi.transfer(b);
  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();
}
void cmd(uint8_t c) { cmdd(c, {}); }

void setWindow(int x0, int y0, int x1, int y1) {
  cmdd(0x2A, {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1});
  cmdd(0x2B, {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1});
  cmd(0x2C);
}

void fillRect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > SCR_W || y + h > SCR_H) return;
  setWindow(x, y, x + w - 1, y + h - 1);
  static uint8_t buf[512];
  for (int i = 0; i < 512; i += 2) { buf[i] = color >> 8; buf[i + 1] = color & 0xFF; }
  spi.beginTransaction(dispSet);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_CS, LOW);
  uint32_t n = (uint32_t)w * h * 2;
  while (n) { uint32_t k = n > 512 ? 512 : n; spi.writeBytes(buf, k); n -= k; }
  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();
}

void drawRectOutline(int x, int y, int w, int h, uint16_t c) {
  fillRect(x, y, w, 1, c);
  fillRect(x, y + h - 1, w, 1, c);
  fillRect(x, y, 1, h, c);
  fillRect(x + w - 1, y, 1, h, c);
}

void initPanel() {
  cmd(0x01); delay(150);
  cmd(0x11); delay(150);
  cmdd(0x36, {0x60});
  cmdd(0x3A, {0x55});
  cmdd(0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33});
  cmdd(0xB7, {0x35});
  cmdd(0xBB, {0x19});
  cmdd(0xC0, {0x2C});
  cmdd(0xC2, {0x01});
  cmdd(0xC3, {0x12});
  cmdd(0xC4, {0x20});
  cmdd(0xC6, {0x0F});
  cmdd(0xD0, {0xA4, 0xA1});
  cmd(0x20);
  cmd(0x13);
  cmd(0x29); delay(50);
}

void drawChar(int x, int y, char c, int s, uint16_t fg, uint16_t bg) {
  if (c < 32 || c > 126) c = '?';
  const uint8_t* g = &font5x7[(c - 32) * 5];
  int w = 6 * s, h = 8 * s;
  static uint8_t buf[6 * 3 * 8 * 3 * 2];
  int i = 0;
  for (int yy = 0; yy < h; yy++) {
    int row = yy / s;
    for (int xx = 0; xx < w; xx++) {
      int col = xx / s;
      bool on = (col < 5) && (row < 7) && ((pgm_read_byte(&g[col]) >> row) & 1);
      uint16_t cc = on ? fg : bg;
      buf[i++] = cc >> 8;
      buf[i++] = cc & 0xFF;
    }
  }
  setWindow(x, y, x + w - 1, y + h - 1);
  spi.beginTransaction(dispSet);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_CS, LOW);
  spi.writeBytes(buf, i);
  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();
}

int textWidth(const String& t, int s) { return t.length() * 6 * s; }

void drawText(int x, int y, const String& t, int s, uint16_t fg, uint16_t bg) {
  for (unsigned i = 0; i < t.length(); i++) {
    if (x + (int)(i + 1) * 6 * s > SCR_W) break;
    drawChar(x + i * 6 * s, y, t[i], s, fg, bg);
  }
}

// generic centered-label button
void drawBtn(int x, int y, int w, int h, const String& label, uint16_t col, int sc) {
  fillRect(x, y, w, h, col);
  drawText(x + (w - textWidth(label, sc)) / 2, y + (h - 8 * sc) / 2, label, sc, C_WHITE, col);
}

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------- Touch ----------
int lastZ = 0;

uint16_t xr(uint8_t command) {
  uint8_t b[3] = {command, 0, 0};
  spi.beginTransaction(touchSet);
  digitalWrite(T_CS, LOW);
  spi.transferBytes(b, b, 3);
  digitalWrite(T_CS, HIGH);
  spi.endTransaction();
  return (((b[1] << 8) | b[2]) >> 3) & 0x0FFF;
}

bool getRaw(int& rx, int& ry) {
  int z1 = xr(0xB1);
  int z2 = xr(0xC1);
  int z = z1 + 4095 - z2;
  lastZ = z;
  if (z < Z_THRESHOLD || z > 3900) { xr(0x90); return false; }
  xr(0xD1); xr(0x91);
  long sx = 0, sy = 0;
  const int N = 5;
  for (int i = 0; i < N; i++) { sx += xr(0xD1); sy += xr(0x91); }
  xr(0x90);
  rx = sx / N; ry = sy / N;
  return (rx > 100 && ry > 100 && rx < 4000 && ry < 4000);
}

void waitRelease() {
  int a, b, misses = 0;
  while (misses < 4) {
    if (getRaw(a, b)) misses = 0; else misses++;
    delay(15);
  }
}

bool swapXY = false;
float ax1, ax2, ay1, ay2;
const int CAL_X1 = 30, CAL_Y1 = 30, CAL_X2 = 290, CAL_Y2 = 210;

void drawCross(int x, int y) {
  fillRect(x - 12, y - 1, 25, 3, C_WHITE);
  fillRect(x - 1, y - 12, 3, 25, C_WHITE);
}

void waitTap(int& rx, int& ry) {
  while (!getRaw(rx, ry)) delay(10);
  Serial.printf("tap raw x=%d y=%d z=%d\n", rx, ry, lastZ);
  waitRelease();
}

// calibration is stored in flash so it survives reboots
void saveCalibration() {
  prefs.putBool("cal", true);
  prefs.putBool("cswap", swapXY);
  prefs.putFloat("cax1", ax1);
  prefs.putFloat("cax2", ax2);
  prefs.putFloat("cay1", ay1);
  prefs.putFloat("cay2", ay2);
}

bool loadCalibration() {
  if (!prefs.getBool("cal", false)) return false;
  swapXY = prefs.getBool("cswap", false);
  ax1 = prefs.getFloat("cax1", 0);
  ax2 = prefs.getFloat("cax2", 0);
  ay1 = prefs.getFloat("cay1", 0);
  ay2 = prefs.getFloat("cay2", 0);
  if (ax1 == ax2 || ay1 == ay2) return false;
  return true;
}

void calibrate() {
  int r1x, r1y, r2x, r2y;
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawText(40, 100, "Tap the crosshair", 2, C_WHITE, C_BLACK);
  drawCross(CAL_X1, CAL_Y1);
  waitTap(r1x, r1y);
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawText(40, 100, "Tap the crosshair", 2, C_WHITE, C_BLACK);
  drawCross(CAL_X2, CAL_Y2);
  waitTap(r2x, r2y);
  swapXY = abs(r2x - r1x) < abs(r2y - r1y);
  if (!swapXY) { ax1 = r1x; ax2 = r2x; ay1 = r1y; ay2 = r2y; }
  else         { ax1 = r1y; ax2 = r2y; ay1 = r1x; ay2 = r2x; }
  if (ax1 == ax2) ax2 = ax1 + 1;
  if (ay1 == ay2) ay2 = ay1 + 1;
  Serial.printf("Cal: swap=%d ax %.0f..%.0f ay %.0f..%.0f\n", swapXY, ax1, ax2, ay1, ay2);
  saveCalibration();
}

bool getTouch(int& x, int& y) {
  int rx, ry;
  if (!getRaw(rx, ry)) return false;
  float a = swapXY ? ry : rx;
  float b = swapXY ? rx : ry;
  x = CAL_X1 + (a - ax1) * (CAL_X2 - CAL_X1) / (ax2 - ax1);
  y = CAL_Y1 + (b - ay1) * (CAL_Y2 - CAL_Y1) / (ay2 - ay1);
  x = constrain(x, 0, SCR_W - 1);
  y = constrain(y, 0, SCR_H - 1);
  return true;
}

// ---------- Emergency EXIT ----------
const int EX_W = 41, EX_H = 24;

void drawExitBtn() {
  fillRect(0, 0, EX_W, EX_H, C_RED);
  drawText((EX_W - textWidth("EXIT", 1)) / 2, (EX_H - 8) / 2, "EXIT", 1, C_WHITE, C_RED);
}

bool inExitZone(int x, int y) { return x < EX_W + 7 && y < EX_H + 8; }

// ---------- App state ----------
enum App  { APP_CALC, APP_GEM };
enum GScr { G_MAIN, G_KB, G_DRAW, G_SET, G_WIFI, G_NETS };
App  app = APP_CALC;
GScr gscr = G_MAIN;
bool cancelled = false;
bool justOpenedGemini = false;
bool kbFromDraw = false;      // keyboard was opened from the draw screen (TXT)

bool pollExit() {
  int x, y;
  if (getTouch(x, y) && inExitZone(x, y)) { waitRelease(); return true; }
  return false;
}

// ---------- Settings / WiFi state (persisted in flash) ----------
#define EM_DRAFT 0
#define EM_SSID  1
#define EM_PASS  2
#define MAX_NETS 6
#define WIFI_IDLE_MS 45000UL

int setBright   = 70;                 // 10..100 %
int sleepIdx    = 1;                  // index into SLEEP_OPTS
int penSize     = DEF_PEN;            // canvas px
int eraserSize  = DEF_ERASER;         // canvas px
int replyScale  = REPLY_SCALE;        // 1 small / 2 large
int ansLenIdx   = 1;                  // index into ANS_TOK
int cpuIdx      = 0;                  // index into CPU_MHZ (0 = 80 MHz eco)
int wifiMode    = 1;                  // 0 = always on, 1 = on demand (saves power)

const int SLEEP_OPTS[6] = {15, 30, 60, 120, 300, 0};   // seconds, 0 = never
const int ANS_TOK[3]    = {300, 800, 1500};
const int CPU_MHZ[3]    = {80, 160, 240};

std::vector<NetCred> nets;            // saved networks, index 0 = most recently used
std::vector<APInfo>  aps;             // last scan result
int apTop = 0;
String selSsid = "";
String entryBuf = "";
int entryMode = EM_DRAFT;
int kbLayer = 0;
int setPage = 0;

bool screenOn = true;
unsigned long lastAct = 0;
unsigned long lastWifiUse = 0;

const int AP_Y0 = 32, AP_RH = 29, AP_VIS = 6;
const int SET_Y0 = 30, SET_RH = 30;

void loadSettings() {
  setBright  = clampi(prefs.getUChar("bri", 70), 10, 100);
  sleepIdx   = clampi(prefs.getUChar("slp", 1), 0, 5);
  penSize    = clampi(prefs.getUChar("pen", DEF_PEN), 1, 5);
  eraserSize = clampi(prefs.getUChar("era", DEF_ERASER), 3, 14);
  replyScale = clampi(prefs.getUChar("rsc", REPLY_SCALE), 1, 2);
  ansLenIdx  = clampi(prefs.getUChar("alen", 1), 0, 2);
  cpuIdx     = clampi(prefs.getUChar("cpu", 0), 0, 2);
  wifiMode   = clampi(prefs.getUChar("wm", 1), 0, 1);
}

void saveSettings() {
  prefs.putUChar("bri", setBright);
  prefs.putUChar("slp", sleepIdx);
  prefs.putUChar("pen", penSize);
  prefs.putUChar("era", eraserSize);
  prefs.putUChar("rsc", replyScale);
  prefs.putUChar("alen", ansLenIdx);
  prefs.putUChar("cpu", cpuIdx);
  prefs.putUChar("wm", wifiMode);
}

// ---------- Backlight (PWM) + screen sleep ----------
void backlightInit() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(BL_PIN_A, 5000, 8);
  ledcAttach(BL_PIN_B, 5000, 8);
#else
  ledcSetup(0, 5000, 8); ledcAttachPin(BL_PIN_A, 0);
  ledcSetup(1, 5000, 8); ledcAttachPin(BL_PIN_B, 1);
#endif
}

void blWrite(int duty) {
  if (BL_ON == LOW) duty = 255 - duty;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(BL_PIN_A, duty);
  ledcWrite(BL_PIN_B, duty);
#else
  ledcWrite(0, duty);
  ledcWrite(1, duty);
#endif
}

void setBacklight(int pct) {
  pct = clampi(pct, 0, 100);
  if (pct == 0) { blWrite(0); return; }
  int duty = 6 + (249L * pct * pct) / 10000;      // gamma-ish curve, 10% is still readable
  blWrite(clampi(duty, 0, 255));
}

void wakeScreen()  { screenOn = true;  setBacklight(setBright); lastAct = millis(); }
void sleepScreen() { screenOn = false; setBacklight(0); }

// ---------- Power saving ----------
void powerSaveInit() {
  btStop();                                       // Bluetooth is never used
#if __has_include("esp_bt.h")
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);  // also frees ~30 KB RAM
#endif
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ---------- Calculator ----------
const int gridCols = 4;
const int gridRows = 7;
const int topH = 44;
const int btnW = SCR_W / gridCols;
const int btnH = (SCR_H - topH) / gridRows;

const char* labels[gridRows][gridCols] = {
  {"C",   "del", "(",   ")"},
  {"sin", "cos", "tan", "^"},
  {"log", "ln",  "sqrt","/"},
  {"7",   "8",   "9",   "*"},
  {"4",   "5",   "6",   "-"},
  {"1",   "2",   "3",   "+"},
  {"0",   ".",   "pi",  "="},
};

String currentInput = "";
String resultText = "";

const char* p;
bool perr;
double parseExpr();

double parsePrimary() {
  if ((*p >= '0' && *p <= '9') || *p == '.') {
    char* end;
    double v = strtod(p, &end);
    p = end;
    return v;
  }
  if (*p == '(') {
    p++;
    double v = parseExpr();
    if (*p == ')') p++;
    return v;
  }
  if (*p >= 'a' && *p <= 'z') {
    char name[8];
    int n = 0;
    while (*p >= 'a' && *p <= 'z' && n < 7) name[n++] = *p++;
    name[n] = 0;
    if (strcmp(name, "pi") == 0) return M_PI;
    if (*p != '(') { perr = true; return 0; }
    p++;
    double a = parseExpr();
    if (*p == ')') p++;
    if (!strcmp(name, "sin"))  return sin(a * M_PI / 180.0);
    if (!strcmp(name, "cos"))  return cos(a * M_PI / 180.0);
    if (!strcmp(name, "tan"))  return tan(a * M_PI / 180.0);
    if (!strcmp(name, "log"))  return log10(a);
    if (!strcmp(name, "ln"))   return log(a);
    if (!strcmp(name, "sqrt")) return sqrt(a);
    perr = true;
    return 0;
  }
  perr = true;
  return 0;
}

double parseUnary();

double parsePower() {
  double b = parsePrimary();
  if (*p == '^') { p++; double e = parseUnary(); return pow(b, e); }
  return b;
}

double parseUnary() {
  if (*p == '-') { p++; return -parseUnary(); }
  if (*p == '+') { p++; return parseUnary(); }
  return parsePower();
}

double parseTerm() {
  double v = parseUnary();
  while (*p == '*' || *p == '/') {
    char op = *p++;
    double r = parseUnary();
    v = (op == '*') ? v * r : v / r;
  }
  return v;
}

double parseExpr() {
  double v = parseTerm();
  while (*p == '+' || *p == '-') {
    char op = *p++;
    double r = parseTerm();
    v = (op == '+') ? v + r : v - r;
  }
  return v;
}

String formatNum(double v) {
  if (isnan(v) || isinf(v)) return "Error";
  String s = String(v, 6);
  if (s.indexOf('.') >= 0) {
    while (s.endsWith("0")) s.remove(s.length() - 1);
    if (s.endsWith(".")) s.remove(s.length() - 1);
  }
  if (s == "-0") s = "0";
  return s;
}

String evaluate(const String& expr) {
  if (expr.length() == 0) return "0";
  p = expr.c_str();
  perr = false;
  double v = parseExpr();
  if (perr || *p != 0) return "Error";
  return formatNum(v);
}

void drawDisplay() {
  fillRect(0, 0, SCR_W, topH, C_BLACK);
  drawRectOutline(3, 3, SCR_W - 6, topH - 6, C_WHITE);
  String in = currentInput;
  if (in.length() > 24) in = in.substring(in.length() - 24);
  drawText(SCR_W - 8 - textWidth(in, 2), 6, in, 2, C_GREY, C_BLACK);
  String r = resultText.length() ? resultText : "0";
  if (r.length() > 24) r = r.substring(0, 24);
  drawText(SCR_W - 8 - textWidth(r, 2), 24, r, 2, C_GREEN, C_BLACK);
}

void drawButtons() {
  for (int r = 0; r < gridRows; r++) {
    for (int c = 0; c < gridCols; c++) {
      int x = c * btnW, y = topH + r * btnH;
      String l = labels[r][c];
      uint16_t col = C_BTN;
      if (l == "=") col = C_EQ;
      else if (l == "C" || l == "del") col = C_RED;
      else if (l == "sin" || l == "cos" || l == "tan" || l == "log" || l == "ln" || l == "sqrt" || l == "pi") col = C_FUNC;
      else if (l == "+" || l == "-" || l == "*" || l == "/" || l == "^" || l == "(" || l == ")") col = C_OP;
      fillRect(x + 2, y + 2, btnW - 4, btnH - 4, col);
      int tx = x + (btnW - textWidth(l, 2)) / 2;
      int ty = y + (btnH - 16) / 2;
      drawText(tx, ty, l, 2, C_WHITE, col);
    }
  }
}

void saveAll();

void handleInput(const String& btn) {
  if (btn == "C") {
    currentInput = "";
    resultText = "";
  } else if (btn == "=") {
    resultText = evaluate(currentInput);
    saveAll();
  } else if (btn == "del") {
    if (currentInput.endsWith("sin(") || currentInput.endsWith("cos(") ||
        currentInput.endsWith("tan(") || currentInput.endsWith("log(")) {
      currentInput.remove(currentInput.length() - 4);
    } else if (currentInput.endsWith("sqrt(")) {
      currentInput.remove(currentInput.length() - 5);
    } else if (currentInput.endsWith("ln(")) {
      currentInput.remove(currentInput.length() - 3);
    } else if (currentInput.length() > 0) {
      currentInput.remove(currentInput.length() - 1);
    }
  } else if (btn == "sin" || btn == "cos" || btn == "tan" ||
             btn == "log" || btn == "ln"  || btn == "sqrt") {
    currentInput += btn + "(";
  } else {
    currentInput += btn;
  }

  if (currentInput == SECRET_COMBO || currentInput.endsWith(SECRET_COMBO)) {
    currentInput = "";
    resultText = "";
    saveAll();
    justOpenedGemini = true;
    return;
  }

  drawDisplay();
}

void drawCalc() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawDisplay();
  drawButtons();
  drawExitBtn();
}

// ---------- Text helpers ----------
String decodeJson(const String& s, int i) {
  String out;
  int n = s.length();
  while (i < n) {
    char c = s[i++];
    if (c == '"') break;
    if (c == '\\' && i < n) {
      char e = s[i++];
      switch (e) {
        case 'n': out += '\n'; break;
        case 't': out += ' '; break;
        case 'r': break;
        case 'u': {
          if (i + 4 > n) return out;
          uint32_t cp = strtoul(s.substring(i, i + 4).c_str(), NULL, 16);
          i += 4;
          if (cp >= 0xD800 && cp <= 0xDBFF) { i += 6; out += '?'; }
          else if (cp == 0x2018 || cp == 0x2019) out += '\'';
          else if (cp == 0x201C || cp == 0x201D) out += '"';
          else if (cp == 0x2013 || cp == 0x2014) out += '-';
          else if (cp == 0x2022) out += '*';
          else if (cp == 0xA0) out += ' ';
          else if (cp < 128) out += (char)cp;
          else out += '?';
          break;
        }
        default: out += e;
      }
    } else out += c;
  }
  return out;
}

String jsonStringAfter(const String& s, const char* key) {
  String k = String("\"") + key + "\"";
  int idx = s.indexOf(k);
  if (idx < 0) return "";
  int j = s.indexOf(':', idx + k.length());
  if (j < 0) return "";
  j++;
  while (j < (int)s.length() && (s[j] == ' ' || s[j] == '\n' || s[j] == '\r')) j++;
  if (j >= (int)s.length() || s[j] != '"') return "";
  return decodeJson(s, j + 1);
}

String toAscii(const String& s) {
  String o;
  int n = s.length();
  for (int i = 0; i < n;) {
    uint8_t c = s[i];
    if (c < 0x80) { if (c >= 32 || c == '\n') o += (char)c; i++; continue; }
    int len = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2);
    char r = '?';
    if (c == 0xC2 && i + 1 < n && (uint8_t)s[i + 1] == 0xA0) r = ' ';
    if (c == 0xE2 && i + 2 < n && (uint8_t)s[i + 1] == 0x80) {
      uint8_t t = s[i + 2];
      if (t == 0x98 || t == 0x99) r = '\'';
      else if (t == 0x9C || t == 0x9D) r = '"';
      else if (t == 0x93 || t == 0x94) r = '-';
      else if (t == 0xA2) r = '*';
      else if (t == 0xA6) r = '.';
    }
    o += r;
    i += len;
  }
  return o;
}

String jsonEscape(const String& s) {
  String o;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if (c >= 32) o += c;
  }
  return o;
}

// ---------- PNG encoder (grayscale 8-bit, zlib "stored" blocks, no libraries) ----------
static const uint32_t crcTable[16] = {
  0,0x1DB71064,0x3B6E20C8,0x26D930AC,0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
  0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C
};
uint32_t crcUpdate(uint32_t crc, uint8_t b) {
  crc ^= b;
  crc = crcTable[crc & 0xF] ^ (crc >> 4);
  crc = crcTable[crc & 0xF] ^ (crc >> 4);
  return crc;
}
uint32_t adler32(const uint8_t* d, size_t n) {
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < n; i++) { a = (a + d[i]) % 65521; b = (b + a) % 65521; }
  return (b << 16) | a;
}
void pushU32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x >> 24); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF);
}
void pushChunk(std::vector<uint8_t>& v, const char* type, const uint8_t* data, size_t n) {
  pushU32(v, n);
  size_t tstart = v.size();
  for (int i = 0; i < 4; i++) v.push_back(type[i]);
  for (size_t i = 0; i < n; i++) v.push_back(data[i]);
  uint32_t c = 0xFFFFFFFF;
  for (size_t i = tstart; i < v.size(); i++) c = crcUpdate(c, v[i]);
  pushU32(v, c ^ 0xFFFFFFFF);
}

// canvas: 160x100 grayscale (0=ink,255=white) -> PNG bytes
#define IMG_W 160
#define IMG_H 100
uint8_t canvasImg[IMG_W * IMG_H];

String pngBase64() {
  // raw stream: each row = filter byte(0) + IMG_W pixels
  size_t rawN = (IMG_W + 1) * IMG_H;
  std::vector<uint8_t> raw(rawN + 7 + 4);   // zlib hdr(2) + block hdr(5) + data + adler(4)
  raw[0] = 0x78; raw[1] = 0x01;             // zlib header, no compression
  raw[2] = 0x01;                            // deflate stored block, BFINAL=1
  raw[3] = rawN & 0xFF; raw[4] = rawN >> 8;
  raw[5] = (~rawN) & 0xFF; raw[6] = ((~rawN) >> 8) & 0xFF;
  size_t o = 7;
  for (int y = 0; y < IMG_H; y++) {
    raw[o++] = 0;                            // filter none
    for (int x = 0; x < IMG_W; x++) raw[o++] = canvasImg[y * IMG_W + x];
  }
  uint32_t ad = adler32(&raw[7], rawN);
  raw[o++] = ad >> 24; raw[o++] = (ad >> 16) & 0xFF; raw[o++] = (ad >> 8) & 0xFF; raw[o++] = ad & 0xFF;

  std::vector<uint8_t> png;
  const uint8_t sig[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
  for (int i = 0; i < 8; i++) png.push_back(sig[i]);
  uint8_t ihdr[13];
  ihdr[0]=0; ihdr[1]=0; ihdr[2]=IMG_W>>8; ihdr[3]=IMG_W&0xFF;
  ihdr[4]=0; ihdr[5]=0; ihdr[6]=IMG_H>>8; ihdr[7]=IMG_H&0xFF;
  ihdr[8]=8; ihdr[9]=0; ihdr[10]=0; ihdr[11]=0; ihdr[12]=0;
  pushChunk(png, "IHDR", ihdr, 13);
  pushChunk(png, "IDAT", raw.data(), o);
  pushChunk(png, "IEND", NULL, 0);

  // base64
  static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  out.reserve(((png.size() + 2) / 3) * 4);
  for (size_t i = 0; i < png.size(); i += 3) {
    uint32_t n = ((uint32_t)png[i] << 16) | ((i+1 < png.size() ? png[i+1] : 0) << 8) | (i+2 < png.size() ? png[i+2] : 0);
    out += b64[(n >> 18) & 63]; out += b64[(n >> 12) & 63];
    out += (i+1 < png.size()) ? b64[(n >> 6) & 63] : '=';
    out += (i+2 < png.size()) ? b64[n & 63] : '=';
  }
  return out;
}

// =====================================================================
//                       WiFi manager (saved networks)
// =====================================================================
void setReply(const String& s);

void bigMsg(const String& l1, const String& l2) {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  String a = l1; if (a.length() > 25) a = a.substring(0, 25);
  String b = l2; if (b.length() > 25) b = b.substring(0, 25);
  drawText(20, 88, a, 2, C_WHITE, C_BLACK);
  drawText(20, 114, b, 2, C_CYAN, C_BLACK);
  drawText(20, 150, "tap EXIT to cancel", 1, C_GREY, C_BLACK);
  drawExitBtn();
}

void connMsg(bool toReply, const String& l1, const String& l2) {
  if (toReply) {
    String s = l1;
    if (l2.length()) { s += "\n"; s += l2; }
    s += "\n(tap EXIT to cancel)";
    setReply(s);
  } else {
    bigMsg(l1, l2);
  }
}

void saveNets() {
  prefs.putUChar("wn", nets.size());
  for (int i = 0; i < MAX_NETS; i++) {
    String ks = "ws" + String(i), kp = "wp" + String(i);
    if (i < (int)nets.size()) {
      prefs.putString(ks.c_str(), nets[i].ssid);
      prefs.putString(kp.c_str(), nets[i].pass);
    } else {
      prefs.remove(ks.c_str());
      prefs.remove(kp.c_str());
    }
  }
}

void loadNets() {
  nets.clear();
  if (!prefs.isKey("wn")) {                       // very first run: seed with the compile-time network
    if (strlen(WIFI_SSID) > 0) {
      NetCred nc; nc.ssid = WIFI_SSID; nc.pass = WIFI_PASS;
      nets.push_back(nc);
    }
    saveNets();
    return;
  }
  int n = prefs.getUChar("wn", 0);
  for (int i = 0; i < n && i < MAX_NETS; i++) {
    String ks = "ws" + String(i), kp = "wp" + String(i);
    NetCred nc;
    nc.ssid = prefs.getString(ks.c_str(), "");
    nc.pass = prefs.getString(kp.c_str(), "");
    if (nc.ssid.length()) nets.push_back(nc);
  }
}

int findNet(const String& ssid) {
  for (size_t i = 0; i < nets.size(); i++) if (nets[i].ssid == ssid) return (int)i;
  return -1;
}

// move/insert to the front (= "latest used") and persist
void rememberNet(const String& ssid, const String& pass) {
  int i = findNet(ssid);
  if (i == 0 && nets[0].pass == pass) return;     // nothing changed, spare the flash
  if (i >= 0) nets.erase(nets.begin() + i);
  NetCred nc; nc.ssid = ssid; nc.pass = pass;
  nets.insert(nets.begin(), nc);
  while ((int)nets.size() > MAX_NETS) nets.pop_back();
  saveNets();
}

void forgetNet(int i) {
  if (i < 0 || i >= (int)nets.size()) return;
  nets.erase(nets.begin() + i);
  saveNets();
}

// async scan so EXIT stays responsive. returns count, or -1 on failure/cancel
int doScan() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  lastWifiUse = millis();
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);
  unsigned long t = millis();
  for (;;) {
    int r = WiFi.scanComplete();
    if (r != WIFI_SCAN_RUNNING) return r;
    if (millis() - t > 12000) { WiFi.scanDelete(); return -1; }
    if (pollExit()) { cancelled = true; WiFi.scanDelete(); return -1; }
    delay(60);
  }
}

bool tryJoin(const String& ssid, const String& pass, unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);                    // recover by itself from brief router drops
  WiFi.setSleep(true);
  WiFi.disconnect(false);
  delay(50);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : NULL);
  unsigned long t = millis();
  while (millis() - t < timeoutMs) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return true;
    unsigned long el = millis() - t;
    if ((s == WL_CONNECT_FAILED && el > 1500) || (s == WL_NO_SSID_AVAIL && el > 4000)) break;
    if (pollExit()) { cancelled = true; return false; }
    delay(100);
  }
  return false;
}

// connect to the best saved network that is in range (most recently used first)
bool connectBest(bool toReply) {
  cancelled = false;
  if (nets.empty()) return false;
  connMsg(toReply, "Scanning WiFi...", "");
  int n = doScan();
  if (cancelled) return false;
  std::vector<int> order;
  for (size_t i = 0; i < nets.size(); i++) {
    bool vis = false;
    for (int k = 0; k < n; k++) if (WiFi.SSID(k) == nets[i].ssid) { vis = true; break; }
    if (vis) order.push_back((int)i);
  }
  WiFi.scanDelete();
  if (order.empty()) order.push_back(0);          // maybe a hidden network: just try the newest
  for (size_t k = 0; k < order.size(); k++) {
    String s = nets[order[k]].ssid, ps = nets[order[k]].pass;
    connMsg(toReply, "Connecting...", s);
    if (tryJoin(s, ps, 12000)) {
      rememberNet(s, ps);
      lastWifiUse = millis();
      return true;
    }
    if (cancelled) return false;
  }
  WiFi.disconnect(false);
  return false;
}

// make sure WiFi is up: quick reconnect first, then full scan + join of saved networks
bool ensureWifi(bool toReply) {
  if (WiFi.status() == WL_CONNECTED) { lastWifiUse = millis(); return true; }
  if (nets.empty()) return false;
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.reconnect();
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 4000) {
      if (pollExit()) { cancelled = true; return false; }
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) { lastWifiUse = millis(); return true; }
  }
  return connectBest(toReply);     // radio was off or reconnect failed
}

// ---------- Gemini (raw socket so EXIT stays live; optional image attached) ----------
String dechunk(String s) {
  String out; int i = 0;
  while (i < (int)s.length()) {
    int e = s.indexOf("\r\n", i);
    if (e < 0) break;
    int sz = (int)strtol(s.substring(i, e).c_str(), NULL, 16);
    if (sz <= 0) break;
    out += s.substring(e + 2, e + 2 + sz);
    i = e + 2 + sz + 2;
  }
  return out;
}

String askGemini(const String& prompt, bool withImage) {
  cancelled = false;
  if (!ensureWifi(true)) {
    if (cancelled) return "";
    return "WiFi not connected.";
  }

  String sysI = "You receive user content. If an image is attached, it is a hand-drawn "
                "sketch (physics circuit, graph, handwritten numbers/text, diagram). "
                "Analyze it carefully: read all text/numbers/components in it and answer "
                "the user's question about it. Plain text only, ASCII only.";
  String parts;
  if (withImage) {
    parts = "{\"inline_data\":{\"mime_type\":\"image/png\",\"data\":\"" + pngBase64() + "\"}},";
  }
  String body = "{\"system_instruction\":{\"parts\":[{\"text\":\"" + jsonEscape(sysI) + "\"}]},"
                "\"contents\":[{\"role\":\"user\",\"parts\":[" + parts +
                "{\"text\":\"" + jsonEscape(prompt) + "\"}]}],"
                "\"generationConfig\":{\"maxOutputTokens\":800,\"thinkingConfig\":{\"thinkingBudget\":0}}}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(45);
  if (!client.connect("generativelanguage.googleapis.com", 443)) return "Connection failed.";

  String req = "POST /v1beta/models/" + String(GEMINI_MODEL) + ":generateContent HTTP/1.1\r\n"
               "Host: generativelanguage.googleapis.com\r\n"
               "Content-Type: application/json\r\n"
               "x-goog-api-key: " + BOT_TOKEN + "\r\n"
               "Content-Length: " + String(body.length()) + "\r\n"
               "Connection: close\r\n\r\n";
  client.print(req);
  const uint8_t* bp = (const uint8_t*)body.c_str();
  size_t blen = body.length(), sent = 0;
  while (sent < blen) {
    size_t k = client.write(bp + sent, blen - sent);
    if (k == 0) break;
    sent += k;
  }

  String resp;
  unsigned long start = millis();
  while ((client.connected() || client.available()) && millis() - start < 45000) {
    if (pollExit()) { client.stop(); cancelled = true; return ""; }
    while (client.available()) { resp += (char)client.read(); start = millis(); lastWifiUse = millis(); }
    delay(5);
  }
  client.stop();

  int bi = resp.indexOf("\r\n\r\n");
  if (bi < 0) return "Bad response from server.";
  String head = resp.substring(0, bi);
  String bodyR = resp.substring(bi + 4);
  if (head.indexOf("hunked") >= 0) bodyR = dechunk(bodyR);

  int sp = head.indexOf(' ');
  int code = head.substring(sp + 1).toInt();
  if (code != 200) {
    String m = jsonStringAfter(bodyR, "message");
    return String("Error ") + code + ": " + (m.length() ? m : bodyR.substring(0, 200));
  }
  String t = jsonStringAfter(bodyR, "text");
  if (!t.length()) return "(no text returned - the answer may have been blocked)";
  t = toAscii(t);
  t.trim();
  return t;
}

// ---------- Reply view ----------
String draft = "";
String reply = "";
std::vector<String> lines;
int replyTop = 0;

const int REPLY_Y = 58, REPLY_H = 148;
int LINE_H = 8 * REPLY_SCALE + 2;
int VIS_LINES = REPLY_H / (8 * REPLY_SCALE + 2);
int COLS = (SCR_W - 8) / (6 * REPLY_SCALE);

void applyReplyScale() {
  LINE_H = 8 * replyScale + 2;
  VIS_LINES = REPLY_H / LINE_H;
  COLS = (SCR_W - 8) / (6 * replyScale);
}

void wrapText(const String& s) {
  lines.clear();
  String cur = "", word = "";
  auto flushWord = [&]() {
    if (!word.length()) return;
    if (cur.length() && (int)(cur.length() + 1 + word.length()) <= COLS) { cur += " "; cur += word; }
    else if (!cur.length() && (int)word.length() <= COLS) { cur = word; }
    else {
      if (cur.length()) lines.push_back(cur);
      cur = "";
      while ((int)word.length() > COLS) { lines.push_back(word.substring(0, COLS)); word = word.substring(COLS); }
      cur = word;
    }
    word = "";
  };
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\n') { flushWord(); lines.push_back(cur); cur = ""; }
    else if (c == ' ') flushWord();
    else word += c;
  }
  flushWord();
  if (cur.length()) lines.push_back(cur);
}

void drawReply() {
  fillRect(0, REPLY_Y, SCR_W, REPLY_H, C_BLACK);
  for (int i = 0; i < VIS_LINES; i++) {
    int idx = replyTop + i;
    if (idx >= (int)lines.size()) break;
    drawText(4, REPLY_Y + 2 + i * LINE_H, lines[idx], replyScale, C_WHITE, C_BLACK);
  }
  if (lines.size() > (size_t)VIS_LINES) {
    int total = lines.size();
    int barH = max(10, REPLY_H * VIS_LINES / total);
    int barY = REPLY_Y + (REPLY_H - barH) * replyTop / max(1, total - VIS_LINES);
    fillRect(SCR_W - 3, barY, 3, barH, C_GREY);
  }
}

void setReply(const String& s) {
  wrapText(s);
  replyTop = 0;
  drawReply();
}

void rewrapReply() {
  wrapText(reply.length() ? reply : "Ready. Tap WRITE, or DRAW to sketch a problem.");
  replyTop = 0;
}

// ---------- Gemini main screen ----------
void drawDraftBox() {
  fillRect(2, 24, 316, 30, C_BLACK);
  drawRectOutline(2, 24, 316, 30, C_CYAN);
  if (draft.length() == 0) {
    drawText(50, 32, "Tap WRITE or DRAW...", 2, C_GREY, C_BLACK);
  } else {
    String shown = draft;
    if (shown.length() > 24) shown = "<" + shown.substring(shown.length() - 23);
    drawText(50, 32, shown, 2, C_GREEN, C_BLACK);
  }
}

// 5 bottom buttons: WRITE DRAW UP DOWN SET
void drawButton(int i, const char* label, uint16_t col) {
  int x = i * 64;
  fillRect(x + 2, 208, 60, 30, col);
  drawText(x + 2 + (60 - textWidth(label, 2)) / 2, 208 + 7, label, 2, C_WHITE, col);
}

void drawWifiTag() {
  fillRect(125, 4, 195, 14, C_BLACK);
  String w;
  uint16_t col;
  if (WiFi.status() == WL_CONNECTED) {
    w = WiFi.SSID();
    if (w.length() > 30) w = w.substring(0, 30);
    col = C_GREEN;
  } else if (WiFi.getMode() == WIFI_OFF && wifiMode == 1) {
    w = "WiFi off (auto)";
    col = C_GREY;
  } else {
    w = "no WiFi";
    col = 0xF800;
  }
  drawText(SCR_W - 4 - textWidth(w, 1), 8, w, 1, col, C_BLACK);
}

void drawGemMain() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawText(45, 6, "Gemini", 2, C_CYAN, C_BLACK);
  drawWifiTag();
  if (draft.length()) {
    String h = "draft: " + String(draft.length()) + " chars";
    drawText(SCR_W - 4 - textWidth(h, 1), 20, h, 1, C_GREY, C_BLACK);
  }
  drawDraftBox();
  drawReply();
  drawButton(0, "WRITE", C_FUNC);
  drawButton(1, "DRAW", C_ORNG);
  drawButton(2, "UP", C_BTN);
  drawButton(3, "DOWN", C_BTN);
  drawButton(4, "SET", C_OP);
  drawExitBtn();
}

// ---------- Keyboard (letters / symbols layers; used for prompt AND wifi credentials) ----------
KeyDef keys[52];
int nkeys = 0;
bool shiftOn = false;

String& entryText() { return entryMode == EM_DRAFT ? draft : entryBuf; }

const char* actLabel() {
  if (entryMode == EM_DRAFT) return "SEND";
  if (entryMode == EM_SSID) return "NEXT";
  return "JOIN";
}

void addKey(int x, int row, int w, const char* lab) {
  KeyDef& k = keys[nkeys++];
  k.x = x; k.y = 34 + row * 40; k.w = w; k.h = 40;
  strncpy(k.lab, lab, 5); k.lab[5] = 0;
}
void addRow(int row, int x0, const char* chars) {
  for (int i = 0; chars[i]; i++) { char b[2] = {chars[i], 0}; addKey(x0 + i * 32, row, 32, b); }
}
void buildKeys() {
  nkeys = 0;
  addRow(0, 0, "1234567890");
  if (kbLayer == 0) {
    addRow(1, 0, "qwertyuiop");
    addRow(2, 16, "asdfghjkl");
    addKey(0, 3, 48, "SH");
    addRow(3, 48, "zxcvbnm");
    addKey(272, 3, 48, "DEL");
    addKey(0, 4, 48, "SYM");
    addKey(48, 4, 32, ",");
    addKey(80, 4, 32, "-");
    addKey(112, 4, 80, "space");
    addKey(192, 4, 32, ".");
    addKey(224, 4, 32, "?");
    addKey(256, 4, 64, actLabel());
  } else {
    addRow(1, 0, "!@#$%^&*()");
    addRow(2, 0, "-_=+[]{}\\|");
    addRow(3, 8, ";:'\"<>/`");
    addKey(272, 3, 48, "DEL");
    addKey(0, 4, 48, "ABC");
    addKey(48, 4, 32, ",");
    addKey(80, 4, 32, ".");
    addKey(112, 4, 80, "space");
    addKey(192, 4, 32, "~");
    addKey(224, 4, 32, "?");
    addKey(256, 4, 64, actLabel());
  }
}

void drawKey(const KeyDef& k) {
  String l = k.lab;
  uint16_t col = C_BTN;
  if (l == "SH") col = shiftOn ? C_ORNG : C_FUNC;
  else if (l == "DEL") col = C_RED;
  else if (l == "SEND" || l == "JOIN" || l == "NEXT") col = C_EQ;
  else if (l == "SYM" || l == "ABC") col = C_FUNC;
  String show = l;
  if (shiftOn && l.length() == 1 && l[0] >= 'a' && l[0] <= 'z') show[0] = l[0] - 32;
  fillRect(k.x + 1, k.y + 1, k.w - 2, k.h - 2, col);
  drawText(k.x + (k.w - textWidth(show, 2)) / 2, k.y + (k.h - 16) / 2, show, 2, C_WHITE, col);
}

void drawKeys() { for (int i = 0; i < nkeys; i++) drawKey(keys[i]); }

void drawInput() {
  fillRect(40, 2, 238, 30, C_BLACK);
  drawRectOutline(40, 2, 238, 30, C_CYAN);
  String& t = entryText();
  if (entryMode != EM_DRAFT && t.length() == 0) {
    if (entryMode == EM_SSID) {
      drawText(46, 10, "Network name", 2, C_GREY, C_BLACK);
    } else {
      drawText(46, 6, "Password for:", 1, C_GREY, C_BLACK);
      String s = selSsid; if (s.length() > 36) s = s.substring(0, 36);
      drawText(46, 18, s, 1, C_CYAN, C_BLACK);
    }
    return;
  }
  String shown = t;
  if (shown.length() > 19) shown = "<" + shown.substring(shown.length() - 18);
  drawText(46, 10, shown + "_", 2, C_GREEN, C_BLACK);
}

void drawKbScreen() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawInput();
  fillRect(282, 2, 36, 30, C_RED);
  drawText(282 + (36 - 12) / 2, 2 + (30 - 16) / 2, "X", 2, C_WHITE, C_RED);
  drawKeys();
  drawExitBtn();
}

void openEntry(int m, const String& init) {
  entryMode = m;
  if (m != EM_DRAFT) entryBuf = init;
  kbLayer = 0;
  shiftOn = false;
  buildKeys();
  gscr = G_KB;
  drawKbScreen();
}

// =====================================================================
//                       Settings / WiFi screens
// =====================================================================
void drawSettings();
void openWifiList();

String setValue(int id) {
  switch (id) {
    case 1: return String(setBright) + "%";
    case 2: { int v = SLEEP_OPTS[sleepIdx]; if (!v) return "NEVER"; if (v < 60) return String(v) + "s"; return String(v / 60) + "m"; }
    case 3: return String(penSize);
    case 4: return String(eraserSize);
    case 5: return replyScale == 1 ? "SMALL" : "LARGE";
    case 6: { const char* n[3] = {"SHORT", "NORMAL", "LONG"}; return String(n[ansLenIdx]); }
    case 7: return String(CPU_MHZ[cpuIdx]) + "MHz";
    case 8: return wifiMode == 0 ? "ALWAYS" : "DEMAND";
  }
  return "";
}

const char* SET_LABEL[12] = {"WiFi", "Bright", "Sleep", "Pen", "Eraser", "Reply txt",
                             "Answer", "CPU", "WiFi use", "Touch cal", "Saved WiFi", "Defaults"};

void drawSetRow(int r) {
  int id = setPage * 6 + r;
  int y = SET_Y0 + r * SET_RH;
  fillRect(0, y, SCR_W, SET_RH, C_BLACK);
  fillRect(4, y + SET_RH - 1, 312, 1, 0x2104);

  if (id == 0) {                                    // WiFi row: name + CHANGE
    drawText(4, y + 1, "WiFi", 2, C_WHITE, C_BLACK);
    String s; uint16_t col = C_GREY;
    if (WiFi.status() == WL_CONNECTED) { s = WiFi.SSID(); col = C_GREEN; }
    else if (nets.size()) s = "saved: " + nets[0].ssid;
    else s = "no network saved";
    if (s.length() > 26) s = s.substring(0, 26);
    drawText(4, y + 19, s, 1, col, C_BLACK);
    drawBtn(172, y + 2, 146, 26, "CHANGE", C_OP, 2);
    return;
  }

  drawText(4, y + 7, SET_LABEL[id], 2, C_WHITE, C_BLACK);
  if (id >= 9) {                                    // action rows
    const char* t = id == 9 ? "RUN" : (id == 10 ? "OPEN" : "RESET");
    drawBtn(172, y + 2, 146, 26, t, id == 11 ? C_RED : C_FUNC, 2);
    return;
  }
  drawBtn(172, y + 2, 34, 26, "-", C_FUNC, 2);
  String v = setValue(id);
  drawText(208 + (78 - textWidth(v, 2)) / 2, y + 7, v, 2, C_GREEN, C_BLACK);
  drawBtn(286, y + 2, 32, 26, "+", C_FUNC, 2);
}

void drawSettings() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawText(45, 6, "Settings", 2, C_CYAN, C_BLACK);
  String pg = String(setPage + 1) + "/2";
  drawText(SCR_W - 4 - textWidth(pg, 2), 6, pg, 2, C_GREY, C_BLACK);
  for (int r = 0; r < 6; r++) drawSetRow(r);
  drawBtn(2, 212, 156, 26, "< BACK", C_FUNC, 2);
  drawBtn(162, 212, 156, 26, setPage == 0 ? "MORE >" : "< PAGE 1", C_FUNC, 2);
  drawExitBtn();
}

void adjustSetting(int id, int dir) {
  switch (id) {
    case 1: setBright = clampi(setBright + dir * 10, 10, 100); setBacklight(setBright); break;
    case 2: sleepIdx = clampi(sleepIdx + dir, 0, 5); break;
    case 3: penSize = clampi(penSize + dir, 1, 5); break;
    case 4: eraserSize = clampi(eraserSize + dir, 3, 14); break;
    case 5: {
      int v = clampi(replyScale + dir, 1, 2);
      if (v != replyScale) { replyScale = v; applyReplyScale(); rewrapReply(); }
      break;
    }
    case 6: ansLenIdx = clampi(ansLenIdx + dir, 0, 2); break;
    case 7: cpuIdx = clampi(cpuIdx + dir, 0, 2); setCpuFrequencyMhz(CPU_MHZ[cpuIdx]); break;
    case 8: wifiMode = clampi(wifiMode + dir, 0, 1); break;
  }
  saveSettings();
}

void resetDefaults() {
  setBright = 70; sleepIdx = 1; penSize = DEF_PEN; eraserSize = DEF_ERASER;
  replyScale = REPLY_SCALE; ansLenIdx = 1; cpuIdx = 0; wifiMode = 1;
  setBacklight(setBright);
  setCpuFrequencyMhz(CPU_MHZ[cpuIdx]);
  applyReplyScale();
  rewrapReply();
  saveSettings();
}

// ----- saved networks screen -----
void drawNetsList() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawText(45, 6, "Saved WiFi", 2, C_CYAN, C_BLACK);
  drawText(45, 22, "tap = join,  X = forget", 1, C_GREY, C_BLACK);
  if (nets.empty()) drawText(20, 100, "No saved networks", 2, C_GREY, C_BLACK);
  for (size_t i = 0; i < nets.size(); i++) {
    int y = AP_Y0 + i * AP_RH;
    bool cur = WiFi.status() == WL_CONNECTED && WiFi.SSID() == nets[i].ssid;
    uint16_t col = cur ? C_EQ : C_BTN;
    fillRect(2, y + 1, 266, AP_RH - 3, col);
    String s = nets[i].ssid; if (s.length() > 20) s = s.substring(0, 20);
    drawText(6, y + 6, s, 2, C_WHITE, col);
    drawBtn(272, y + 1, 44, AP_RH - 3, "X", C_RED, 2);
  }
  drawBtn(2, 208, 156, 30, "< BACK", C_FUNC, 2);
  drawBtn(162, 208, 156, 30, "SCAN", C_ORNG, 2);
  drawExitBtn();
}

// ----- scan result screen -----
void buildApList(int n) {
  aps.clear();
  for (int i = 0; i < n && aps.size() < 30; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    int rs = WiFi.RSSI(i);
    bool sec = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    bool found = false;
    for (size_t k = 0; k < aps.size(); k++) {
      if (aps[k].ssid == s) { if (rs > aps[k].rssi) aps[k].rssi = rs; found = true; break; }
    }
    if (!found) { APInfo a; a.ssid = s; a.rssi = rs; a.secure = sec; a.manual = false; aps.push_back(a); }
  }
  for (size_t i = 0; i + 1 < aps.size(); i++)
    for (size_t j = 0; j + 1 < aps.size() - i; j++)
      if (aps[j].rssi < aps[j + 1].rssi) std::swap(aps[j], aps[j + 1]);
  APInfo m; m.ssid = "+ type a name..."; m.rssi = 0; m.secure = true; m.manual = true;
  aps.push_back(m);
}

void drawApRow(int vi) {
  int idx = apTop + vi;
  int y = AP_Y0 + vi * AP_RH;
  fillRect(0, y, SCR_W, AP_RH, C_BLACK);
  if (idx >= (int)aps.size()) return;
  APInfo& a = aps[idx];
  bool saved = !a.manual && findNet(a.ssid) >= 0;
  bool cur = !a.manual && WiFi.status() == WL_CONNECTED && WiFi.SSID() == a.ssid;
  uint16_t col = a.manual ? C_FUNC : (cur ? C_EQ : (saved ? C_OP : C_BTN));
  fillRect(2, y + 1, 314, AP_RH - 3, col);
  String s = a.ssid; if (s.length() > 15) s = s.substring(0, 15);
  drawText(6, y + 6, s, 2, C_WHITE, col);
  if (a.manual) return;
  const char* tag = cur ? "connected" : (saved ? "saved" : (a.secure ? "locked" : "open"));
  drawText(190, y + 10, tag, 1, C_WHITE, col);
  int bars = a.rssi > -55 ? 4 : (a.rssi > -65 ? 3 : (a.rssi > -75 ? 2 : 1));
  for (int b = 0; b < 4; b++) {
    int h = 4 + b * 4;
    fillRect(272 + b * 9, y + 23 - h, 5, h, b < bars ? C_WHITE : 0x4208);
  }
}

void drawWifiList() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  drawText(45, 6, "WiFi", 2, C_CYAN, C_BLACK);
  String c = WiFi.status() == WL_CONNECTED ? ("now: " + WiFi.SSID()) : String("not connected");
  if (c.length() > 30) c = c.substring(0, 30);
  drawText(SCR_W - 4 - textWidth(c, 1), 8, c, 1, C_GREY, C_BLACK);
  drawText(45, 22, "tap a network to join", 1, C_GREY, C_BLACK);
  for (int i = 0; i < AP_VIS; i++) drawApRow(i);
  drawBtn(2, 208, 76, 30, "UP", C_BTN, 2);
  drawBtn(82, 208, 76, 30, "DOWN", C_BTN, 2);
  drawBtn(162, 208, 76, 30, "SCAN", C_ORNG, 2);
  drawBtn(242, 208, 76, 30, "BACK", C_FUNC, 2);
  drawExitBtn();
}

void openWifiList() {
  gscr = G_WIFI;
  apTop = 0;
  bigMsg("Scanning WiFi...", "");
  int n = doScan();
  if (cancelled) {
    cancelled = false;
    app = APP_CALC; gscr = G_MAIN;
    drawCalc();
    return;
  }
  buildApList(n < 0 ? 0 : n);
  WiFi.scanDelete();
  lastWifiUse = millis();
  drawWifiList();
}

void joinFromUi(String ssid, String pass, bool typed) {
  bigMsg("Connecting...", ssid);
  WiFi.scanDelete();
  bool ok = tryJoin(ssid, pass, 15000);
  if (cancelled) {
    cancelled = false;
    app = APP_CALC; gscr = G_MAIN;
    drawCalc();
    return;
  }
  if (ok) {
    rememberNet(ssid, pass);
    lastWifiUse = millis();
    bigMsg("Connected!", WiFi.localIP().toString());
    drawText(20, 150, "saved - will auto-join   ", 1, C_GREY, C_BLACK);
    delay(1400);
    gscr = G_SET;
    drawSettings();
  } else {
    WiFi.disconnect(false);
    bigMsg("Could not connect", "wrong pass/no signal");
    delay(1800);
    if (pass.length() == 0 && !typed) {              // open network failed
      if (aps.size()) { gscr = G_WIFI; drawWifiList(); } else { gscr = G_SET; drawSettings(); }
    } else {
      selSsid = ssid;
      openEntry(EM_PASS, typed ? pass : String(""));   // let the user fix the password
    }
  }
}

void selectAp(int idx) {
  APInfo a = aps[idx];
  if (a.manual) { openEntry(EM_SSID, ""); return; }
  selSsid = a.ssid;
  int si = findNet(a.ssid);
  if (si >= 0) { String ps = nets[si].pass; joinFromUi(a.ssid, ps, false); }
  else if (!a.secure) joinFromUi(a.ssid, "", false);
  else openEntry(EM_PASS, "");
}

void handleWifiTouch(int x, int y) {
  if (y >= 208) {
    int b = clampi(x / 80, 0, 3);
    if (b == 0)      { apTop = max(0, apTop - (AP_VIS - 1)); drawWifiList(); }
    else if (b == 1) { apTop = min(max(0, (int)aps.size() - AP_VIS), apTop + (AP_VIS - 1)); drawWifiList(); }
    else if (b == 2) { waitRelease(); openWifiList(); }
    else             { gscr = G_SET; drawSettings(); }
    return;
  }
  if (y >= AP_Y0 && y < AP_Y0 + AP_VIS * AP_RH) {
    int idx = apTop + (y - AP_Y0) / AP_RH;
    if (idx < (int)aps.size()) { waitRelease(); selectAp(idx); }
  }
}

void handleNetsTouch(int x, int y) {
  if (y >= 208) {
    if (x < 160) { gscr = G_SET; drawSettings(); }
    else { waitRelease(); openWifiList(); }
    return;
  }
  if (y >= AP_Y0 && y < AP_Y0 + MAX_NETS * AP_RH) {
    int i = (y - AP_Y0) / AP_RH;
    if (i < (int)nets.size()) {
      waitRelease();
      if (x >= 270) { forgetNet(i); drawNetsList(); }
      else { selSsid = nets[i].ssid; String s = nets[i].ssid, ps = nets[i].pass; joinFromUi(s, ps, false); }
    }
  }
}

void handleSettingsTouch(int x, int y) {
  if (y >= 212) {
    if (x < 160) { gscr = G_MAIN; drawGemMain(); }
    else { setPage ^= 1; drawSettings(); }
    return;
  }
  if (y < SET_Y0 || x < 172) return;
  int r = (y - SET_Y0) / SET_RH;
  if (r > 5) return;
  int id = setPage * 6 + r;

  if (id == 0)       { waitRelease(); openWifiList(); }
  else if (id == 9)  { waitRelease(); calibrate(); drawSettings(); }
  else if (id == 10) { gscr = G_NETS; drawNetsList(); }
  else if (id == 11) { resetDefaults(); drawSettings(); }
  else {
    int dir = 0;
    if (x < 208) dir = -1;
    else if (x >= 286) dir = 1;
    if (dir) { adjustSetting(id, dir); drawSetRow(r); }
  }
}

// =====================================================================
//                         DRAW MODE
// canvas 160x100 internal, shown 2x on screen (320x200). Toolbar at y=200..240.
// Features: PEN / ERA (real eraser, bigger brush) / UNDO (per stroke) / CLR (undoable)
// Anti-glitch: median-filtered touch, pen-down confirmation, jump rejection,
//              lift debounce, light smoothing, canvas-bounds clamping.
// Pen / eraser thickness come from Settings.
// =====================================================================
#define MAX_JUMP      50     // screen px; bigger single-sample jumps are treated as noise
#define MAX_UNDO_PTS  3000   // history memory cap (older strokes get baked into base image)
#define MAX_STROKES   40
#define STROKE_CHUNK  1500   // long strokes get split into chunks so undo stays cheap
#define TB_BTN_W      53     // 6 toolbar buttons

bool eraser = false;

std::vector<Stroke> strokes;
size_t totalPts = 0;
uint8_t baseImg[IMG_W * IMG_H];  // everything older than the undo history

// ----- brush / line on any image buffer -----
void brushImg(uint8_t* img, int cx, int cy, int size, uint8_t v) {
  int bx = cx - size / 2, by = cy - size / 2;
  for (int yy = by; yy < by + size; yy++) {
    if (yy < 0 || yy >= IMG_H) continue;
    for (int xx = bx; xx < bx + size; xx++) {
      if (xx < 0 || xx >= IMG_W) continue;
      img[yy * IMG_W + xx] = v;
    }
  }
}

void showBrush(int cx, int cy, int size, uint8_t v) {
  int bx = cx - size / 2, by = cy - size / 2;
  int x0 = max(bx, 0), y0 = max(by, 0);
  int x1 = min(bx + size, (int)IMG_W), y1 = min(by + size, (int)IMG_H);
  if (x1 <= x0 || y1 <= y0) return;
  fillRect(x0 * 2, y0 * 2, (x1 - x0) * 2, (y1 - y0) * 2, v ? C_WHITE : C_BLACK);
}

// live = also paint to the screen
void lineImg(uint8_t* img, int x0, int y0, int x1, int y1, int size, uint8_t v, bool live) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    brushImg(img, x0, y0, size, v);
    if (live) showBrush(x0, y0, size, v);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// ----- fast full-canvas redraw (row streaming) -----
void canvasShowAll() {
  static uint8_t rowBuf[SCR_W * 2];
  setWindow(0, 0, SCR_W - 1, IMG_H * 2 - 1);
  spi.beginTransaction(dispSet);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_CS, LOW);
  for (int y = 0; y < IMG_H; y++) {
    int o = 0;
    for (int x = 0; x < IMG_W; x++) {
      uint8_t c = canvasImg[y * IMG_W + x] ? 0xFF : 0x00;
      rowBuf[o++] = c; rowBuf[o++] = c; rowBuf[o++] = c; rowBuf[o++] = c;
    }
    spi.writeBytes(rowBuf, SCR_W * 2);
    spi.writeBytes(rowBuf, SCR_W * 2);
  }
  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();
}

// ----- history -----
void renderStroke(uint8_t* img, const Stroke& s) {
  if (s.kind == 2) { memset(img, 255, IMG_W * IMG_H); return; }
  int size = s.kind == 1 ? eraserSize : penSize;
  uint8_t v = s.kind == 1 ? 255 : 0;
  size_t n = s.pts.size() / 2;
  if (n == 0) return;
  int px = s.pts[0], py = s.pts[1];
  brushImg(img, px, py, size, v);
  for (size_t i = 1; i < n; i++) {
    int x = s.pts[i * 2], y = s.pts[i * 2 + 1];
    lineImg(img, px, py, x, y, size, v, false);
    px = x; py = y;
  }
}

void trimHistory() {
  while ((strokes.size() > MAX_STROKES || totalPts > MAX_UNDO_PTS) && strokes.size() > 1) {
    renderStroke(baseImg, strokes[0]);
    totalPts -= strokes[0].pts.size() / 2;
    strokes.erase(strokes.begin());
  }
}

void commitStroke(uint8_t kind, const std::vector<uint8_t>& pts) {
  if (pts.empty()) return;
  Stroke s;
  s.kind = kind;
  s.pts = pts;
  totalPts += pts.size() / 2;
  strokes.push_back(s);
  trimHistory();
}

void redrawFromHistory() {
  memcpy(canvasImg, baseImg, sizeof(canvasImg));
  for (size_t i = 0; i < strokes.size(); i++) renderStroke(canvasImg, strokes[i]);
  canvasShowAll();
  drawExitBtn();
}

void undoStroke() {
  if (strokes.empty()) return;
  totalPts -= strokes.back().pts.size() / 2;
  strokes.pop_back();
  redrawFromHistory();
}

void resetCanvas() {                       // hard reset (entering DRAW)
  strokes.clear();
  totalPts = 0;
  memset(canvasImg, 255, sizeof(canvasImg));
  memset(baseImg, 255, sizeof(baseImg));
}

void clearAction() {                       // CLR button - can be undone
  if (!strokes.empty() && strokes.back().kind == 2) return;
  Stroke s;
  s.kind = 2;
  strokes.push_back(s);
  trimHistory();
  memset(canvasImg, 255, sizeof(canvasImg));
  canvasShowAll();
  drawExitBtn();
}

// ----- clean touch reading for drawing (median of 7, noise rejection) -----
static void sort7(int* a) {
  for (int i = 1; i < 7; i++) {
    int k = a[i], j = i - 1;
    while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; j--; }
    a[j + 1] = k;
  }
}

bool getTouchDraw(int& x, int& y) {
  int z1 = xr(0xB1);
  int z2 = xr(0xC1);
  int z = z1 + 4095 - z2;
  lastZ = z;
  if (z < Z_THRESHOLD || z > 3900) { xr(0x90); return false; }
  xr(0xD1); xr(0x91);
  int sx[7], sy[7];
  for (int i = 0; i < 7; i++) { sx[i] = xr(0xD1); sy[i] = xr(0x91); }
  xr(0x90);
  sort7(sx); sort7(sy);
  if (sx[5] - sx[1] > 150 || sy[5] - sy[1] > 150) return false;   // unstable reading
  int rx = sx[3], ry = sy[3];
  if (!(rx > 100 && ry > 100 && rx < 4000 && ry < 4000)) return false;
  float a = swapXY ? ry : rx;
  float b = swapXY ? rx : ry;
  x = CAL_X1 + (a - ax1) * (CAL_X2 - CAL_X1) / (ax2 - ax1);
  y = CAL_Y1 + (b - ay1) * (CAL_Y2 - CAL_Y1) / (ay2 - ay1);
  x = constrain(x, 0, SCR_W - 1);
  y = constrain(y, 0, SCR_H - 1);
  return true;
}

// ----- toolbar -----
void drawToolBtn(int i) {
  const char* labs[6] = { "PEN", "ERA", "UNDO", "CLR", "TXT", "SEND" };
  uint16_t col;
  switch (i) {
    case 0: col = eraser ? C_FUNC : C_OP;   break;   // PEN (highlighted when active)
    case 1: col = eraser ? C_ORNG : C_FUNC; break;   // ERA (highlighted when active)
    case 2: col = C_BTN;  break;
    case 3: col = C_RED;  break;
    case 4: col = C_FUNC; break;
    default: col = C_EQ;  break;
  }
  int x = i * TB_BTN_W + 1;
  fillRect(x, 202, TB_BTN_W - 2, 36, col);
  drawText(x + (TB_BTN_W - 2 - textWidth(labs[i], 2)) / 2, 202 + 10, labs[i], 2, C_WHITE, col);
}

void drawToolbar() {
  fillRect(0, 200, SCR_W, 40, C_BLACK);
  for (int i = 0; i < 6; i++) drawToolBtn(i);
}

void drawDrawScreen() {
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
  canvasShowAll();
  drawToolbar();
  drawExitBtn();
}

// ----- one stroke: from finger-down to finger-up. Returns true if EXIT was hit. -----
bool runStroke() {
  const int size = eraser ? eraserSize : penSize;
  const uint8_t v = eraser ? 255 : 0;
  const uint8_t kind = eraser ? 1 : 0;

  // 1) confirm pen-down: two clean consecutive samples close together
  int a = 0, b = 0, c = 0, d = 0;
  bool started = false;
  for (int t = 0; t < 25 && !started; t++) {
    if (!getTouchDraw(a, b)) { delay(4); continue; }
    delay(4);
    if (getTouchDraw(c, d) && abs(a - c) <= 10 && abs(b - d) <= 10) started = true;
  }
  if (!started) return false;
  if (d >= 200 || inExitZone(c, d)) return false;

  float fx = c, fy = d;                 // smoothed position (screen px)
  int lastSx = c, lastSy = d;           // last accepted raw sample
  int lastCx = constrain((int)(fx / 2 + 0.5f), 0, IMG_W - 1);
  int lastCy = constrain((int)(fy / 2 + 0.5f), 0, IMG_H - 1);

  std::vector<uint8_t> pts;
  pts.reserve(256);
  pts.push_back(lastCx); pts.push_back(lastCy);
  brushImg(canvasImg, lastCx, lastCy, size, v);
  showBrush(lastCx, lastCy, size, v);

  int misses = 0, rejN = 0, rejX = 0, rejY = 0;

  while (misses < 8) {                  // ~50 ms without touch = finger lifted
    int nx, ny;
    if (!getTouchDraw(nx, ny)) { misses++; delay(3); continue; }
    misses = 0;

    int dj = max(abs(nx - lastSx), abs(ny - lastSy));
    if (dj > MAX_JUMP) {
      // outlier: ignore it, unless it repeats at the same new spot (real re-touch)
      if (rejN > 0 && max(abs(nx - rejX), abs(ny - rejY)) <= 14) rejN++;
      else rejN = 1;
      rejX = nx; rejY = ny;
      if (rejN >= 3) {
        commitStroke(kind, pts);        // finish old stroke, begin a NEW one (no connecting line)
        pts.clear();
        if (inExitZone(nx, ny)) return true;
        if (ny >= 200) return false;
        fx = nx; fy = ny; lastSx = nx; lastSy = ny;
        lastCx = constrain((int)(fx / 2 + 0.5f), 0, IMG_W - 1);
        lastCy = constrain((int)(fy / 2 + 0.5f), 0, IMG_H - 1);
        pts.push_back(lastCx); pts.push_back(lastCy);
        brushImg(canvasImg, lastCx, lastCy, size, v);
        showBrush(lastCx, lastCy, size, v);
        rejN = 0;
      }
      delay(2);
      continue;
    }
    rejN = 0;
    lastSx = nx; lastSy = ny;

    if (inExitZone(nx, ny)) { commitStroke(kind, pts); return true; }

    fx = fx * 0.5f + nx * 0.5f;
    fy = fy * 0.5f + ny * 0.5f;
    int cx = constrain((int)(fx / 2 + 0.5f), 0, IMG_W - 1);
    int cy = constrain((int)(fy / 2 + 0.5f), 0, IMG_H - 1);   // never leaves the canvas
    if (cx != lastCx || cy != lastCy) {
      lineImg(canvasImg, lastCx, lastCy, cx, cy, size, v, true);
      pts.push_back(cx); pts.push_back(cy);
      lastCx = cx; lastCy = cy;
      if (pts.size() >= STROKE_CHUNK * 2) {
        commitStroke(kind, pts);
        pts.clear();
        pts.push_back(cx); pts.push_back(cy);
      }
    }
    delay(2);
  }
  commitStroke(kind, pts);
  return false;
}

void sendDrawing() {
  kbFromDraw = false;
  if (draft.length() == 0) draft = "Solve / analyze what is drawn in the image.";
  String q = draft;
  prefs.putString("draft", "");
  draft = "";
  gscr = G_MAIN;
  drawGemMain();
  setReply("Sending drawing...  (tap EXIT to cancel)");
  reply = askGemini(q, true);
  lastAct = millis();
  if (cancelled) {
    app = APP_CALC;
    gscr = G_MAIN;
    drawCalc();
    return;
  }
  saveAll();
  setReply(reply);
  drawGemMain();
}

// ---------- Persistence ----------
void saveAll() {
  prefs.putString("draft", draft);
  prefs.putString("reply", reply.substring(0, 3500));
  prefs.putString("calc", currentInput);
  prefs.putString("calcres", resultText);
}

void sendPrompt() {
  if (draft.length() == 0) return;
  String q = draft;
  prefs.putString("draft", "");
  draft = "";
  gscr = G_MAIN;
  drawGemMain();
  setReply("Thinking...  (tap EXIT to cancel)");
  reply = askGemini(q, false);
  lastAct = millis();
  if (cancelled) {
    app = APP_CALC;
    gscr = G_MAIN;
    drawCalc();
    return;
  }
  saveAll();
  setReply(reply);
  drawGemMain();
}

void onKey(const KeyDef& k) {
  String l = k.lab;
  if (l == "SH") { shiftOn = !shiftOn; drawKeys(); return; }
  if (l == "SYM" || l == "ABC") {
    kbLayer = (l == "SYM") ? 1 : 0;
    shiftOn = false;
    buildKeys();
    fillRect(0, 34, SCR_W, SCR_H - 34, C_BLACK);
    drawKeys();
    return;
  }
  if (l == "SEND" || l == "JOIN" || l == "NEXT") {
    waitRelease();
    if (entryMode == EM_DRAFT) {
      if (kbFromDraw) sendDrawing();       // came from the drawing -> send drawing + prompt
      else sendPrompt();
    } else if (entryMode == EM_SSID) {
      if (entryBuf.length()) { selSsid = entryBuf; openEntry(EM_PASS, ""); }
    } else {
      String s = selSsid, ps = entryBuf;
      joinFromUi(s, ps, true);
    }
    return;
  }
  String& t = entryText();
  int maxLen = entryMode == EM_DRAFT ? MAX_DRAFT : (entryMode == EM_SSID ? 32 : 63);
  if (l == "DEL") {
    if (t.length()) t.remove(t.length() - 1);
  } else if (l == "space") {
    if ((int)t.length() < maxLen) t += ' ';
  } else {
    char c = k.lab[0];
    if (shiftOn && c >= 'a' && c <= 'z') { c -= 32; shiftOn = false; drawKeys(); }
    if ((int)t.length() < maxLen) t += c;
  }
  if (entryMode == EM_DRAFT) saveAll();
  drawInput();
}

void setup() {
  Serial.begin(115200);
  powerSaveInit();

  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(T_CS, OUTPUT);   digitalWrite(T_CS, HIGH);
  spi.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, -1);

  backlightInit();
  blWrite(0);                                       // keep backlight off until the panel is ready

  initPanel();
  fillRect(0, 0, SCR_W, SCR_H, C_BLACK);

  prefs.begin("app", false);
  loadSettings();
  setCpuFrequencyMhz(CPU_MHZ[cpuIdx]);
  setBacklight(setBright);
  lastAct = millis();

  draft        = prefs.getString("draft", "");
  reply        = prefs.getString("reply", "");
  currentInput = prefs.getString("calc", "");
  resultText   = prefs.getString("calcres", "");

  WiFi.persistent(false);                           // don't wear the flash with WiFi config writes
  WiFi.mode(WIFI_OFF);
  loadNets();

  // touch calibration is saved; hold the screen at boot to redo it
  if (!loadCalibration()) {
    calibrate();
  } else {
    fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
    drawText(46, 96, "Hold to recalibrate", 2, C_WHITE, C_BLACK);
    bool recal = false;
    unsigned long t0 = millis();
    int rx, ry;
    while (millis() - t0 < 1200) { if (getRaw(rx, ry)) { recal = true; break; } delay(20); }
    if (recal) { waitRelease(); calibrate(); }
  }

  buildKeys();
  resetCanvas();
  applyReplyScale();
  rewrapReply();

  if (wifiMode == 0) { connectBest(false); cancelled = false; }   // "always on" mode connects at boot

  drawCalc();
  lastAct = millis();
}

void loop() {
  if (justOpenedGemini) {
    justOpenedGemini = false;
    app = APP_GEM;
    gscr = G_MAIN;
    drawGemMain();
  }

  // on-demand WiFi: switch the radio off only when truly idle
  if (wifiMode == 1 && WiFi.getMode() != WIFI_OFF) {
    unsigned long lastUse = lastWifiUse;
    if (app == APP_GEM && lastAct > lastUse) lastUse = lastAct;   // typing/drawing counts as use
    if (millis() - lastUse > WIFI_IDLE_MS) {
      bool busy = (app == APP_GEM) && (gscr == G_WIFI || gscr == G_NETS || (gscr == G_KB && entryMode != EM_DRAFT));
      if (!busy) {
        wifiOff();
        if (app == APP_GEM && gscr == G_MAIN && screenOn) drawWifiTag();
      }
    }
  }

  int x, y;
  if (getTouch(x, y)) {
    lastAct = millis();

    // sleeping screen: the first touch only wakes it up
    if (!screenOn) {
      wakeScreen();
      waitRelease();
      delay(10);
      return;
    }

    // EXIT checked FIRST, on every screen
    if (inExitZone(x, y)) {
      saveAll();
      app = APP_CALC;
      gscr = G_MAIN;
      waitRelease();
      drawCalc();
      delay(10);
      return;
    }

    if (app == APP_CALC) {
      if (y > topH) {
        int col = x / btnW;
        int row = (y - topH) / btnH;
        if (col < gridCols && row < gridRows) handleInput(labels[row][col]);
      }
      waitRelease();
    } else {
      if (gscr == G_MAIN) {
        if (y >= 208) {
          int b = min(4, x / 64);
          if (b == 0)      { kbFromDraw = false; openEntry(EM_DRAFT, ""); }
          else if (b == 1) { kbFromDraw = false; gscr = G_DRAW; eraser = false; resetCanvas(); drawDrawScreen(); }
          else if (b == 2) { replyTop = max(0, replyTop - (VIS_LINES - 1)); drawReply(); }
          else if (b == 3) { replyTop = min(max(0, (int)lines.size() - VIS_LINES), replyTop + (VIS_LINES - 1)); drawReply(); }
          else             { gscr = G_SET; setPage = 0; drawSettings(); }
        } else if (y >= 22 && y < 56) {
          kbFromDraw = false;
          openEntry(EM_DRAFT, "");
        }
      }
      else if (gscr == G_SET)  { handleSettingsTouch(x, y); }
      else if (gscr == G_WIFI) { handleWifiTouch(x, y); }
      else if (gscr == G_NETS) { handleNetsTouch(x, y); }
      else if (gscr == G_DRAW) {
        if (y >= 200) {                       // toolbar
          int b = min(5, x / TB_BTN_W);
          waitRelease();
          if (b == 0)      { eraser = false; drawToolbar(); }
          else if (b == 1) { eraser = true;  drawToolbar(); }
          else if (b == 2) { undoStroke(); }
          else if (b == 3) { clearAction(); }
          else if (b == 4) { kbFromDraw = true; openEntry(EM_DRAFT, ""); }   // TXT -> keyboard, drawing is kept
          else             { sendDrawing(); }
        } else {                              // drawing: one stroke per finger-down
          if (runStroke()) {                  // EXIT touched while drawing
            saveAll();
            app = APP_CALC; gscr = G_MAIN;
            waitRelease(); drawCalc();
            return;
          }
        }
      }
      else {                                  // keyboard screen
        if (x >= 280 && y < 34) {
          if (entryMode == EM_DRAFT) {
            saveAll();
            if (kbFromDraw) { gscr = G_DRAW; drawDrawScreen(); }   // back to the drawing, nothing lost
            else            { gscr = G_MAIN; drawGemMain(); }
          } else {                            // cancel wifi credential entry
            entryBuf = "";
            if (aps.size()) { gscr = G_WIFI; drawWifiList(); }
            else            { gscr = G_SET;  drawSettings(); }
          }
        } else {
          for (int i = 0; i < nkeys; i++) {
            const KeyDef& k = keys[i];
            if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) { onKey(k); break; }
          }
        }
      }
      waitRelease();
    }
    lastAct = millis();
  } else if (screenOn && SLEEP_OPTS[sleepIdx] > 0 &&
             millis() - lastAct > (unsigned long)SLEEP_OPTS[sleepIdx] * 1000UL) {
    sleepScreen();                            // idle -> backlight off (biggest battery saver)
  }
  delay(screenOn ? 10 : 40);
}
