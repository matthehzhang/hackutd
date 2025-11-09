#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_PWMServoDriver.h>

// ---------- YOUR MATRIX SETUP ----------
#define PIN_DATA       D10
#define PANELS         4
#define PANEL_W        8
#define PANEL_H        8
#define SERPENTINE     true
#define LAYOUT_HORIZ   true
#define START_BRIGHT   4
// --------------------------------------

#define PER_PANEL      (PANEL_W * PANEL_H)
#define NUMPIXELS      (PER_PANEL * PANELS)

Adafruit_NeoPixel leds(NUMPIXELS, PIN_DATA, NEO_GRB + NEO_KHZ800);

// ---------- Display Modes ----------
enum DisplayMode {
  MODE_GRASS,
  MODE_ATTACK1,
  MODE_STONE_WALL,
  MODE_FEATHERS,
  MODE_HOUSE,
  MODE_OFF,
  MODE_MEDIUMATTACK1,
  MODE_MEDIUMATTACK2,
  MODE_MEDIUMATTACK3,
  MODE_MEDIUMATTACK4,
  MODE_MEDIUMATTACK5
};

DisplayMode currentMode = MODE_GRASS;
DisplayMode lastMode    = MODE_OFF;

// ---------- Panel Rotation Setup ----------
const int8_t panelRotation[PANELS] = {90, 0, 0, -90};

// ---------- One-shot animation state ----------
struct OneShot { bool active = false; uint32_t t0 = 0; };
OneShot mediumShot;
OneShot attack1Shot;

// ---------- Arrow helpers ----------
enum ArrowDir { ARROW_RIGHT, ARROW_LEFT, ARROW_UP, ARROW_DOWN };

// ---------- PCA9685 SERVO SETUP ----------
#define SDA_PIN        D4
#define SCL_PIN        D5
#define PCA_ADDR       0x40
#define SERVO_FREQ     50

#define SERVO_MIN_US   500    // Wider range for full motion
#define SERVO_MAX_US   2500

Adafruit_PWMServoDriver pca(PCA_ADDR);
bool pcaReady = false;

// ========== SERVO CALIBRATION VALUES ==========
// >>> CALIBRATE THESE USING S0/S1 COMMANDS <

// Physical starting positions
#define RIGHT_START_ANGLE  30
#define LEFT_START_ANGLE   150

// Target angles for FLAT PLANE (0° physical on both)
#define RIGHT_FLAT_ANGLE   90
#define LEFT_FLAT_ANGLE    90

// >>> CALIBRATE THESE FOR 90° PHYSICAL (PERPENDICULAR) <
#define RIGHT_90_ANGLE     180
#define LEFT_90_ANGLE      0

// ==============================================

// Physical servo channels
#define SERVO_RIGHT    0
#define SERVO_LEFT     1

// Servo state
struct ServoState {
  float currentAngle;
  float targetAngle;
  float startAngle;
  bool initialized;
};

ServoState servos[2];

const float SERVO_SPEED = 1.5f;

// Wing flap animation state
uint32_t wingFlapStartTime = 0;
bool wingFlapping = false;

// Stone wall shake state
uint32_t lastShakeTime = 0;

uint16_t angleToUs(float angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  return SERVO_MIN_US + (uint16_t)((angle / 180.0f) * (SERVO_MAX_US - SERVO_MIN_US));
}

void setServoAngleDirect(uint8_t channel, float angle) {
  if (!pcaReady) return;
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  
  uint16_t us = angleToUs(angle);
  uint16_t ticks = (us * 4096UL) / 20000UL;
  pca.setPWM(channel, 0, ticks);
}

void setServoAngle(uint8_t channel, float angle) {
  if (!pcaReady) return;
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  
  uint16_t us = angleToUs(angle);
  uint16_t ticks = (us * 4096UL) / 20000UL;
  pca.setPWM(channel, 0, ticks);
  
  Serial.print("Servo ch"); Serial.print(channel);
  Serial.print(": "); Serial.print(angle);
  Serial.print("° ("); Serial.print(us); Serial.println("µs)");
}

