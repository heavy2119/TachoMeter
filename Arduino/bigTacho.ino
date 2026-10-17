#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <MotorVID28.h>

// ============================================================
// VID29-05 계기판
// Arduino UNO/Nano 기준
//
// 모터 배선
//   방향 때문에 A/C는 바꿔서 연결
//   VID29 pin 1   -> D11
//   VID29 pin 2   -> D10
//   VID29 pin 3   -> D10
//   VID29 pin 4   -> D9
//
// LED
//   D6 -> WS2812B ring 16 LEDs
//   D5 -> WS2812B single LED
//
// 시리얼 명령
//   R0~R1200[,0|1]  실제 RPM/10. R613=6130RPM. ,1이면 변속 ON
//   K0~K3600[,0|1]  속도 x10. K1000이면 100.0km/h. ,1이면 변속 ON
//   S값           수동 위치 이동
//   CS,index,step 속도 보정값 변경
//   CR,index,step RPM 보정값 변경
//   LK,r,g,b,br      KM 링 16개 단색
//   KS,r,g,b,br      KM 싱글 1개 단색
//   LR,r,g,b,br      RPM 링 16개 단색
//   RS,r,g,b,br      RPM 싱글 1개 단색
//   LRZ,s,e,r,g,b,en RPM 링 범위색. s~e, en=1이면 사용
//   LKZ,s,e,r,g,b,en KM 링 범위색
//   SB,mode,ms,tgt,r,g,b  변속 깜빡임. mode 0=끔 1=RPM 2=KM, tgt 0=전체 1=범위
//   G1 / G0          변속 신호 ON/OFF (설정 프로그램 테스트용)
//   SAVE          EEPROM 저장
//   LOAD          EEPROM 로드
//   DUMP          현재 보정값 출력
//   RESETCFG      기본값으로 되돌림. 저장은 SAVE를 눌러야 됨
//   PARK          바늘 0점 복귀
//   REBOOT        setup()부터 다시 시작. EEPROM 확인용
// ============================================================

// -------------------- 모터 기본값 --------------------
// 방향이 반대로 움직이면, A,C 번호를 바꿀것!

const byte MOTOR_PIN_A = 11;
const byte MOTOR_PIN_B = 10;
const byte MOTOR_PIN_C = 9;


// 보정 상한 5000까지 허용 (기존 315° micro ≈ 3780보다 여유)
const int MOTOR_MICRO_STEPS = 5001;     // 부드럽게 움직일 때 사용
const int MOTOR_FAST_STEPS = 315 * 3;   // 빠르게 움직일 때 사용

const int MOTOR_TOTAL_STEPS = MOTOR_MICRO_STEPS;
const int MAX_GAUGE_STEP = MOTOR_TOTAL_STEPS - 1; // 5000

// 속도계는 부드럽게, RPM은 빠르게 반응하도록 따로 둠
const unsigned int SPEED_MOTOR_MAX_SPEED = 1800;
const unsigned int RPM_MOTOR_MAX_SPEED = 800;
const unsigned int MANUAL_MOTOR_MAX_SPEED = 3000;

// true면 빠른 coarse 모드, false면 부드러운 microstep 모드
const bool USE_FAST_FOR_SPEED = false;
const bool USE_FAST_FOR_MANUAL = false;

MotorVID28 gaugeFast(MOTOR_FAST_STEPS, false, MOTOR_PIN_A, MOTOR_PIN_B, MOTOR_PIN_C);
MotorVID28 gaugeMotor(MOTOR_MICRO_STEPS, true, MOTOR_PIN_A, MOTOR_PIN_B, MOTOR_PIN_C);

const int FAST_SWEEP_STEPS = 1000;
// 부팅 스윕 속도. 숫자가 낮을수록 빠름
const unsigned int FAST_SWEEP_STEP_US = 1000;

int commandedStep = 0;

// -------------------- LED 기본값 --------------------
const byte LED_RING_PIN = 6;
const byte LED_SINGLE_PIN = 5;

const byte LED_RING_COUNT = 16;
const byte LED_SINGLE_COUNT = 1;

Adafruit_NeoPixel ringLed(LED_RING_COUNT, LED_RING_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel singleLed(LED_SINGLE_COUNT, LED_SINGLE_PIN, NEO_GRB + NEO_KHZ800);

uint8_t rpmRed = 0;
uint8_t rpmGreen = 255;
uint8_t rpmBlue = 0;
uint8_t rpmBrightness = 155;

uint8_t rpmSingleRed = 255;
uint8_t rpmSingleGreen = 0;
uint8_t rpmSingleBlue = 0;
uint8_t rpmSingleBrightness = 155;

uint8_t kmRed = 255;
uint8_t kmGreen = 0;
uint8_t kmBlue = 0;
uint8_t kmBrightness = 155;

uint8_t kmSingleRed = 255;
uint8_t kmSingleGreen = 0;
uint8_t kmSingleBlue = 0;
uint8_t kmSingleBrightness = 155;

const uint8_t DEFAULT_RPM_RED = 0;
const uint8_t DEFAULT_RPM_GREEN = 255;
const uint8_t DEFAULT_RPM_BLUE = 0;
const uint8_t DEFAULT_RPM_BRIGHTNESS = 155;

const uint8_t DEFAULT_RPM_SINGLE_RED = 255;
const uint8_t DEFAULT_RPM_SINGLE_GREEN = 0;
const uint8_t DEFAULT_RPM_SINGLE_BLUE = 0;
const uint8_t DEFAULT_RPM_SINGLE_BRIGHTNESS = 155;

const uint8_t DEFAULT_KM_RED = 255;
const uint8_t DEFAULT_KM_GREEN = 0;
const uint8_t DEFAULT_KM_BLUE = 0;
const uint8_t DEFAULT_KM_BRIGHTNESS = 155;

const uint8_t DEFAULT_KM_SINGLE_RED = 255;
const uint8_t DEFAULT_KM_SINGLE_GREEN = 0;
const uint8_t DEFAULT_KM_SINGLE_BLUE = 0;
const uint8_t DEFAULT_KM_SINGLE_BRIGHTNESS = 155;

uint8_t rpmRangeEnabled = 0;
uint8_t rpmRangeStart = 6;
uint8_t rpmRangeEnd = 8;
uint8_t rpmRangeRed = 255;
uint8_t rpmRangeGreen = 0;
uint8_t rpmRangeBlue = 0;

uint8_t kmRangeEnabled = 0;
uint8_t kmRangeStart = 6;
uint8_t kmRangeEnd = 8;
uint8_t kmRangeRed = 255;
uint8_t kmRangeGreen = 0;
uint8_t kmRangeBlue = 0;

const uint8_t SHIFT_BLINK_OFF = 0;
const uint8_t SHIFT_BLINK_RPM = 1;
const uint8_t SHIFT_BLINK_KM = 2;
const uint8_t SHIFT_BLINK_TARGET_ALL = 0;
const uint8_t SHIFT_BLINK_TARGET_RANGE = 1;

uint8_t shiftBlinkMode = SHIFT_BLINK_OFF;
uint16_t shiftBlinkPeriodMs = 150;
uint8_t shiftBlinkTarget = SHIFT_BLINK_TARGET_ALL;
uint8_t shiftBlinkRed = 255;
uint8_t shiftBlinkGreen = 0;
uint8_t shiftBlinkBlue = 0;

bool shiftSignalOn = false;
bool shiftBlinkLit = true;
unsigned long lastShiftBlinkMs = 0;

// -------------------- RPM 보정 --------------------
// CR index N = N×1000 RPM 눈금. index 12 = 12000 RPM (R1200)
const int RPM_DISPLAY_MAX = 12000;
const byte RPM_CALIB_POINT_COUNT = 13; // index 0~12 → 0~12000 RPM

// EEPROM이 비어 있으면 이 값으로 시작
const int DEFAULT_RPM_TICK_TO_STEP[13] = {
  0,
  315,
  630,
  945,
  1260,
  1575,
  1890,
  2205,
  2520,
  2835,
  3150,
  3465,
  3779   // 12000 RPM
};

int RPM_TICK_TO_STEP[13] = {
  0,
  315,
  630,
  945,
  1260,
  1575,
  1890,
  2205,
  2520,
  2835,
  3150,
  3465,
  3779
};

// -------------------- 속도 보정 --------------------
// SPEED_KMH 눈금에 맞는 바늘 위치
// fast 모드를 써도 저장값은 microstep 기준으로 유지
const byte SPEED_POINT_COUNT_LEGACY = 16; // EEPROM v1~v8 (0~300 km/h)
const byte SPEED_POINT_COUNT = 19;        // EEPROM v9+ (0~360 km/h)

const int SPEED_KMH[SPEED_POINT_COUNT] = {
  0, 20, 40, 60, 80, 100, 120, 140,
  160, 180, 200, 220, 240, 260, 280, 300,
  320, 340, 360
};

const int DEFAULT_SPEED_TO_STEP[SPEED_POINT_COUNT] = {
  0,     // 0 km/h
  225,   // 20 km/h
  417,   // 40 km/h
  640,   // 60 km/h
  862,   // 80 km/h
  1071,  // 100 km/h
  1290,  // 120 km/h
  1520,  // 140 km/h
  1740,  // 160 km/h
  1967,  // 180 km/h
  2016,  // 200 km/h
  2190,  // 220 km/h
  2400,  // 240 km/h
  2620,  // 260 km/h
  2850,  // 280 km/h
  3100,  // 300 km/h
  3350,  // 320 km/h
  3600,  // 340 km/h
  3779   // 360 km/h
};

int SPEED_TO_STEP[SPEED_POINT_COUNT] = {
  0,     // 0 km/h
  225,   // 20 km/h
  417,   // 40 km/h
  640,   // 60 km/h
  862,   // 80 km/h
  1071,  // 100 km/h
  1290,  // 120 km/h
  1520,  // 140 km/h
  1740,  // 160 km/h
  1967,  // 180 km/h
  2016,  // 200 km/h
  2190,  // 220 km/h
  2400,  // 240 km/h
  2620,  // 260 km/h
  2850,  // 280 km/h
  3100,  // 300 km/h
  3350,  // 320 km/h
  3600,  // 340 km/h
  3779   // 360 km/h
};

// -------------------- EEPROM 저장 형식 --------------------
const unsigned long EEPROM_MAGIC = 0x47554745UL; // GUGE
const byte EEPROM_VERSION = 11;

struct GaugeConfigV1 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  unsigned int checksum;
};

