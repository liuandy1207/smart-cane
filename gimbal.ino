#include <Wire.h>

// ── LEDC Servo ────────────────────────────────────────────────
#define SERVO_PIN    18
#define LEDC_CHANNEL 0
#define LEDC_FREQ    50
#define LEDC_RES     16

void servoSetup() {
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(SERVO_PIN, LEDC_CHANNEL);
}
void servoWrite(int deg) {
  deg = constrain(deg, 0, 180);
  ledcWrite(LEDC_CHANNEL, map(deg, 0, 180, 3277, 6554));
}

// ── MPU-6050 ──────────────────────────────────────────────────
#define MPU_ADDR 0x68
#define SDA_PIN  21
#define SCL_PIN  22

// ── State ─────────────────────────────────────────────────────
float angle        = 0;
float targetAngle  = 0;   // set at startup — this is "home"
float lastError    = 0;
float gyroBias     = 0;
unsigned long lastTime;
unsigned long lastResetTime;

// ── Tuning ────────────────────────────────────────────────────
float Kp = 2.0;
float Kd = 0.05;

// ── Raw read helper ───────────────────────────────────────────
struct RawIMU { int16_t ax, ay, az, gy; bool ok; };

RawIMU readIMU() {
  RawIMU d = {0,0,0,0,false};
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  if (Wire.requestFrom(MPU_ADDR, 14, true) < 14) return d;
  d.ax = Wire.read() << 8 | Wire.read();
  d.ay = Wire.read() << 8 | Wire.read();
  d.az = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();                     // temp
  Wire.read(); Wire.read();                     // gx
  d.gy = Wire.read() << 8 | Wire.read();        // pitch gyro
  Wire.read(); Wire.read();                     // gz
  d.ok = true;
  return d;
}

// ── Gyro bias calibration ─────────────────────────────────────
void calibrateGyro() {
  Serial.println("=== Keep still — calibrating... ===");
  long sum = 0;
  for (int i = 0; i < 300; i++) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x45);                           // GY register
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 2, true);
    sum += (int16_t)(Wire.read() << 8 | Wire.read());
    delay(3);
  }
  gyroBias = sum / 300.0;
  Serial.print("Gyro bias: "); Serial.println(gyroBias);
}

// ── Read angle from accelerometer only (for seeding) ─────────
float accelAngleNow() {
  RawIMU d = readIMU();
  if (!d.ok) return 0;
  // Using ax/az — change to ay/az if your axis finder showed ay moves
  return atan2((float)d.ax, (float)d.az) * 180.0 / PI;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // Reset MPU
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x80);
  Wire.endTransmission();
  delay(100);

  // Wake MPU
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission();
  delay(100);

  // Sanity check
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1, true);
  byte id = Wire.read();
  Serial.print("WHO_AM_I: 0x"); Serial.println(id, HEX);
  if (id != 0x68) {
    Serial.println("MPU not found — check wiring. Halting.");
    while (true) delay(1000);
  }

  calibrateGyro();

  // ── Capture home position ─────────────────────────────────
  // Average 50 accel readings for a stable seed
  float sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += accelAngleNow();
    delay(10);
  }
  targetAngle = sum / 50.0;
  angle       = targetAngle;  // start filter at home too

  Serial.print("Home angle set to: "); Serial.println(targetAngle);
  Serial.println("=== Running ===");

  servoSetup();
  servoWrite(90);

  lastTime      = micros();
  lastResetTime = millis();
}

void loop() {
  unsigned long now = micros();
  float dt = (now - lastTime) / 1000000.0;
  lastTime = now;
  if (dt <= 0 || dt > 0.1) return;

  RawIMU d = readIMU();
  if (!d.ok) {
    Serial.println("I2C fail");
    return;
  }

  float accelAngle = atan2((float)d.ax, (float)d.az) * 180.0 / PI;
  float gyroRate   = (d.gy - gyroBias) / 131.0;

  // Complementary filter
  angle = 0.96 * (angle + gyroRate * dt) + 0.04 * accelAngle;

  // Soft drift correction every 5s
  if (millis() - lastResetTime > 5000) {
    if (abs(angle - accelAngle) > 5.0) {
      Serial.print("Drift snap: "); Serial.print(angle);
      Serial.print(" -> "); Serial.println(accelAngle);
      angle = 0.5 * angle + 0.5 * accelAngle;
    }
    lastResetTime = millis();
  }

  // ── Error relative to HOME, not 0° ───────────────────────
  float error      = targetAngle - angle;
  float derivative = (error - lastError) / dt;
  lastError        = error;
  float output     = Kp * error + Kd * derivative;

  int servoPos = 90 + constrain((int)output, -90, 90);
  servoWrite(servoPos);

  // Debug
  Serial.print("home=");   Serial.print(targetAngle, 1);
  Serial.print("  angle="); Serial.print(angle, 1);
  Serial.print("  err=");   Serial.print(error, 1);
  Serial.print("  out=");   Serial.print(output, 1);
  Serial.print("  servo="); Serial.println(servoPos);

  delay(10);
}