void setServoTarget(uint8_t servoIndex, float angle) {
  if (servoIndex < 2) {
    servos[servoIndex].targetAngle = angle;
  }
}

void updateServos() {
  if (!pcaReady) return;
  
  for (uint8_t i = 0; i < 2; i++) {
    if (!servos[i].initialized) continue;
    
    float current = servos[i].currentAngle;
    float target = servos[i].targetAngle;
    
    float diff = target - current;
    
    if (abs(diff) > 0.5f) {
      float step = (diff > 0) ? SERVO_SPEED : -SERVO_SPEED;
      
      if (abs(step) > abs(diff)) {
        step = diff;
      }
      
      servos[i].currentAngle += step;
      
      uint8_t channel = (i == 0) ? SERVO_RIGHT : SERVO_LEFT;
      setServoAngleDirect(channel, servos[i].currentAngle);
    }
  }
}

void updateServosStoneWall() {
  if (!pcaReady) return;
  
  uint32_t now = millis();
  
  if (now - lastShakeTime > random(30, 80)) {
    lastShakeTime = now;
    
    float shakeRight = RIGHT_90_ANGLE + random(-8, 9);
    float shakeLeft = LEFT_90_ANGLE + random(-8, 9);
    
    setServoAngleDirect(SERVO_RIGHT, shakeRight);
    setServoAngleDirect(SERVO_LEFT, shakeLeft);
    
    servos[0].currentAngle = shakeRight;
    servos[1].currentAngle = shakeLeft;
  }
}

// Update servos for FEATHERS (mirrored wing flapping around 0° center)
void updateServosFeathers() {
  if (!pcaReady) return;
  
  if (!wingFlapping) {
    wingFlapping = true;
    wingFlapStartTime = millis();
  }
  
  uint32_t elapsed = millis() - wingFlapStartTime;
  
  // Wing flap cycle: 1200ms total
  // Upstroke (slow): -30° to +70° (100° range) - 800ms
  // Downstroke (fast): +70° to -30° - 400ms
  const uint16_t CYCLE_MS = 1200;
  const uint16_t UPSTROKE_MS = 800;
  
  uint32_t cyclePos = elapsed % CYCLE_MS;
  
  float angleRight, angleLeft;
  
  if (cyclePos < UPSTROKE_MS) {
    // Upstroke - slow movement: wings rise from -30° to +70° (around flat center)
    float progress = (float)cyclePos / (float)UPSTROKE_MS;
    
    // Right wing: flat - 30 → flat + 70
    angleRight = (RIGHT_FLAT_ANGLE - 30) + (progress * 100);
    
    // Left wing MIRRORS: flat + 30 → flat - 70
    angleLeft = (LEFT_FLAT_ANGLE + 30) - (progress * 100);
    
  } else {
    // Downstroke - fast movement: wings drop from +70° to -30°
    float progress = (float)(cyclePos - UPSTROKE_MS) / (float)(CYCLE_MS - UPSTROKE_MS);
    
    // Right wing: flat + 70 → flat - 30
    angleRight = (RIGHT_FLAT_ANGLE + 70) - (progress * 100);
    
    // Left wing MIRRORS: flat - 70 → flat + 30
    angleLeft = (LEFT_FLAT_ANGLE - 70) + (progress * 100);
  }
  
  // Clamp to valid servo range
  if (angleRight < 0) angleRight = 0;
  if (angleRight > 180) angleRight = 180;
  if (angleLeft < 0) angleLeft = 0;
  if (angleLeft > 180) angleLeft = 180;
  
  setServoAngleDirect(SERVO_RIGHT, angleRight);
  setServoAngleDirect(SERVO_LEFT, angleLeft);
  
  servos[0].currentAngle = angleRight;
  servos[1].currentAngle = angleLeft;
}