struct GaugeConfigV2 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t ringRed;
  uint8_t ringGreen;
  uint8_t ringBlue;
  uint8_t ringBrightness;
  uint8_t singleRed;
  uint8_t singleGreen;
  uint8_t singleBlue;
  uint8_t singleBrightness;
  unsigned int checksum;
};

struct GaugeConfigV3 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t rpmStartRed;
  uint8_t rpmStartGreen;
  uint8_t rpmStartBlue;
  uint8_t rpmEndRed;
  uint8_t rpmEndGreen;
  uint8_t rpmEndBlue;
  uint8_t rpmBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  unsigned int checksum;
};

struct GaugeConfigV4 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t rpmStartRed;
  uint8_t rpmStartGreen;
  uint8_t rpmStartBlue;
  uint8_t rpmEndRed;
  uint8_t rpmEndGreen;
  uint8_t rpmEndBlue;
  uint8_t rpmBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t singleRed;
  uint8_t singleGreen;
  uint8_t singleBlue;
  uint8_t singleBrightness;
  unsigned int checksum;
};

struct GaugeConfigV5 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t rpmStartRed;
  uint8_t rpmStartGreen;
  uint8_t rpmStartBlue;
  uint8_t rpmEndRed;
  uint8_t rpmEndGreen;
  uint8_t rpmEndBlue;
  uint8_t rpmBrightness;
  uint8_t rpmStartOffset;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t singleRed;
  uint8_t singleGreen;
  uint8_t singleBlue;
  uint8_t singleBrightness;
  unsigned int checksum;
};

struct GaugeConfigV6 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t rpmStartRed;
  uint8_t rpmStartGreen;
  uint8_t rpmStartBlue;
  uint8_t rpmEndRed;
  uint8_t rpmEndGreen;
  uint8_t rpmEndBlue;
  uint8_t rpmBrightness;
  uint8_t rpmStartOffset;
  uint8_t rpmBaseRed;
  uint8_t rpmBaseGreen;
  uint8_t rpmBaseBlue;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t singleRed;
  uint8_t singleGreen;
  uint8_t singleBlue;
  uint8_t singleBrightness;
  unsigned int checksum;
};

struct GaugeConfigV7 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t rpmRed;
  uint8_t rpmGreen;
  uint8_t rpmBlue;
  uint8_t rpmBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t singleRed;
  uint8_t singleGreen;
  uint8_t singleBlue;
  uint8_t singleBrightness;
  unsigned int checksum;
};

// EEPROM v8: 속도 보정 0~300 km/h (16점)
struct GaugeConfigV8 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT_LEGACY];
  uint8_t rpmRed;
  uint8_t rpmGreen;
  uint8_t rpmBlue;
  uint8_t rpmBrightness;
  uint8_t rpmSingleRed;
  uint8_t rpmSingleGreen;
  uint8_t rpmSingleBlue;
  uint8_t rpmSingleBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t kmSingleRed;
  uint8_t kmSingleGreen;
  uint8_t kmSingleBlue;
  uint8_t kmSingleBrightness;
  unsigned int checksum;
};

// EEPROM v9: 속도 보정 0~360 km/h (19점)
struct GaugeConfigV9 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT];
  uint8_t rpmRed;
  uint8_t rpmGreen;
  uint8_t rpmBlue;
  uint8_t rpmBrightness;
  uint8_t rpmSingleRed;
  uint8_t rpmSingleGreen;
  uint8_t rpmSingleBlue;
  uint8_t rpmSingleBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t kmSingleRed;
  uint8_t kmSingleGreen;
  uint8_t kmSingleBlue;
  uint8_t kmSingleBrightness;
  unsigned int checksum;
};

// EEPROM v10: 링 범위색 + 변속 깜빡임
struct GaugeConfigV10 {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT];
  uint8_t rpmRed;
  uint8_t rpmGreen;
  uint8_t rpmBlue;
  uint8_t rpmBrightness;
  uint8_t rpmSingleRed;
  uint8_t rpmSingleGreen;
  uint8_t rpmSingleBlue;
  uint8_t rpmSingleBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t kmSingleRed;
  uint8_t kmSingleGreen;
  uint8_t kmSingleBlue;
  uint8_t kmSingleBrightness;
  uint8_t rpmRangeEnabled;
  uint8_t rpmRangeStart;
  uint8_t rpmRangeEnd;
  uint8_t rpmRangeRed;
  uint8_t rpmRangeGreen;
  uint8_t rpmRangeBlue;
  uint8_t kmRangeEnabled;
  uint8_t kmRangeStart;
  uint8_t kmRangeEnd;
  uint8_t kmRangeRed;
  uint8_t kmRangeGreen;
  uint8_t kmRangeBlue;
  uint8_t shiftBlinkMode;
  uint16_t shiftBlinkPeriodMs;
  uint8_t shiftBlinkTarget;
  unsigned int checksum;
};

// EEPROM v11: 변속 깜빡임 색상
struct GaugeConfig {
  unsigned long magic;
  byte version;
  int rpm[13];
  int speed[SPEED_POINT_COUNT];
  uint8_t rpmRed;
  uint8_t rpmGreen;
  uint8_t rpmBlue;
  uint8_t rpmBrightness;
  uint8_t rpmSingleRed;
  uint8_t rpmSingleGreen;
  uint8_t rpmSingleBlue;
  uint8_t rpmSingleBrightness;
  uint8_t kmRed;
  uint8_t kmGreen;
  uint8_t kmBlue;
  uint8_t kmBrightness;
  uint8_t kmSingleRed;
  uint8_t kmSingleGreen;
  uint8_t kmSingleBlue;
  uint8_t kmSingleBrightness;
  uint8_t rpmRangeEnabled;
  uint8_t rpmRangeStart;
  uint8_t rpmRangeEnd;
  uint8_t rpmRangeRed;
  uint8_t rpmRangeGreen;
  uint8_t rpmRangeBlue;
  uint8_t kmRangeEnabled;
  uint8_t kmRangeStart;
  uint8_t kmRangeEnd;
  uint8_t kmRangeRed;
  uint8_t kmRangeGreen;
  uint8_t kmRangeBlue;
  uint8_t shiftBlinkMode;
  uint16_t shiftBlinkPeriodMs;
  uint8_t shiftBlinkTarget;
  uint8_t shiftBlinkRed;
  uint8_t shiftBlinkGreen;
  uint8_t shiftBlinkBlue;
  unsigned int checksum;
};

// -------------------- 텔레메트리 끊김 처리 --------------------
const unsigned long TELEMETRY_TIMEOUT_MS = 700;
// RD/T 가 이 시간 동안 없으면 심허브가 꺼진 것으로 보고 0 복귀 후 소등
const unsigned long HOST_SILENCE_MS = 2000;

unsigned long lastTelemetryMs = 0;
unsigned long lastHostLineMs = 0;
bool telemetryTimedOut = false;
bool manualHoldMode = false;
bool gaugeReady = false;
bool gaugeHoming = false;

enum GaugeMode {
  GAUGE_MODE_SPEED,
  GAUGE_MODE_RPM
};

GaugeMode activeGaugeMode = GAUGE_MODE_SPEED;

// -------------------- 시리얼 수신 버퍼 --------------------
const byte RX_BUF_SIZE = 48;
char rxBuf[RX_BUF_SIZE];
byte rxLen = 0;

// 링 LED는 모드 전환·색 설정 시에만 show() (텔레메트리마다 호출 금지)
byte ringDisplayMode = 0; // 0=none, 1=rpm, 2=km
bool rpmLedForceUpdate = true;
bool kmLedForceUpdate = true;
bool pendingRpmLed = false;
bool pendingKmLed = false;

// ============================================================
// LED (KM 링/싱글, RPM 링/싱글 — 단색. 물리 싱글 LED는 D5 1개)
// ============================================================

void invalidateRingLed() {
  rpmLedForceUpdate = true;
  kmLedForceUpdate = true;
  ringDisplayMode = 0;
}

void applySingleLed(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness) {
  uint8_t b = brightness;
  if (b >= 255) b = 254;
  if (b < 1) b = 1;
  singleLed.setBrightness(b);
  singleLed.setPixelColor(0, singleLed.Color(red, green, blue));
  singleLed.show();
}

void applyRpmSingleLed() {
  applySingleLed(rpmSingleRed, rpmSingleGreen, rpmSingleBlue, rpmSingleBrightness);
}

void applyKmSingleLed() {
  applySingleLed(kmSingleRed, kmSingleGreen, kmSingleBlue, kmSingleBrightness);
}

void applyRangePixels(uint8_t enabled, uint8_t startIdx, uint8_t endIdx,
                      uint8_t red, uint8_t green, uint8_t blue) {
  if (!enabled) return;
  if (startIdx >= LED_RING_COUNT) return;
  if (endIdx >= LED_RING_COUNT) endIdx = LED_RING_COUNT - 1;
  if (startIdx > endIdx) return;

  uint32_t color = ringLed.Color(red, green, blue);
  for (byte i = startIdx; i <= endIdx; i++) {
    ringLed.setPixelColor(i, color);
  }
}

void showRingPattern(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness,
                     uint8_t rangeEn, uint8_t rangeStart, uint8_t rangeEnd,
                     uint8_t rangeR, uint8_t rangeG, uint8_t rangeB,
                     byte mode, bool blinkOff, bool blinkRangeOnly) {
  uint8_t b = brightness;
  if (b >= 255) b = 254;
  if (b < 1) b = 1;
  ringLed.setBrightness(b);

  if (blinkOff && !blinkRangeOnly) {
    ringLed.clear();
  } else {
    uint32_t color = ringLed.Color(red, green, blue);
    for (byte i = 0; i < LED_RING_COUNT; i++) {
      ringLed.setPixelColor(i, color);
    }

    if (!(blinkOff && blinkRangeOnly)) {
      applyRangePixels(rangeEn, rangeStart, rangeEnd, rangeR, rangeG, rangeB);
    }
  }

  ringLed.show();
  ringDisplayMode = mode;
}

bool isShiftBlinkActive() {
  if (!shiftSignalOn || shiftBlinkMode == SHIFT_BLINK_OFF) return false;
  if (shiftBlinkMode == SHIFT_BLINK_RPM) return activeGaugeMode == GAUGE_MODE_RPM;
  if (shiftBlinkMode == SHIFT_BLINK_KM) return activeGaugeMode == GAUGE_MODE_SPEED;
  return false;
}

void showCurrentRing(bool blinkOff) {
  bool rangeOnly = (shiftBlinkTarget == SHIFT_BLINK_TARGET_RANGE);
  uint8_t baseR, baseG, baseB, baseBr;
  uint8_t rangeEn, rangeStart, rangeEnd, rangeR, rangeG, rangeB;
  byte mode;

  if (activeGaugeMode == GAUGE_MODE_RPM) {
    baseR = rpmRed; baseG = rpmGreen; baseB = rpmBlue; baseBr = rpmBrightness;
    rangeEn = rpmRangeEnabled; rangeStart = rpmRangeStart; rangeEnd = rpmRangeEnd;
    rangeR = rpmRangeRed; rangeG = rpmRangeGreen; rangeB = rpmRangeBlue;
    mode = 1;
  } else {
    baseR = kmRed; baseG = kmGreen; baseB = kmBlue; baseBr = kmBrightness;
    rangeEn = kmRangeEnabled; rangeStart = kmRangeStart; rangeEnd = kmRangeEnd;
    rangeR = kmRangeRed; rangeG = kmRangeGreen; rangeB = kmRangeBlue;
    mode = 2;
  }

  if (!blinkOff && !rangeOnly) {
    showRingPattern(shiftBlinkRed, shiftBlinkGreen, shiftBlinkBlue, baseBr,
                    0, 0, 0, 0, 0, 0,
                    mode, false, false);
  } else if (!blinkOff && rangeOnly) {
    showRingPattern(baseR, baseG, baseB, baseBr,
                    1, rangeStart, rangeEnd,
                    shiftBlinkRed, shiftBlinkGreen, shiftBlinkBlue,
                    mode, false, false);
  } else {
    showRingPattern(baseR, baseG, baseB, baseBr,
                    rangeEn, rangeStart, rangeEnd, rangeR, rangeG, rangeB,
                    mode, true, rangeOnly);
  }

  if (mode == 1) {
    applyRpmSingleLed();
    rpmLedForceUpdate = false;
    kmLedForceUpdate = true;
  } else {
    applyKmSingleLed();
    kmLedForceUpdate = false;
    rpmLedForceUpdate = true;
  }
}

void showRpmLedSolid() {
  if (isShiftBlinkActive()) return;
  if (ringDisplayMode == 1 && !rpmLedForceUpdate) return;
  showRingPattern(rpmRed, rpmGreen, rpmBlue, rpmBrightness,
                  rpmRangeEnabled, rpmRangeStart, rpmRangeEnd,
                  rpmRangeRed, rpmRangeGreen, rpmRangeBlue,
                  1, false, false);
  applyRpmSingleLed();
  rpmLedForceUpdate = false;
  kmLedForceUpdate = true;
}

void showKmLedSolid() {
  if (isShiftBlinkActive()) return;
  if (ringDisplayMode == 2 && !kmLedForceUpdate) return;
  showRingPattern(kmRed, kmGreen, kmBlue, kmBrightness,
                  kmRangeEnabled, kmRangeStart, kmRangeEnd,
                  kmRangeRed, kmRangeGreen, kmRangeBlue,
                  2, false, false);
  applyKmSingleLed();
  kmLedForceUpdate = false;
  rpmLedForceUpdate = true;
}

void updateShiftBlink() {
  if (!isShiftBlinkActive()) {
    return;
  }

  unsigned long now = millis();
  if (now - lastShiftBlinkMs < shiftBlinkPeriodMs) return;

  lastShiftBlinkMs = now;
  shiftBlinkLit = !shiftBlinkLit;
  showCurrentRing(!shiftBlinkLit);
}

void turnGaugeLedsOff() {
  ringLed.clear();
  singleLed.clear();
  ringLed.show();
  singleLed.show();
  invalidateRingLed();
  pendingRpmLed = false;
  pendingKmLed = false;
}

void setupLeds() {
  ringLed.begin();
  singleLed.begin();
  turnGaugeLedsOff();
}

void loadDefaultLedConfig() {
  rpmRed = DEFAULT_RPM_RED;
  rpmGreen = DEFAULT_RPM_GREEN;
  rpmBlue = DEFAULT_RPM_BLUE;
  rpmBrightness = DEFAULT_RPM_BRIGHTNESS;
  rpmSingleRed = DEFAULT_RPM_SINGLE_RED;
  rpmSingleGreen = DEFAULT_RPM_SINGLE_GREEN;
  rpmSingleBlue = DEFAULT_RPM_SINGLE_BLUE;
  rpmSingleBrightness = DEFAULT_RPM_SINGLE_BRIGHTNESS;

  kmRed = DEFAULT_KM_RED;
  kmGreen = DEFAULT_KM_GREEN;
  kmBlue = DEFAULT_KM_BLUE;
  kmBrightness = DEFAULT_KM_BRIGHTNESS;
  kmSingleRed = DEFAULT_KM_SINGLE_RED;
  kmSingleGreen = DEFAULT_KM_SINGLE_GREEN;
  kmSingleBlue = DEFAULT_KM_SINGLE_BLUE;
  kmSingleBrightness = DEFAULT_KM_SINGLE_BRIGHTNESS;

  rpmRangeEnabled = 0;
  rpmRangeStart = 6;
  rpmRangeEnd = 8;
  rpmRangeRed = 255;
  rpmRangeGreen = 0;
  rpmRangeBlue = 0;

  kmRangeEnabled = 0;
  kmRangeStart = 6;
  kmRangeEnd = 8;
  kmRangeRed = 255;
  kmRangeGreen = 0;
  kmRangeBlue = 0;

  shiftBlinkMode = SHIFT_BLINK_OFF;
  shiftBlinkPeriodMs = 150;
  shiftBlinkTarget = SHIFT_BLINK_TARGET_ALL;
  shiftBlinkRed = 255;
  shiftBlinkGreen = 0;
  shiftBlinkBlue = 0;
  shiftSignalOn = false;
  shiftBlinkLit = true;
}

// ============================================================
// EEPROM
// ============================================================

void loadDefaultConfig() {
  for (byte i = 0; i < 13; i++) {
    RPM_TICK_TO_STEP[i] = DEFAULT_RPM_TICK_TO_STEP[i];
  }

  for (byte i = 0; i < SPEED_POINT_COUNT; i++) {
    SPEED_TO_STEP[i] = DEFAULT_SPEED_TO_STEP[i];
  }

  loadDefaultLedConfig();
}

unsigned int calculateChecksumBytes(const byte* data, unsigned int sizeWithoutChecksum) {
  unsigned int sum = 0;

  for (unsigned int i = 0; i < sizeWithoutChecksum; i++) {
    sum = (sum * 31) + data[i];
  }

  return sum;
}

unsigned int calculateConfigChecksum(const GaugeConfig& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfig) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV1Checksum(const GaugeConfigV1& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV1) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV2Checksum(const GaugeConfigV2& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV2) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV3Checksum(const GaugeConfigV3& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV3) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV4Checksum(const GaugeConfigV4& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV4) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV5Checksum(const GaugeConfigV5& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV5) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV6Checksum(const GaugeConfigV6& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV6) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV7Checksum(const GaugeConfigV7& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV7) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV8Checksum(const GaugeConfigV8& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV8) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV9Checksum(const GaugeConfigV9& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV9) - sizeof(cfg.checksum));
}

unsigned int calculateConfigV10Checksum(const GaugeConfigV10& cfg) {
  return calculateChecksumBytes((const byte*)&cfg, sizeof(GaugeConfigV10) - sizeof(cfg.checksum));
}

void applyLegacySpeedPoints(const int* speed16) {
  for (byte i = 0; i < SPEED_POINT_COUNT_LEGACY; i++) {
    SPEED_TO_STEP[i] = speed16[i];
  }

  for (byte i = SPEED_POINT_COUNT_LEGACY; i < SPEED_POINT_COUNT; i++) {
    SPEED_TO_STEP[i] = DEFAULT_SPEED_TO_STEP[i];
  }
}