void setServoTargetsForMode(DisplayMode mode) {
  wingFlapping = false;
  
  switch(mode) {
    case MODE_GRASS:
      setServoTarget(0, RIGHT_FLAT_ANGLE);
      setServoTarget(1, LEFT_FLAT_ANGLE);
      Serial.println("Servo targets: GRASS (flat plane)");
      break;
      
    case MODE_ATTACK1:
      setServoTarget(0, RIGHT_FLAT_ANGLE);
      setServoTarget(1, LEFT_FLAT_ANGLE);
      Serial.println("Servo targets: ATTACK1 (flat)");
      break;
      
    case MODE_STONE_WALL:
      setServoTarget(0, RIGHT_90_ANGLE);
      setServoTarget(1, LEFT_90_ANGLE);
      lastShakeTime = millis();
      Serial.println("Servo targets: STONE_WALL (shaking at 90°)");
      break;
      
    case MODE_FEATHERS:
      Serial.println("Servo targets: FEATHERS (wing flapping -30° to +70°)");
      break;
      
    case MODE_HOUSE:
      // Same smooth transition as other modes
      setServoTarget(0, RIGHT_90_ANGLE);
      setServoTarget(1, LEFT_90_ANGLE);
      Serial.println("Servo targets: HOUSE (90° smooth)");
      break;
      
    case MODE_OFF:
      setServoTarget(0, RIGHT_FLAT_ANGLE);
      setServoTarget(1, LEFT_FLAT_ANGLE);
      Serial.println("Servo targets: OFF (flat)");
      break;
      
    case MODE_MEDIUMATTACK1:
    case MODE_MEDIUMATTACK2:
    case MODE_MEDIUMATTACK3:
    case MODE_MEDIUMATTACK4:
    case MODE_MEDIUMATTACK5:
      setServoTarget(0, RIGHT_90_ANGLE);
      setServoTarget(1, LEFT_90_ANGLE);
      Serial.println("Servo targets: MEDIUM ATTACK (90°)");
      break;
      
    default:
      break;
  }
}

// ---------- Mapping helpers with rotation ----------
static inline void applyRotation(uint8_t panelNum, uint8_t &lx, uint8_t &ly) {
  int8_t rot = panelRotation[panelNum];
  uint8_t tempX = lx, tempY = ly;
  if (rot == 90)      { lx = (PANEL_W - 1) - tempY; ly = tempX; }
  else if (rot == -90){ lx = tempY;                 ly = (PANEL_H - 1) - tempX; }
}

static inline uint16_t indexInPanel(uint8_t panelNum, uint8_t lx, uint8_t ly) {
  applyRotation(panelNum, lx, ly);
  if (SERPENTINE && (ly & 1)) return ly * PANEL_W + (PANEL_W - 1 - lx);
  return ly * PANEL_W + lx;
}

static inline uint16_t mapXY(uint16_t gx, uint16_t gy) {
  uint8_t p, lx, ly;
  if (LAYOUT_HORIZ) { p = gx / PANEL_W; lx = gx % PANEL_W; ly = gy; }
  else              { p = gy / PANEL_H; lx = gx;            ly = gy % PANEL_H; }
  if (p >= PANELS) return 0;
  return p * PER_PANEL + indexInPanel(p, lx, ly);
}

static inline void setPixel(uint16_t gx, uint16_t gy, uint8_t r, uint8_t g, uint8_t b) {
  if (LAYOUT_HORIZ) { if (gx >= PANELS * PANEL_W || gy >= PANEL_H) return; }
  else              { if (gx >= PANEL_W || gy >= PANELS * PANEL_H) return; }
  leds.setPixelColor(mapXY(gx, gy), leds.Color(r, g, b));
}

static inline void setPanelPixel(uint8_t panelNum, uint8_t lx, uint8_t ly, uint8_t r, uint8_t g, uint8_t b) {
  if (panelNum >= PANELS || lx >= PANEL_W || ly >= PANEL_H) return;
  uint16_t idx = panelNum * PER_PANEL + indexInPanel(panelNum, lx, ly);
  leds.setPixelColor(idx, leds.Color(r, g, b));
}

static inline void fillAll(uint8_t r, uint8_t g, uint8_t b) {
  for (uint16_t i = 0; i < NUMPIXELS; i++) leds.setPixelColor(i, leds.Color(r, g, b));
}