void applyLedFromLegacyStart(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  rpmRed = r;
  rpmGreen = g;
  rpmBlue = b;
  rpmBrightness = brightness;
}

void applyLegacySharedSingle(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  rpmSingleRed = r;
  rpmSingleGreen = g;
  rpmSingleBlue = b;
  rpmSingleBrightness = brightness;
  kmSingleRed = r;
  kmSingleGreen = g;
  kmSingleBlue = b;
  kmSingleBrightness = brightness;
}

void saveConfigToEeprom() {
  GaugeConfig cfg;
  memset(&cfg, 0, sizeof(cfg));

  cfg.magic = EEPROM_MAGIC;
  cfg.version = EEPROM_VERSION;

  for (byte i = 0; i < 13; i++) {
    cfg.rpm[i] = RPM_TICK_TO_STEP[i];
  }

  for (byte i = 0; i < SPEED_POINT_COUNT; i++) {
    cfg.speed[i] = SPEED_TO_STEP[i];
  }

  cfg.rpmRed = rpmRed;
  cfg.rpmGreen = rpmGreen;
  cfg.rpmBlue = rpmBlue;
  cfg.rpmBrightness = rpmBrightness;
  cfg.rpmSingleRed = rpmSingleRed;
  cfg.rpmSingleGreen = rpmSingleGreen;
  cfg.rpmSingleBlue = rpmSingleBlue;
  cfg.rpmSingleBrightness = rpmSingleBrightness;

  cfg.kmRed = kmRed;
  cfg.kmGreen = kmGreen;
  cfg.kmBlue = kmBlue;
  cfg.kmBrightness = kmBrightness;
  cfg.kmSingleRed = kmSingleRed;
  cfg.kmSingleGreen = kmSingleGreen;
  cfg.kmSingleBlue = kmSingleBlue;
  cfg.kmSingleBrightness = kmSingleBrightness;

  cfg.rpmRangeEnabled = rpmRangeEnabled;
  cfg.rpmRangeStart = rpmRangeStart;
  cfg.rpmRangeEnd = rpmRangeEnd;
  cfg.rpmRangeRed = rpmRangeRed;
  cfg.rpmRangeGreen = rpmRangeGreen;
  cfg.rpmRangeBlue = rpmRangeBlue;

  cfg.kmRangeEnabled = kmRangeEnabled;
  cfg.kmRangeStart = kmRangeStart;
  cfg.kmRangeEnd = kmRangeEnd;
  cfg.kmRangeRed = kmRangeRed;
  cfg.kmRangeGreen = kmRangeGreen;
  cfg.kmRangeBlue = kmRangeBlue;

  cfg.shiftBlinkMode = shiftBlinkMode;
  cfg.shiftBlinkPeriodMs = shiftBlinkPeriodMs;
  cfg.shiftBlinkTarget = shiftBlinkTarget;
  cfg.shiftBlinkRed = shiftBlinkRed;
  cfg.shiftBlinkGreen = shiftBlinkGreen;
  cfg.shiftBlinkBlue = shiftBlinkBlue;

  cfg.checksum = calculateConfigChecksum(cfg);
  EEPROM.put(0, cfg);
}

bool loadConfigFromEeprom() {
  GaugeConfig cfg;
  EEPROM.get(0, cfg);

  if (cfg.magic == EEPROM_MAGIC &&
      cfg.version == EEPROM_VERSION &&
      cfg.checksum == calculateConfigChecksum(cfg)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfg.rpm[i];
    }

    for (byte i = 0; i < SPEED_POINT_COUNT; i++) {
      SPEED_TO_STEP[i] = cfg.speed[i];
    }

    rpmRed = cfg.rpmRed;
    rpmGreen = cfg.rpmGreen;
    rpmBlue = cfg.rpmBlue;
    rpmBrightness = cfg.rpmBrightness;
    rpmSingleRed = cfg.rpmSingleRed;
    rpmSingleGreen = cfg.rpmSingleGreen;
    rpmSingleBlue = cfg.rpmSingleBlue;
    rpmSingleBrightness = cfg.rpmSingleBrightness;

    kmRed = cfg.kmRed;
    kmGreen = cfg.kmGreen;
    kmBlue = cfg.kmBlue;
    kmBrightness = cfg.kmBrightness;
    kmSingleRed = cfg.kmSingleRed;
    kmSingleGreen = cfg.kmSingleGreen;
    kmSingleBlue = cfg.kmSingleBlue;
    kmSingleBrightness = cfg.kmSingleBrightness;

    rpmRangeEnabled = cfg.rpmRangeEnabled;
    rpmRangeStart = cfg.rpmRangeStart;
    rpmRangeEnd = cfg.rpmRangeEnd;
    rpmRangeRed = cfg.rpmRangeRed;
    rpmRangeGreen = cfg.rpmRangeGreen;
    rpmRangeBlue = cfg.rpmRangeBlue;

    kmRangeEnabled = cfg.kmRangeEnabled;
    kmRangeStart = cfg.kmRangeStart;
    kmRangeEnd = cfg.kmRangeEnd;
    kmRangeRed = cfg.kmRangeRed;
    kmRangeGreen = cfg.kmRangeGreen;
    kmRangeBlue = cfg.kmRangeBlue;

    shiftBlinkMode = cfg.shiftBlinkMode;
    shiftBlinkPeriodMs = cfg.shiftBlinkPeriodMs;
    if (shiftBlinkPeriodMs < 50) shiftBlinkPeriodMs = 50;
    if (shiftBlinkPeriodMs > 500) shiftBlinkPeriodMs = 500;
    shiftBlinkTarget = cfg.shiftBlinkTarget;
    shiftBlinkRed = cfg.shiftBlinkRed;
    shiftBlinkGreen = cfg.shiftBlinkGreen;
    shiftBlinkBlue = cfg.shiftBlinkBlue;

    return true;
  }

  GaugeConfigV10 cfgV10;
  EEPROM.get(0, cfgV10);

  if (cfgV10.magic == EEPROM_MAGIC &&
      cfgV10.version == 10 &&
      cfgV10.checksum == calculateConfigV10Checksum(cfgV10)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV10.rpm[i];
    }

    for (byte i = 0; i < SPEED_POINT_COUNT; i++) {
      SPEED_TO_STEP[i] = cfgV10.speed[i];
    }

    rpmRed = cfgV10.rpmRed;
    rpmGreen = cfgV10.rpmGreen;
    rpmBlue = cfgV10.rpmBlue;
    rpmBrightness = cfgV10.rpmBrightness;
    rpmSingleRed = cfgV10.rpmSingleRed;
    rpmSingleGreen = cfgV10.rpmSingleGreen;
    rpmSingleBlue = cfgV10.rpmSingleBlue;
    rpmSingleBrightness = cfgV10.rpmSingleBrightness;

    kmRed = cfgV10.kmRed;
    kmGreen = cfgV10.kmGreen;
    kmBlue = cfgV10.kmBlue;
    kmBrightness = cfgV10.kmBrightness;
    kmSingleRed = cfgV10.kmSingleRed;
    kmSingleGreen = cfgV10.kmSingleGreen;
    kmSingleBlue = cfgV10.kmSingleBlue;
    kmSingleBrightness = cfgV10.kmSingleBrightness;

    rpmRangeEnabled = cfgV10.rpmRangeEnabled;
    rpmRangeStart = cfgV10.rpmRangeStart;
    rpmRangeEnd = cfgV10.rpmRangeEnd;
    rpmRangeRed = cfgV10.rpmRangeRed;
    rpmRangeGreen = cfgV10.rpmRangeGreen;
    rpmRangeBlue = cfgV10.rpmRangeBlue;

    kmRangeEnabled = cfgV10.kmRangeEnabled;
    kmRangeStart = cfgV10.kmRangeStart;
    kmRangeEnd = cfgV10.kmRangeEnd;
    kmRangeRed = cfgV10.kmRangeRed;
    kmRangeGreen = cfgV10.kmRangeGreen;
    kmRangeBlue = cfgV10.kmRangeBlue;

    shiftBlinkMode = cfgV10.shiftBlinkMode;
    shiftBlinkPeriodMs = cfgV10.shiftBlinkPeriodMs;
    if (shiftBlinkPeriodMs < 50) shiftBlinkPeriodMs = 50;
    if (shiftBlinkPeriodMs > 500) shiftBlinkPeriodMs = 500;
    shiftBlinkTarget = cfgV10.shiftBlinkTarget;
    shiftBlinkRed = 255;
    shiftBlinkGreen = 0;
    shiftBlinkBlue = 0;

    return true;
  }

  GaugeConfigV9 cfgV9;
  EEPROM.get(0, cfgV9);

  if (cfgV9.magic == EEPROM_MAGIC &&
      cfgV9.version == 9 &&
      cfgV9.checksum == calculateConfigV9Checksum(cfgV9)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV9.rpm[i];
    }

    for (byte i = 0; i < SPEED_POINT_COUNT; i++) {
      SPEED_TO_STEP[i] = cfgV9.speed[i];
    }

    rpmRed = cfgV9.rpmRed;
    rpmGreen = cfgV9.rpmGreen;
    rpmBlue = cfgV9.rpmBlue;
    rpmBrightness = cfgV9.rpmBrightness;
    rpmSingleRed = cfgV9.rpmSingleRed;
    rpmSingleGreen = cfgV9.rpmSingleGreen;
    rpmSingleBlue = cfgV9.rpmSingleBlue;
    rpmSingleBrightness = cfgV9.rpmSingleBrightness;

    kmRed = cfgV9.kmRed;
    kmGreen = cfgV9.kmGreen;
    kmBlue = cfgV9.kmBlue;
    kmBrightness = cfgV9.kmBrightness;
    kmSingleRed = cfgV9.kmSingleRed;
    kmSingleGreen = cfgV9.kmSingleGreen;
    kmSingleBlue = cfgV9.kmSingleBlue;
    kmSingleBrightness = cfgV9.kmSingleBrightness;

    return true;
  }

  GaugeConfigV8 cfgV8;
  EEPROM.get(0, cfgV8);

  if (cfgV8.magic == EEPROM_MAGIC &&
      cfgV8.version == 8 &&
      cfgV8.checksum == calculateConfigV8Checksum(cfgV8)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV8.rpm[i];
    }

    applyLegacySpeedPoints(cfgV8.speed);

    rpmRed = cfgV8.rpmRed;
    rpmGreen = cfgV8.rpmGreen;
    rpmBlue = cfgV8.rpmBlue;
    rpmBrightness = cfgV8.rpmBrightness;
    rpmSingleRed = cfgV8.rpmSingleRed;
    rpmSingleGreen = cfgV8.rpmSingleGreen;
    rpmSingleBlue = cfgV8.rpmSingleBlue;
    rpmSingleBrightness = cfgV8.rpmSingleBrightness;

    kmRed = cfgV8.kmRed;
    kmGreen = cfgV8.kmGreen;
    kmBlue = cfgV8.kmBlue;
    kmBrightness = cfgV8.kmBrightness;
    kmSingleRed = cfgV8.kmSingleRed;
    kmSingleGreen = cfgV8.kmSingleGreen;
    kmSingleBlue = cfgV8.kmSingleBlue;
    kmSingleBrightness = cfgV8.kmSingleBrightness;

    return true;
  }

  GaugeConfigV7 cfgV7;
  EEPROM.get(0, cfgV7);

  if (cfgV7.magic == EEPROM_MAGIC &&
      cfgV7.version == 7 &&
      cfgV7.checksum == calculateConfigV7Checksum(cfgV7)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV7.rpm[i];
    }

    applyLegacySpeedPoints(cfgV7.speed);

    rpmRed = cfgV7.rpmRed;
    rpmGreen = cfgV7.rpmGreen;
    rpmBlue = cfgV7.rpmBlue;
    rpmBrightness = cfgV7.rpmBrightness;
    kmRed = cfgV7.kmRed;
    kmGreen = cfgV7.kmGreen;
    kmBlue = cfgV7.kmBlue;
    kmBrightness = cfgV7.kmBrightness;
    applyLegacySharedSingle(cfgV7.singleRed, cfgV7.singleGreen, cfgV7.singleBlue, cfgV7.singleBrightness);
    return true;
  }

  GaugeConfigV6 cfgV6;
  EEPROM.get(0, cfgV6);

  if (cfgV6.magic == EEPROM_MAGIC &&
      cfgV6.version == 6 &&
      cfgV6.checksum == calculateConfigV6Checksum(cfgV6)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV6.rpm[i];
    }

    applyLegacySpeedPoints(cfgV6.speed);

    applyLedFromLegacyStart(cfgV6.rpmStartRed, cfgV6.rpmStartGreen, cfgV6.rpmStartBlue, cfgV6.rpmBrightness);
    kmRed = cfgV6.kmRed;
    kmGreen = cfgV6.kmGreen;
    kmBlue = cfgV6.kmBlue;
    kmBrightness = cfgV6.kmBrightness;
    applyLegacySharedSingle(cfgV6.singleRed, cfgV6.singleGreen, cfgV6.singleBlue, cfgV6.singleBrightness);
    return true;
  }

  GaugeConfigV5 cfgV5;
  EEPROM.get(0, cfgV5);

  if (cfgV5.magic == EEPROM_MAGIC &&
      cfgV5.version == 5 &&
      cfgV5.checksum == calculateConfigV5Checksum(cfgV5)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV5.rpm[i];
    }

    applyLegacySpeedPoints(cfgV5.speed);

    applyLedFromLegacyStart(cfgV5.rpmStartRed, cfgV5.rpmStartGreen, cfgV5.rpmStartBlue, cfgV5.rpmBrightness);

    kmRed = cfgV5.kmRed;
    kmGreen = cfgV5.kmGreen;
    kmBlue = cfgV5.kmBlue;
    kmBrightness = cfgV5.kmBrightness;

    applyLegacySharedSingle(cfgV5.singleRed, cfgV5.singleGreen, cfgV5.singleBlue, cfgV5.singleBrightness);

    return true;
  }

  GaugeConfigV4 cfgV4;
  EEPROM.get(0, cfgV4);

  if (cfgV4.magic == EEPROM_MAGIC &&
      cfgV4.version == 4 &&
      cfgV4.checksum == calculateConfigV4Checksum(cfgV4)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV4.rpm[i];
    }

    applyLegacySpeedPoints(cfgV4.speed);

    applyLedFromLegacyStart(cfgV4.rpmStartRed, cfgV4.rpmStartGreen, cfgV4.rpmStartBlue, cfgV4.rpmBrightness);

    kmRed = cfgV4.kmRed;
    kmGreen = cfgV4.kmGreen;
    kmBlue = cfgV4.kmBlue;
    kmBrightness = cfgV4.kmBrightness;

    applyLegacySharedSingle(cfgV4.singleRed, cfgV4.singleGreen, cfgV4.singleBlue, cfgV4.singleBrightness);

    return true;
  }

  GaugeConfigV3 cfgV3;
  EEPROM.get(0, cfgV3);

  if (cfgV3.magic == EEPROM_MAGIC &&
      cfgV3.version == 3 &&
      cfgV3.checksum == calculateConfigV3Checksum(cfgV3)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV3.rpm[i];
    }

    applyLegacySpeedPoints(cfgV3.speed);

    applyLedFromLegacyStart(cfgV3.rpmStartRed, cfgV3.rpmStartGreen, cfgV3.rpmStartBlue, cfgV3.rpmBrightness);

    kmRed = cfgV3.kmRed;
    kmGreen = cfgV3.kmGreen;
    kmBlue = cfgV3.kmBlue;
    kmBrightness = cfgV3.kmBrightness;

    applyLegacySharedSingle(cfgV3.kmRed, cfgV3.kmGreen, cfgV3.kmBlue, cfgV3.kmBrightness);

    return true;
  }

  GaugeConfigV2 cfgV2;
  EEPROM.get(0, cfgV2);

  if (cfgV2.magic == EEPROM_MAGIC &&
      cfgV2.version == 2 &&
      cfgV2.checksum == calculateConfigV2Checksum(cfgV2)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = cfgV2.rpm[i];
    }

    applyLegacySpeedPoints(cfgV2.speed);

    applyLedFromLegacyStart(cfgV2.ringRed, cfgV2.ringGreen, cfgV2.ringBlue, cfgV2.ringBrightness);

    kmRed = cfgV2.ringRed;
    kmGreen = cfgV2.ringGreen;
    kmBlue = cfgV2.ringBlue;
    kmBrightness = cfgV2.ringBrightness;

    applyLegacySharedSingle(cfgV2.singleRed, cfgV2.singleGreen, cfgV2.singleBlue, cfgV2.singleBrightness);

    return true;
  }

  GaugeConfigV1 oldCfg;
  EEPROM.get(0, oldCfg);

  if (oldCfg.magic == EEPROM_MAGIC &&
      oldCfg.version == 1 &&
      oldCfg.checksum == calculateConfigV1Checksum(oldCfg)) {
    for (byte i = 0; i < 13; i++) {
      RPM_TICK_TO_STEP[i] = oldCfg.rpm[i];
    }

    applyLegacySpeedPoints(oldCfg.speed);

    loadDefaultLedConfig();
    return true;
  }

  loadDefaultConfig();
  return false;
}

void dumpConfig() {
  for (byte i = 0; i < SPEED_POINT_COUNT; i++) {
    Serial.print(F("CS,"));
    Serial.print(i);
    Serial.print(F(","));
    Serial.println(SPEED_TO_STEP[i]);
  }

  for (byte i = 0; i < 13; i++) {
    Serial.print(F("CR,"));
    Serial.print(i);
    Serial.print(F(","));
    Serial.println(RPM_TICK_TO_STEP[i]);
  }

  Serial.print(F("LR,"));
  Serial.print(rpmRed);
  Serial.print(F(","));
  Serial.print(rpmGreen);
  Serial.print(F(","));
  Serial.print(rpmBlue);
  Serial.print(F(","));
  Serial.println(rpmBrightness);

  Serial.print(F("RS,"));
  Serial.print(rpmSingleRed);
  Serial.print(F(","));
  Serial.print(rpmSingleGreen);
  Serial.print(F(","));
  Serial.print(rpmSingleBlue);
  Serial.print(F(","));
  Serial.println(rpmSingleBrightness);

  Serial.print(F("LK,"));
  Serial.print(kmRed);
  Serial.print(F(","));
  Serial.print(kmGreen);
  Serial.print(F(","));
  Serial.print(kmBlue);
  Serial.print(F(","));
  Serial.println(kmBrightness);

  Serial.print(F("KS,"));
  Serial.print(kmSingleRed);
  Serial.print(F(","));
  Serial.print(kmSingleGreen);
  Serial.print(F(","));
  Serial.print(kmSingleBlue);
  Serial.print(F(","));
  Serial.println(kmSingleBrightness);

  Serial.print(F("LRZ,"));
  Serial.print(rpmRangeStart);
  Serial.print(F(","));
  Serial.print(rpmRangeEnd);
  Serial.print(F(","));
  Serial.print(rpmRangeRed);
  Serial.print(F(","));
  Serial.print(rpmRangeGreen);
  Serial.print(F(","));
  Serial.print(rpmRangeBlue);
  Serial.print(F(","));
  Serial.println(rpmRangeEnabled);

  Serial.print(F("LKZ,"));
  Serial.print(kmRangeStart);
  Serial.print(F(","));
  Serial.print(kmRangeEnd);
  Serial.print(F(","));
  Serial.print(kmRangeRed);
  Serial.print(F(","));
  Serial.print(kmRangeGreen);
  Serial.print(F(","));
  Serial.print(kmRangeBlue);
  Serial.print(F(","));
  Serial.println(kmRangeEnabled);

  Serial.print(F("SB,"));
  Serial.print(shiftBlinkMode);
  Serial.print(F(","));
  Serial.print(shiftBlinkPeriodMs);
  Serial.print(F(","));
  Serial.print(shiftBlinkTarget);
  Serial.print(F(","));
  Serial.print(shiftBlinkRed);
  Serial.print(F(","));
  Serial.print(shiftBlinkGreen);
  Serial.print(F(","));
  Serial.println(shiftBlinkBlue);

  Serial.println(F("END"));
}