uint16_t GW() { return LAYOUT_HORIZ ? PANELS * PANEL_W : PANEL_W; }
uint16_t GH() { return LAYOUT_HORIZ ? PANEL_H : PANELS * PANEL_H; }

// ---------- Grass animation variables ----------
uint8_t  sparkle[NUMPIXELS];
uint16_t pixHash[NUMPIXELS];

const uint8_t  GRASS_G_BASE   = 50;
const uint8_t  GRASS_G_RANGE  = 22;
const uint8_t  GRASS_R_TINT   = 8;
const uint8_t  GRASS_B_TINT   = 3;
const float    SWAY_SPEED     = 0.00025f;
const float    SWAY_SCALE     = 0.21f;
const float    WIND_SPEED     = 0.00012f;
const float    WIND_SPATIAL   = 0.36f;
const uint8_t  SPARK_SPAWN_PER_FRAME = 2;
const uint8_t  SPARK_INTENSITY       = 220;
const uint8_t  SPARK_FADE_STEP       = 8;

static inline void addSparkleRGB(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t s) {
  uint16_t rr = r + (uint16_t)(s * 0.9f);
  uint16_t gg = g + (uint16_t)(s * 0.95f);
  uint16_t bb = b + (uint16_t)(s * 0.02f);
  r = rr > 255 ? 255 : rr; g = gg > 255 ? 255 : gg; b = bb > 255 ? 255 : bb;
}

// ---------- Stone Wall variables ----------
uint8_t grayBackground[NUMPIXELS];
bool    wallInitialized = false;
uint8_t grayInitMin = 110, grayInitMax = 170;

// ---------- House variables ----------
bool houseDrawn = false;

// ---------- Arrow math ----------
static inline void rotateForDir(uint8_t x, uint8_t y, ArrowDir dir, uint8_t &rx, uint8_t &ry) {
  switch (dir) {
    case ARROW_RIGHT: rx = x;                    ry = y;                    break;
    case ARROW_LEFT:  rx = (PANEL_W - 1) - x;    ry = (PANEL_H - 1) - y;    break;
    case ARROW_UP:    rx = y;                    ry = (PANEL_W - 1) - x;    break;
    case ARROW_DOWN:  rx = (PANEL_H - 1) - y;    ry = x;                    break;
  }
}

static void drawArrowStep(uint8_t panelNum, ArrowDir dir, uint8_t step,
                          uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t maxShaftX = (step <= 5) ? step : 5;
  for (uint8_t x = 1; x <= maxShaftX; x++) {
    for (uint8_t y = 3; y <= 4; y++) {
      uint8_t rx, ry; rotateForDir(x, y, dir, rx, ry);
      setPanelPixel(panelNum, rx, ry, r, g, b);
    }
  }

  if (step >= 6) {
    for (uint8_t y = 3; y <= 4; y++) {
      uint8_t rx, ry; rotateForDir(6, y, dir, rx, ry);
      setPanelPixel(panelNum, rx, ry, r, g, b);
    }
  }
  if (step >= 7) {
    uint8_t rx, ry;
    rotateForDir(6, 2, dir, rx, ry); setPanelPixel(panelNum, rx, ry, r, g, b);
    rotateForDir(6, 5, dir, rx, ry); setPanelPixel(panelNum, rx, ry, r, g, b);
    rotateForDir(7, 3, dir, rx, ry); setPanelPixel(panelNum, rx, ry, r, g, b);
    rotateForDir(7, 4, dir, rx, ry); setPanelPixel(panelNum, rx, ry, r, g, b);
  }
}

// ---------- MEDIUM ATTACK ----------
static void drawMediumAttackCommon(const ArrowDir dirForPanel[]) {
  const uint16_t REVEAL_MS = 900;
  const uint16_t HOLD_MS   = 250;

  if (!mediumShot.active) { mediumShot.active = true; mediumShot.t0 = millis(); }
  uint32_t dt = millis() - mediumShot.t0;

  uint8_t step = 7;
  if (dt < REVEAL_MS) {
    step = (uint8_t)((dt * 8UL) / REVEAL_MS);
    if (step > 7) step = 7;
  }

  fillAll(0, 0, 0);
  const uint8_t AR = 220, AG = 255, AB = 60;

  for (uint8_t p = 0; p < PANELS; p++) drawArrowStep(p, dirForPanel[p], step, AR, AG, AB);
  leds.show();

  if (dt >= (REVEAL_MS + HOLD_MS)) { mediumShot.active = false; currentMode = MODE_OFF; }
}

void drawMediumAttack1() { const ArrowDir d[PANELS]={ARROW_RIGHT,ARROW_RIGHT,ARROW_RIGHT,ARROW_RIGHT}; drawMediumAttackCommon(d); }
void drawMediumAttack2() { const ArrowDir d[PANELS]={ARROW_LEFT, ARROW_LEFT, ARROW_LEFT, ARROW_LEFT }; drawMediumAttackCommon(d); }
void drawMediumAttack3() { const ArrowDir d[PANELS]={ARROW_UP,   ARROW_UP,   ARROW_UP,   ARROW_UP   }; drawMediumAttackCommon(d); }
void drawMediumAttack4() { const ArrowDir d[PANELS]={ARROW_DOWN, ARROW_DOWN, ARROW_DOWN, ARROW_DOWN }; drawMediumAttackCommon(d); }
void drawMediumAttack5() { const ArrowDir d[PANELS]={ARROW_RIGHT,ARROW_RIGHT,ARROW_LEFT, ARROW_LEFT }; drawMediumAttackCommon(d); }

// ---------- FORWARD DECLS ----------
void drawWindowOnPanel(uint8_t panelNum);
void drawDoorOnPanel(uint8_t panelNum);

// ========== SERIAL COMMAND HANDLER ==========
void checkSerialCommands() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command.length() == 0) return;

  Serial.print("Received: "); Serial.println(command);

  if (command == "GRASS") { currentMode = MODE_GRASS; }
  else if (command == "ATTACK1") { currentMode = MODE_ATTACK1; attack1Shot.active = false; }
  else if (command == "STONE_WALL") { currentMode = MODE_STONE_WALL; wallInitialized = false; }
  else if (command == "FEATHERS") { currentMode = MODE_FEATHERS; }
  else if (command == "HOUSE") { currentMode = MODE_HOUSE; houseDrawn = false; }
  else if (command == "OFF") { currentMode = MODE_OFF; }
  else if (command == "MEDIUMATTACK1") { currentMode = MODE_MEDIUMATTACK1; mediumShot.active = false; }
  else if (command == "MEDIUMATTACK2") { currentMode = MODE_MEDIUMATTACK2; mediumShot.active = false; }
  else if (command == "MEDIUMATTACK3") { currentMode = MODE_MEDIUMATTACK3; mediumShot.active = false; }
  else if (command == "MEDIUMATTACK4") { currentMode = MODE_MEDIUMATTACK4; mediumShot.active = false; }
  else if (command == "MEDIUMATTACK5") { currentMode = MODE_MEDIUMATTACK5; mediumShot.active = false; }
  
  else if (command.startsWith("S0 ")) {
    float angle = command.substring(3).toFloat();
    setServoTarget(0, angle);
    Serial.print("Set RIGHT servo target: "); Serial.println(angle);
  }
  else if (command.startsWith("S1 ")) {
    float angle = command.substring(3).toFloat();
    setServoTarget(1, angle);
    Serial.print("Set LEFT servo target: "); Serial.println(angle);
  }
  else if (command == "SPOS") {
    Serial.println("=== SERVO POSITIONS ===");
    Serial.print("RIGHT (ch0): current="); Serial.print(servos[0].currentAngle); 
    Serial.print("° target="); Serial.print(servos[0].targetAngle); Serial.println("°");
    Serial.print("LEFT (ch1):  current="); Serial.print(servos[1].currentAngle);
    Serial.print("° target="); Serial.print(servos[1].targetAngle); Serial.println("°");
  }
  else {
    Serial.println("-> Unknown command");
  }
}