// ============================================================
// PWM / 모터
// ============================================================

void setupPwmFrequency() {
  // D9, D10 PWM 주파수 올림
  TCCR1B = (TCCR1B & 0b11111000) | 0x01;

  // D11도 같이 올림
  TCCR2B = (TCCR2B & 0b11111000) | 0x01;
}

void moveMotorTo(int step) {
  // 부드러운 이동용
  if (step < 0) step = 0;
  if (step > MAX_GAUGE_STEP) step = MAX_GAUGE_STEP;

  int current = gaugeMotor.getPosition();

  if (step == current) {
    commandedStep = step;
    return;
  }

  signed char dir = (step > current) ? 1 : -1;

  gaugeMotor.setPosition(step, dir);
  commandedStep = step;
}

void moveFastMotorTo(int step) {
  // 빠른 반응용
  if (step < 0) step = 0;
  if (step > MOTOR_FAST_STEPS - 1) step = MOTOR_FAST_STEPS - 1;

  int current = gaugeFast.getPosition();

  if (step == current) {
    return;
  }

  signed char dir = (step > current) ? 1 : -1;

  gaugeFast.setPosition(step, dir);
}

int microStepToFastStep(int microStep) {
  // 보정표는 microstep 기준이라 fast 모드에서 변환해서 씀
  if (microStep < 0) microStep = 0;
  if (microStep > MAX_GAUGE_STEP) microStep = MAX_GAUGE_STEP;

  return (int)((microStep * (long)(MOTOR_FAST_STEPS - 1)) / MAX_GAUGE_STEP);
}

void startupSweepCalibration() {
  // 꺼질 때 0점으로 보냈다는 전제로 시작
  // 스토퍼에 더 밀어붙이면 소리가 나서 강제 홈은 안 함
  gaugeFast.setZero();
  gaugeMotor.setZero();

  // 전원 켰을 때 계기판처럼 한 번 쓸고 돌아옴
  for (int i = 0; i < FAST_SWEEP_STEPS; i++) {
    gaugeFast.stepUp();
    delayMicroseconds(FAST_SWEEP_STEP_US);
  }
  delay(80);
  for (int i = 0; i < FAST_SWEEP_STEPS; i++) {
    gaugeFast.stepDown();
    delayMicroseconds(FAST_SWEEP_STEP_US);
  }

  gaugeFast.powerOff();
  gaugeFast.setZero();
  gaugeMotor.setZero();
  commandedStep = 0;
}

void noteHostLine() {
  lastHostLineMs = millis();
}

void startGaugeFromHost() {
  // 심허브 RD. 스윕이 끝나는 시각을 연결 시각으로 잡는다.
  showRpmLedSolid();
  startupSweepCalibration();

  while (Serial.available() > 0) {
    Serial.read();
  }
  rxLen = 0;

  gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
  gaugeMotor.setMaxSpeed(SPEED_MOTOR_MAX_SPEED);
  activeGaugeMode = GAUGE_MODE_RPM;
  gaugeReady = true;
  gaugeHoming = false;
  shiftSignalOn = false;
  telemetryTimedOut = false;
  manualHoldMode = false;
  lastTelemetryMs = millis();
  lastHostLineMs = millis();
}

bool activeMotorParkedAtZero() {
  if (activeGaugeMode == GAUGE_MODE_RPM) {
    return !gaugeFast.isRunning() && gaugeFast.getPosition() == 0;
  }
  return !gaugeMotor.isRunning() && gaugeMotor.getPosition() == 0;
}

void commandActiveMotorHome() {
  if (activeGaugeMode == GAUGE_MODE_RPM) {
    gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
    moveFastMotorTo(0);
  } else {
    gaugeMotor.setMaxSpeed(SPEED_MOTOR_MAX_SPEED);
    moveMotorTo(0);
  }
}

void shutdownGaugeFromHost() {
  gaugeFast.powerOff();
  gaugeMotor.powerOff();
  shiftSignalOn = false;
  shiftBlinkLit = true;
  gaugeReady = false;
  gaugeHoming = false;
  telemetryTimedOut = true;
  manualHoldMode = false;
  turnGaugeLedsOff();
}

void checkHostSilence() {
  // 보정 명령으로 잡고 있는 동안은 심허브 종료로 보지 않음
  if (!gaugeReady) return;
  if (manualHoldMode) return;

  if (millis() - lastHostLineMs < HOST_SILENCE_MS) return;

  if (!gaugeHoming) {
    gaugeHoming = true;
    shiftSignalOn = false;
    pendingRpmLed = false;
    pendingKmLed = false;
    commandActiveMotorHome();
  }

  if (activeMotorParkedAtZero()) {
    shutdownGaugeFromHost();
  }
}

// ============================================================
// 값 변환
// ============================================================

int rpmPercentToStep(long percentPermille) {
  // R613 → 6130 RPM, R1200 → 12000 RPM (percentPermille × 10)
  if (percentPermille < 0) percentPermille = 0;
  if (percentPermille > 1200) percentPermille = 1200;

  long targetRpm = percentPermille * 10L;

  if (targetRpm <= 0) {
    return RPM_TICK_TO_STEP[0];
  }

  if (targetRpm >= RPM_DISPLAY_MAX) {
    return RPM_TICK_TO_STEP[RPM_CALIB_POINT_COUNT - 1];
  }

  for (byte i = 0; i < RPM_CALIB_POINT_COUNT - 1; i++) {
    long rpmA = (long)i * 1000L;
    long rpmB = (long)(i + 1) * 1000L;

    if (targetRpm >= rpmA && targetRpm <= rpmB) {
      int stepA = RPM_TICK_TO_STEP[i];
      int stepB = RPM_TICK_TO_STEP[i + 1];

      long span = rpmB - rpmA;
      long offset = targetRpm - rpmA;

      long step = stepA + ((long)(stepB - stepA) * offset) / span;

      if (step < 0) step = 0;
      if (step > MAX_GAUGE_STEP) step = MAX_GAUGE_STEP;

      return (int)step;
    }
  }

  return RPM_TICK_TO_STEP[RPM_CALIB_POINT_COUNT - 1];
}

int rpmPercentToFastStep(long percentPermille) {
  // SimHub에서 넘어온 RPM(/10)을 빠른 모터 위치로 변환
  if (percentPermille < 0) percentPermille = 0;
  if (percentPermille > 1200) percentPermille = 1200;

  return microStepToFastStep(rpmPercentToStep(percentPermille));
}

int speedDeciKmhToStep(long kmh10) {
  // K값은 km/h * 10. K1000이면 100.0km/h
  long minKmh10 = (long)SPEED_KMH[0] * 10L;
  long maxKmh10 = (long)SPEED_KMH[SPEED_POINT_COUNT - 1] * 10L;

  if (kmh10 <= minKmh10) {
    return SPEED_TO_STEP[0];
  }

  if (kmh10 >= maxKmh10) {
    return SPEED_TO_STEP[SPEED_POINT_COUNT - 1];
  }

  for (byte i = 0; i < SPEED_POINT_COUNT - 1; i++) {
    long kmhA10 = (long)SPEED_KMH[i] * 10L;
    long kmhB10 = (long)SPEED_KMH[i + 1] * 10L;

    if (kmh10 >= kmhA10 && kmh10 <= kmhB10) {
      int stepA = SPEED_TO_STEP[i];
      int stepB = SPEED_TO_STEP[i + 1];

      long span = kmhB10 - kmhA10;
      long offset = kmh10 - kmhA10;

      long step = stepA + ((long)(stepB - stepA) * offset) / span;

      if (step < 0) step = 0;
      if (step > MAX_GAUGE_STEP) step = MAX_GAUGE_STEP;

      return (int)step;
    }
  }

  return 0;
}

// ============================================================
// 시리얼 파서
// ============================================================

bool isDigitChar(char c) {
  return c >= '0' && c <= '9';
}

char toUpperAscii(char c) {
  if (c >= 'a' && c <= 'z') {
    return c - 32;
  }

  return c;
}

bool equalsIgnoreCase(const char* value, const char* expected) {
  while (*value && *expected) {
    if (toUpperAscii(*value) != toUpperAscii(*expected)) {
      return false;
    }

    value++;
    expected++;
  }

  return *value == '\0' && *expected == '\0';
}

bool parseLongValue(const char* p, long& out) {
  while (*p == ' ' || *p == '\t' || *p == ':' || *p == '=') {
    p++;
  }

  bool negative = false;
  if (*p == '-') {
    negative = true;
    p++;
  }

  bool found = false;
  long value = 0;

  while (isDigitChar(*p)) {
    found = true;
    value = value * 10 + (*p - '0');
    p++;
  }

  if (!found) return false;

  out = negative ? -value : value;
  return true;
}