// ========== MODE: GRASS ==========
void drawGrass() {
  uint32_t t = millis();
  uint16_t gw = GW(), gh = GH();

  for (uint8_t k = 0; k < SPARK_SPAWN_PER_FRAME; k++) {
    uint16_t idx = random(NUMPIXELS);
    if (sparkle[idx] == 0) sparkle[idx] = SPARK_INTENSITY;
  }

  for (uint16_t y = 0; y < gh; y++) {
    for (uint16_t x = 0; x < gw; x++) {
      uint16_t idx = mapXY(x, y);
      float localPhase = (float)t * SWAY_SPEED + (float)pixHash[idx] * SWAY_SCALE * 0.0001f;
      float windPhase  = (float)t * WIND_SPEED + x * WIND_SPATIAL * 0.25f;
      float s = 0.5f + 0.5f * sinf(localPhase + sinf(windPhase) * 0.9f);
      int16_t g = GRASS_G_BASE + (int16_t)(s * GRASS_G_RANGE) + (int16_t)((gh - 1 - y) * 1);
      if (g < 0) g = 0; if (g > 255) g = 255;
      uint8_t r = GRASS_R_TINT, b = GRASS_B_TINT, g8 = (uint8_t)g;
      uint8_t sp = sparkle[idx];
      if (sp) { addSparkleRGB(r, g8, b, sp); sparkle[idx] = (sp >= SPARK_FADE_STEP) ? (sp - SPARK_FADE_STEP) : 0; }
      setPixel(x, y, r, g8, b);
    }
  }
  leds.show();
}

// ========== MODE: ATTACK1 ==========
void drawAttack1() {
  static const uint16_t dur[8] = {260, 480, 260, 150, 120, 100, 70, 60};
  const uint16_t TOTAL_MS = 1500;

  if (!attack1Shot.active) { attack1Shot.active = true; attack1Shot.t0 = millis(); }
  uint32_t dt = millis() - attack1Shot.t0;

  if (dt >= TOTAL_MS) { attack1Shot.active = false; currentMode = MODE_OFF; return; }

  uint32_t acc = 0; int col = 0;
  for (; col < (int)PANEL_W; col++) { acc += dur[col]; if (dt < acc) break; }
  if (col >= (int)PANEL_W) col = PANEL_W - 1;

  fillAll(0, 0, 0);
  for (uint8_t panel = 0; panel < PANELS; panel++)
    for (uint8_t y = 0; y < PANEL_H; y++)
      setPanelPixel(panel, (uint8_t)col, y, 255, 0, 0);
  leds.show();
}

// ========== MODE: STONE_WALL ==========
void drawStoneWall() {
  uint16_t gw = GW(), gh = GH();
  uint32_t t  = millis();

  const float SHIM_SPEED = 0.0015f; const uint8_t SHIM_AMPL = 6;

  if (!wallInitialized) {
    for (uint16_t i = 0; i < NUMPIXELS; i++) grayBackground[i] = random(grayInitMin, grayInitMax + 1);
    wallInitialized = true;
  }

  for (uint16_t y = 0; y < gh; y++) {
    for (uint16_t x = 0; x < gw; x++) {
      uint16_t idx = mapXY(x, y);
      uint8_t  base = grayBackground[idx];
      float phase = (float)t * SHIM_SPEED + (float)pixHash[idx] * 0.00025f;
      int16_t g = (int16_t)base + (int16_t)(sinf(phase) * SHIM_AMPL);
      if (g < 0) g = 0; if (g > 255) g = 255;
      setPixel(x, y, (uint8_t)g, (uint8_t)g, (uint8_t)g);
    }
  }

  const uint8_t BAND_R = 52, BAND_G = 36, BAND_B = 16;
  const uint8_t RAIL_R = 58, RAIL_G = 42, RAIL_B = 18;
  const uint8_t TIE_R  = 46, TIE_G  = 34, TIE_B  = 14;

  float bandShift = (t / 80.0f);
  for (uint16_t x = 0; x < gw; x++) {
    uint8_t p = x / PANEL_W;
    if (p == 0 || p == 3) {
      int phase = (int)(x - bandShift);
      int mod = ((phase % 10) + 10) % 10;
      if (mod < 2) for (uint16_t y = 0; y < GH(); y++) setPixel(x, y, BAND_R, BAND_G, BAND_B);
    }
  }

  const uint8_t leftPanel = 1, rightPanel = 2;
  const uint16_t trackStartX = leftPanel * PANEL_W;
  const uint16_t trackEndX   = (rightPanel + 1) * PANEL_W - 1;
  const uint8_t railY1 = 2, railY2 = 5;
  const uint8_t tieSpacing = 3;
  float tieShift = (t / 100.0f);

  for (uint16_t x = trackStartX; x <= trackEndX; x++) {
    setPixel(x, railY1, RAIL_R, RAIL_G, RAIL_B);
    setPixel(x, railY2, RAIL_R, RAIL_G, RAIL_B);
  }
  for (uint16_t x = trackStartX; x <= trackEndX; x++) {
    int phase = (int)(x - tieShift);
    int mod = ((phase % tieSpacing) + tieSpacing) % tieSpacing;
    if (mod == 0) {
      int8_t y0 = (int8_t)railY1 - 1, y1 = (int8_t)railY2 + 1;
      if (y0 < 0) y0 = 0; if (y1 >= (int8_t)PANEL_H) y1 = PANEL_H - 1;
      for (int8_t y = y0; y <= y1; y++) setPixel(x, (uint8_t)y, TIE_R, TIE_G, TIE_B);
    }
  }

  leds.show();
}

// ========== MODE: FEATHERS (dark purple with blue grain) ==========
void drawFeathers() {
  uint16_t gw = GW(), gh = GH();
  uint32_t t = millis();
  
  // Dark purple base colors
  const uint8_t PURPLE_R_BASE = 45;   // Dark purple red component
  const uint8_t PURPLE_G_BASE = 20;   // Dark purple green component
  const uint8_t PURPLE_B_BASE = 70;   // Dark purple blue component
  
  // Blue grain variation
  const uint8_t BLUE_GRAIN_AMPL = 25;  // How much blue varies
  
  for (uint16_t y = 0; y < gh; y++) {
    for (uint16_t x = 0; x < gw; x++) {
      // Wave motion for texture
      float w1 = sin((x * 0.4f + t * 0.002f));
      float w2 = sin((x * 0.25f - t * 0.0015f + y * 0.5f));
      float combined = (w1 + w2) * 0.5f;  // -1 to 1
      
      // Add blue grain variation
      uint8_t r = PURPLE_R_BASE;
      uint8_t g = PURPLE_G_BASE;
      uint8_t b = PURPLE_B_BASE + (uint8_t)(combined * BLUE_GRAIN_AMPL + BLUE_GRAIN_AMPL);
      
      // Clamp blue
      if (b > 255) b = 255;
      
      setPixel(x, y, r, g, b);
    }
  }
  leds.show();
}

// ========== MODE: HOUSE ==========
void drawHouse() {
  if (!houseDrawn) {
    fillAll(60, 35, 15);
    drawWindowOnPanel(0);
    drawWindowOnPanel(1);
    drawDoorOnPanel(2);
    leds.show();
    houseDrawn = true;
  }
  leds.show();
}

void drawWindowOnPanel(uint8_t panelNum) {
  for (uint8_t ly = 1; ly < 6; ly++) for (uint8_t lx = 2; lx < 5; lx++) setPanelPixel(panelNum, lx, ly, 30, 80, 150);
  for (uint8_t lx = 1; lx < 6; lx++) { setPanelPixel(panelNum, lx, 0, 10, 30, 60); setPanelPixel(panelNum, lx, 6, 10, 30, 60); }
  for (uint8_t ly = 0; ly < 7; ly++) { setPanelPixel(panelNum, 1, ly, 10, 30, 60); setPanelPixel(panelNum, 5, ly, 10, 30, 60); }
}