bool parseNextLong(const char*& p, long& out) {
  while (*p == ' ' || *p == '\t' || *p == ',' || *p == ':' || *p == '=') {
    p++;
  }

  bool negative = false;
  if (*p == '-') {
    negative = true;
    p++;
  }

  bool found = false;
  long value = 0;

  while (isDigitChar(*p)) {
    found = true;
    value = value * 10 + (*p - '0');
    p++;
  }

  if (!found) return false;

  out = negative ? -value : value;
  return true;
}

void markTelemetryReceived() {
  // 정상 데이터가 들어오면 끊김 상태 해제
  lastTelemetryMs = millis();
  telemetryTimedOut = false;
  manualHoldMode = false;
}

void previewSpeedPoint(byte index) {
  long kmh10 = (long)SPEED_KMH[index] * 10L;
  int microStep = speedDeciKmhToStep(kmh10);

  manualHoldMode = true;
  telemetryTimedOut = false;

  if (USE_FAST_FOR_SPEED) {
    activeGaugeMode = GAUGE_MODE_RPM;

    gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
    moveFastMotorTo(microStepToFastStep(microStep));
  } else {
    activeGaugeMode = GAUGE_MODE_SPEED;

    gaugeMotor.setMaxSpeed(MANUAL_MOTOR_MAX_SPEED);
    moveMotorTo(microStep);
  }
}

void previewRpmPoint(byte index) {
  // index 12 (12000 RPM) → R1200, index 6 (6000 RPM) → R600
  long percentPermille = (long)index * 100L;
  if (percentPermille > 1200) percentPermille = 1200;

  manualHoldMode = true;
  telemetryTimedOut = false;
  activeGaugeMode = GAUGE_MODE_RPM;

  gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
  moveFastMotorTo(rpmPercentToFastStep(percentPermille));
}

void handleCalSpeed(const char* args) {
  long index = 0;
  long step = 0;
  const char* p = args;

  if (!parseNextLong(p, index) || !parseNextLong(p, step) ||
      index < 0 || index >= SPEED_POINT_COUNT) {
    Serial.println(F("ERR"));
    return;
  }

  if (step < 0) step = 0;
  if (step > MAX_GAUGE_STEP) step = MAX_GAUGE_STEP;

  SPEED_TO_STEP[index] = (int)step;
  previewSpeedPoint((byte)index);
  Serial.println(F("OK"));
}

void handleCalRpm(const char* args) {
  long index = 0;
  long step = 0;
  const char* p = args;

  if (!parseNextLong(p, index) || !parseNextLong(p, step) ||
      index < 0 || index >= 13) {
    Serial.println(F("ERR"));
    return;
  }

  if (step < 0) step = 0;
  if (step > MAX_GAUGE_STEP) step = MAX_GAUGE_STEP;

  RPM_TICK_TO_STEP[index] = (int)step;
  previewRpmPoint((byte)index);
  Serial.println(F("OK"));
}

uint8_t clampByteValue(long value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t)value;
}

void handleLedSetting(const char* args, byte ledMode) {
  long red = 0;
  long green = 0;
  long blue = 0;
  long brightness = 0;
  const char* p = args;

  if (!parseNextLong(p, red) ||
      !parseNextLong(p, green) ||
      !parseNextLong(p, blue) ||
      !parseNextLong(p, brightness)) {
    Serial.println(F("ERR"));
    return;
  }

  uint8_t r = clampByteValue(red);
  uint8_t g = clampByteValue(green);
  uint8_t b = clampByteValue(blue);
  uint8_t br = clampByteValue(brightness);

  // 0=RPM ring, 1=KM ring, 2=RPM single, 3=KM single, 4=legacy LS(both singles)
  if (ledMode == 0) {
    rpmRed = r;
    rpmGreen = g;
    rpmBlue = b;
    rpmBrightness = br;

    manualHoldMode = true;
    telemetryTimedOut = false;
    invalidateRingLed();
    showRpmLedSolid();
  } else if (ledMode == 1) {
    kmRed = r;
    kmGreen = g;
    kmBlue = b;
    kmBrightness = br;

    manualHoldMode = true;
    telemetryTimedOut = false;
    invalidateRingLed();
    showKmLedSolid();
  } else if (ledMode == 2) {
    rpmSingleRed = r;
    rpmSingleGreen = g;
    rpmSingleBlue = b;
    rpmSingleBrightness = br;
    applyRpmSingleLed();
  } else if (ledMode == 3) {
    kmSingleRed = r;
    kmSingleGreen = g;
    kmSingleBlue = b;
    kmSingleBrightness = br;
    applyKmSingleLed();
  } else {
    applyLegacySharedSingle(r, g, b, br);
    applyRpmSingleLed();
  }

  Serial.println(F("OK"));
}

void handleLedRange(const char* args, bool rpmMode) {
  long startIdx = 0;
  long endIdx = 0;
  long red = 0;
  long green = 0;
  long blue = 0;
  long enabled = 1;
  const char* p = args;

  if (!parseNextLong(p, startIdx) ||
      !parseNextLong(p, endIdx) ||
      !parseNextLong(p, red) ||
      !parseNextLong(p, green) ||
      !parseNextLong(p, blue)) {
    Serial.println(F("ERR"));
    return;
  }

  parseNextLong(p, enabled);

  if (startIdx < 0) startIdx = 0;
  if (endIdx < 0) endIdx = 0;
  if (startIdx > LED_RING_COUNT - 1) startIdx = LED_RING_COUNT - 1;
  if (endIdx > LED_RING_COUNT - 1) endIdx = LED_RING_COUNT - 1;
  if (startIdx > endIdx) enabled = 0;

  if (rpmMode) {
    rpmRangeStart = (uint8_t)startIdx;
    rpmRangeEnd = (uint8_t)endIdx;
    rpmRangeRed = clampByteValue(red);
    rpmRangeGreen = clampByteValue(green);
    rpmRangeBlue = clampByteValue(blue);
    rpmRangeEnabled = enabled ? 1 : 0;
    manualHoldMode = true;
    telemetryTimedOut = false;
    invalidateRingLed();
    showRpmLedSolid();
  } else {
    kmRangeStart = (uint8_t)startIdx;
    kmRangeEnd = (uint8_t)endIdx;
    kmRangeRed = clampByteValue(red);
    kmRangeGreen = clampByteValue(green);
    kmRangeBlue = clampByteValue(blue);
    kmRangeEnabled = enabled ? 1 : 0;
    manualHoldMode = true;
    telemetryTimedOut = false;
    invalidateRingLed();
    showKmLedSolid();
  }

  Serial.println(F("OK"));
}

void handleShiftBlinkSetting(const char* args) {
  long mode = 0;
  long period = 150;
  long target = 0;
  long red = 255;
  long green = 0;
  long blue = 0;
  const char* p = args;

  if (!parseNextLong(p, mode)) {
    Serial.println(F("ERR"));
    return;
  }

  parseNextLong(p, period);
  parseNextLong(p, target);
  parseNextLong(p, red);
  parseNextLong(p, green);
  parseNextLong(p, blue);

  if (mode < 0) mode = 0;
  if (mode > 2) mode = 2;
  if (period < 50) period = 50;
  if (period > 500) period = 500;
  if (target < 0) target = 0;
  if (target > 1) target = 1;

  shiftBlinkMode = (uint8_t)mode;
  shiftBlinkPeriodMs = (uint16_t)period;
  shiftBlinkTarget = (uint8_t)target;
  shiftBlinkRed = clampByteValue(red);
  shiftBlinkGreen = clampByteValue(green);
  shiftBlinkBlue = clampByteValue(blue);

  Serial.println(F("OK"));
}

void applyShiftSignal(bool on) {
  bool changed = (shiftSignalOn != on);

  if (on) {
    shiftSignalOn = true;
    if (changed) {
      shiftBlinkLit = true;
      lastShiftBlinkMs = millis();
      if (shiftBlinkMode == SHIFT_BLINK_RPM) activeGaugeMode = GAUGE_MODE_RPM;
      if (shiftBlinkMode == SHIFT_BLINK_KM) activeGaugeMode = GAUGE_MODE_SPEED;
      if (isShiftBlinkActive()) {
        showCurrentRing(false);
      }
    }
  } else if (changed) {
    shiftSignalOn = false;
    shiftBlinkLit = true;
    invalidateRingLed();
    if (activeGaugeMode == GAUGE_MODE_RPM) {
      pendingRpmLed = true;
      pendingKmLed = false;
      if (!manualHoldMode) showRpmLedSolid();
    } else {
      pendingKmLed = true;
      pendingRpmLed = false;
      if (!manualHoldMode) showKmLedSolid();
    }
  }
}

void handleShiftSignal(bool on) {
  markTelemetryReceived();
  applyShiftSignal(on);
  Serial.println(F("OK"));
}

bool parseTelemetryValueAndOptionalShift(const char* args, long& value, bool& hasShift, bool& shiftOn) {
  const char* p = args;
  if (!parseNextLong(p, value)) return false;

  long shiftFlag = 0;
  hasShift = parseNextLong(p, shiftFlag);
  shiftOn = hasShift && (shiftFlag != 0);
  return true;
}

void parkNeedleToZero() {
  // 종료나 리부트 전에 바늘부터 0으로 보냄
  manualHoldMode = true;
  telemetryTimedOut = false;

  gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
  gaugeMotor.setMaxSpeed(MANUAL_MOTOR_MAX_SPEED);

  moveFastMotorTo(0);
  moveMotorTo(0);

  unsigned long startMs = millis();
  while (millis() - startMs < 2500UL) {
    gaugeFast.update();
    gaugeMotor.update();
  }

  gaugeFast.powerOff();
  gaugeMotor.powerOff();
  gaugeFast.setZero();
  gaugeMotor.setZero();
  commandedStep = 0;
  manualHoldMode = false;
  telemetryTimedOut = true;
  invalidateRingLed();
  showKmLedSolid();
}

void rebootArduino() {
  parkNeedleToZero();

  Serial.println(F("OK"));
  Serial.flush();
  delay(50);

  void (*resetFunc)(void) = 0;
  resetFunc();
}

void processLine(char* line) {
  if (line[0] == '\0') return;

  if ((line[0] == 'C' || line[0] == 'c') &&
      (line[1] == 'S' || line[1] == 's')) {
    handleCalSpeed(line + 2);
    return;
  }

  if ((line[0] == 'C' || line[0] == 'c') &&
      (line[1] == 'R' || line[1] == 'r')) {
    handleCalRpm(line + 2);
    return;
  }

  if ((line[0] == 'L' || line[0] == 'l') &&
      (line[1] == 'R' || line[1] == 'r') &&
      (line[2] == 'Z' || line[2] == 'z')) {
    handleLedRange(line + 3, true);
    return;
  }

  if ((line[0] == 'L' || line[0] == 'l') &&
      (line[1] == 'K' || line[1] == 'k') &&
      (line[2] == 'Z' || line[2] == 'z')) {
    handleLedRange(line + 3, false);
    return;
  }

  if ((line[0] == 'L' || line[0] == 'l') &&
      (line[1] == 'R' || line[1] == 'r')) {
    handleLedSetting(line + 2, 0);
    return;
  }

  if ((line[0] == 'L' || line[0] == 'l') &&
      (line[1] == 'K' || line[1] == 'k')) {
    handleLedSetting(line + 2, 1);
    return;
  }

  if ((line[0] == 'S' || line[0] == 's') &&
      (line[1] == 'B' || line[1] == 'b')) {
    handleShiftBlinkSetting(line + 2);
    return;
  }

  if ((line[0] == 'G' || line[0] == 'g') &&
      (line[1] == '1') && line[2] == '\0') {
    handleShiftSignal(true);
    return;
  }

  if ((line[0] == 'G' || line[0] == 'g') &&
      (line[1] == '0') && line[2] == '\0') {
    handleShiftSignal(false);
    return;
  }

  // RS / KS 는 R / K 텔레메트리보다 먼저 처리해야 함
  if ((line[0] == 'R' || line[0] == 'r') &&
      (line[1] == 'S' || line[1] == 's')) {
    handleLedSetting(line + 2, 2);
    return;
  }

  if ((line[0] == 'K' || line[0] == 'k') &&
      (line[1] == 'S' || line[1] == 's')) {
    handleLedSetting(line + 2, 3);
    return;
  }

  // 구버전 호환: LS 는 KM/RPM 싱글 공통값
  if ((line[0] == 'L' || line[0] == 'l') &&
      (line[1] == 'S' || line[1] == 's')) {
    handleLedSetting(line + 2, 4);
    return;
  }

  if (equalsIgnoreCase(line, "SAVE")) {
    saveConfigToEeprom();
    Serial.println(F("OK"));
    return;
  }

  if (equalsIgnoreCase(line, "LOAD")) {
    bool loaded = loadConfigFromEeprom();
    invalidateRingLed();
    showRpmLedSolid();
    Serial.println(loaded ? F("OK") : F("ERR"));
    return;
  }

  if (equalsIgnoreCase(line, "DUMP")) {
    dumpConfig();
    return;
  }

  if (equalsIgnoreCase(line, "RESETCFG")) {
    loadDefaultConfig();
    invalidateRingLed();
    showKmLedSolid();
    Serial.println(F("OK"));
    return;
  }

  if (equalsIgnoreCase(line, "PARK")) {
    parkNeedleToZero();
    Serial.println(F("OK"));
    return;
  }

  if (equalsIgnoreCase(line, "REBOOT")) {
    rebootArduino();
    return;
  }

  // 심허브 연결. 응답 없음. 이미 초기화된 RD는 연결 유지로만 본다.
  if (equalsIgnoreCase(line, "RD")) {
    noteHostLine();
    if (gaugeHoming) {
      gaugeHoming = false;
      pendingRpmLed = true;
      return;
    }
    if (gaugeReady) return;
    startGaugeFromHost();
    return;
  }

  // 텔레메트리. T613 또는 T613,1 (한 줄에 변속)
  // 정상본과 같이 여기선 모터만 움직임. LED는 loop에서 따로 갱신
  if (line[0] == 'R' || line[0] == 'r') {
    if (!gaugeReady) return;

    noteHostLine();
    gaugeHoming = false;

    long percentPermille = 0;
    bool hasShift = false;
    bool shiftOn = false;

    if (parseTelemetryValueAndOptionalShift(line + 1, percentPermille, hasShift, shiftOn)) {
      markTelemetryReceived();
      activeGaugeMode = GAUGE_MODE_RPM;

      gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
      moveFastMotorTo(rpmPercentToFastStep(percentPermille));

      pendingRpmLed = true;
      pendingKmLed = false;

      if (hasShift) applyShiftSignal(shiftOn);
    }

    return;
  }

  // 속도. K1000이면 100.0km/h. K1000,1 이면 변속 ON
  // 정상본과 같이 여기선 모터만 움직임. LED는 loop에서 따로 갱신
  if (line[0] == 'K' || line[0] == 'k') {
    if (!gaugeReady) return;

    noteHostLine();
    gaugeHoming = false;

    long kmh10 = 0;
    bool hasShift = false;
    bool shiftOn = false;

    if (parseTelemetryValueAndOptionalShift(line + 1, kmh10, hasShift, shiftOn)) {
      markTelemetryReceived();

      if (USE_FAST_FOR_SPEED) {
        activeGaugeMode = GAUGE_MODE_RPM;

        gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
        moveFastMotorTo(microStepToFastStep(speedDeciKmhToStep(kmh10)));
      } else {
        activeGaugeMode = GAUGE_MODE_SPEED;

        gaugeMotor.setMaxSpeed(SPEED_MOTOR_MAX_SPEED);
        moveMotorTo(speedDeciKmhToStep(kmh10));
      }

      pendingKmLed = true;
      pendingRpmLed = false;

      if (hasShift) applyShiftSignal(shiftOn);
    }

    return;
  }

  // 수동 위치 이동
  if (line[0] == 'S' || line[0] == 's') {
    long step = 0;

    if (parseLongValue(line + 1, step)) {
      manualHoldMode = true;
      telemetryTimedOut = false;

      if (USE_FAST_FOR_MANUAL) {
        activeGaugeMode = GAUGE_MODE_RPM;

        gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
        moveFastMotorTo((int)step);
      } else {
        activeGaugeMode = GAUGE_MODE_SPEED;

        gaugeMotor.setMaxSpeed(MANUAL_MOTOR_MAX_SPEED);
        moveMotorTo((int)step);
      }
    }

    return;
  }
}

void readSerialNonBlocking() {
  // 정상본처럼 들어온 문자는 바로 처리.
  // R/K는 모터만 움직이므로 LED show가 끼어들지 않음.
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      processLine(rxBuf);
      rxLen = 0;
      continue;
    }

    if (rxLen < RX_BUF_SIZE - 1) {
      rxBuf[rxLen++] = c;
    } else {
      rxLen = 0;
    }
  }
}

void updateLedsDeferred() {
  // 단색이므로 모드 전환/강제 갱신일 때만 show()
  if (manualHoldMode) return;
  if (isShiftBlinkActive()) return;

  if (pendingRpmLed && activeGaugeMode == GAUGE_MODE_RPM) {
    showRpmLedSolid();
    pendingRpmLed = false;
    return;
  }

  if (pendingKmLed && activeGaugeMode == GAUGE_MODE_SPEED) {
    showKmLedSolid();
    pendingKmLed = false;
  }
}

void checkTelemetryTimeout() {
  // 게임 데이터가 끊기면 바늘을 0으로 돌림
  // 심허브 연결 전, 종료 복귀 중에는 여기서 움직이지 않음
  if (!gaugeReady) return;
  if (gaugeHoming) return;
  if (manualHoldMode) return;
  if (shiftSignalOn) return;

  unsigned long now = millis();

  if (!telemetryTimedOut && now - lastTelemetryMs > TELEMETRY_TIMEOUT_MS) {
    if (activeGaugeMode == GAUGE_MODE_RPM) {
      moveFastMotorTo(0);
      pendingRpmLed = true;
      pendingKmLed = false;
    } else {
      moveMotorTo(0);
      pendingKmLed = true;
      pendingRpmLed = false;
    }
    telemetryTimedOut = true;
  }
}

// ============================================================
// Arduino 시작점
// ============================================================

void setup() {
  setupPwmFrequency();
  loadConfigFromEeprom();
  setupLeds();

  Serial.begin(115200);

  gaugeFast.setMaxSpeed(RPM_MOTOR_MAX_SPEED);
  gaugeMotor.setMaxSpeed(SPEED_MOTOR_MAX_SPEED);

  while (Serial.available() > 0) {
    Serial.read();
  }

  gaugeReady = false;
  gaugeHoming = false;
  lastTelemetryMs = millis();
  lastHostLineMs = millis();
}

void loop() {
  gaugeFast.update();
  gaugeMotor.update();

  readSerialNonBlocking();
  checkTelemetryTimeout();
  checkHostSilence();
  updateLedsDeferred();
  updateShiftBlink();
}