void drawDoorOnPanel(uint8_t panelNum) {
  for (uint8_t ly = 2; ly < 6; ly++) for (uint8_t lx = 0; lx < 7; lx++) setPanelPixel(panelNum, lx, ly, 40, 20, 5);
  for (uint8_t lx = 0; lx < 7; lx++) { setPanelPixel(panelNum, lx, 1, 20, 10, 2); setPanelPixel(panelNum, lx, 6, 20, 10, 2); }
  for (uint8_t ly = 1; ly < 7; ly++) setPanelPixel(panelNum, 7, ly, 20, 10, 2);
  setPanelPixel(panelNum, 6, 4, 200, 180, 50);
}

// ---------- I2C SCAN ----------
void scanI2C() {
  Serial.println("I2C scan...");
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x"); if (addr < 16) Serial.print('0'); Serial.println(addr, HEX);
      count++;
    }
  }
  if (!count) Serial.println("  No I2C devices found.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  leds.begin(); leds.setBrightness(START_BRIGHT); leds.clear(); leds.show();
  randomSeed((uint32_t)micros());
  for (uint16_t i = 0; i < NUMPIXELS; i++) {
    sparkle[i] = 0; uint32_t h = i * 2654435761UL; h ^= (h << 13); h ^= (h >> 17); h ^= (h << 5); pixHash[i] = (uint16_t)(h & 0xFFFF);
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  scanI2C();
  
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);
  pcaReady = true;
  
  Serial.println("\n=== SERVO INITIALIZATION ===");
  
  servos[0].startAngle = RIGHT_START_ANGLE;
  servos[0].currentAngle = RIGHT_START_ANGLE;
  servos[0].targetAngle = RIGHT_START_ANGLE;
  servos[0].initialized = true;
  setServoAngle(SERVO_RIGHT, servos[0].currentAngle);
  Serial.print("RIGHT servo (ch0): Start PWM="); Serial.print(servos[0].startAngle); Serial.println("°");
  
  servos[1].startAngle = LEFT_START_ANGLE;
  servos[1].currentAngle = LEFT_START_ANGLE;
  servos[1].targetAngle = LEFT_START_ANGLE;
  servos[1].initialized = true;
  setServoAngle(SERVO_LEFT, servos[1].currentAngle);
  Serial.print("LEFT servo (ch1):  Start PWM="); Serial.print(servos[1].startAngle); Serial.println("°");

  Serial.println("\nReady. Commands:");
  Serial.println("  GRASS, ATTACK1, STONE_WALL, FEATHERS, HOUSE, OFF");
  Serial.println("  MEDIUMATTACK1..5");
  Serial.println("  S0 <angle>, S1 <angle>, SPOS");
}

void loop() {
  checkSerialCommands();
  
  if (currentMode != lastMode) {
    setServoTargetsForMode(currentMode);
  }
  
  // HOUSE now uses smooth updateServos() like other modes
  if (currentMode == MODE_STONE_WALL) {
    updateServosStoneWall();
  } else if (currentMode == MODE_FEATHERS) {
    updateServosFeathers();
  } else {
    updateServos();  // House uses this - smooth transition!
  }

  switch(currentMode) {
    case MODE_GRASS:       drawGrass();       delay(28); break;
    case MODE_ATTACK1:     drawAttack1();                 break;
    case MODE_STONE_WALL:  drawStoneWall();   delay(20);  break;
    case MODE_FEATHERS:    drawFeathers();    delay(40);  break;
    case MODE_HOUSE:       drawHouse();       delay(100); break;
    case MODE_OFF:         leds.clear(); leds.show(); delay(100); break;
    case MODE_MEDIUMATTACK1: drawMediumAttack1(); break;
    case MODE_MEDIUMATTACK2: drawMediumAttack2(); break;
    case MODE_MEDIUMATTACK3: drawMediumAttack3(); break;
    case MODE_MEDIUMATTACK4: drawMediumAttack4(); break;
    case MODE_MEDIUMATTACK5: drawMediumAttack5(); break;
  }

  lastMode = currentMode;
}