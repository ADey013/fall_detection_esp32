#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// ESP32-C3 FALL DETECTION SYSTEM
// MPU6050 + SSD1306 OLED + LED + BUZZER + BUTTON
//
// PINS:
// MPU6050 SDA -> GPIO 4
// MPU6050 SCL -> GPIO 5
// OLED SDA    -> GPIO 4
// OLED SCL    -> GPIO 5
// LED         -> GPIO 3
// BUZZER      -> GPIO 6
// BUTTON      -> GPIO 7
// =====================================================


// ================= PIN CONFIGURATION =================

#define SDA_PIN 4
#define SCL_PIN 5

#define LED_PIN 3
#define BUZZER_PIN 6
#define RESET_BUTTON 7


// ================= I2C ADDRESSES =================

#define MPU6050_ADDR 0x68
#define OLED_ADDR 0x3C


// ================= MPU6050 REGISTERS =================

#define PWR_MGMT_1 0x6B
#define ACCEL_CONFIG 0x1C
#define GYRO_CONFIG 0x1B
#define ACCEL_XOUT_H 0x3B


// ================= OLED =================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// =====================================================
// FALL DETECTION SETTINGS
// BALANCED SETTINGS
// =====================================================

const float IMPACT_LEVEL = 1.80;
const float GYRO_CHANGE = 150.0;
const float ANGLE_CHANGE = 25.0;

const unsigned long FALL_WINDOW = 1000;


// ================= LED SETTINGS =================

const unsigned long BLINK_INTERVAL = 250;


// ================= BUZZER SETTINGS =================

const unsigned long BUZZER_INTERVAL = 50;
const int BUZZER_FREQUENCY = 4000;


// =====================================================
// SENSOR VARIABLES
// =====================================================

float ax = 0;
float ay = 0;
float az = 0;

float gx = 0;
float gy = 0;
float gz = 0;

float acceleration = 0;
float angleX = 0;
float angleY = 0;


// =====================================================
// REFERENCE ORIENTATION
// =====================================================

float referenceAngleX = 0;
float referenceAngleY = 0;


// =====================================================
// FALL STATES
// =====================================================

bool possibleFall = false;
bool impactDetected = false;
bool movementDetected = false;
bool fallDetected = false;


// =====================================================
// TIMERS
// =====================================================

unsigned long possibleFallTime = 0;

unsigned long lastBlinkTime = 0;
unsigned long lastBuzzerTime = 0;


// =====================================================
// OUTPUT STATES
// =====================================================

bool ledState = false;
bool buzzerState = false;


// =====================================================
// WRITE MPU6050
// =====================================================

void writeMPU6050(byte reg, byte data)
{
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}


// =====================================================
// READ MPU6050
// =====================================================

bool readMPU6050()
{
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);

  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }

  Wire.requestFrom(MPU6050_ADDR, 14);

  if (Wire.available() < 14)
  {
    return false;
  }


  int16_t rawAx =
    (Wire.read() << 8) | Wire.read();

  int16_t rawAy =
    (Wire.read() << 8) | Wire.read();

  int16_t rawAz =
    (Wire.read() << 8) | Wire.read();


  // Skip temperature
  Wire.read();
  Wire.read();


  int16_t rawGx =
    (Wire.read() << 8) | Wire.read();

  int16_t rawGy =
    (Wire.read() << 8) | Wire.read();

  int16_t rawGz =
    (Wire.read() << 8) | Wire.read();


  // Accelerometer ±4g
  ax = rawAx / 8192.0;
  ay = rawAy / 8192.0;
  az = rawAz / 8192.0;


  // Gyroscope ±500 deg/sec
  gx = rawGx / 65.5;
  gy = rawGy / 65.5;
  gz = rawGz / 65.5;


  return true;
}


// =====================================================
// TOTAL ACCELERATION
// =====================================================

float getAcceleration()
{
  return sqrt(
    ax * ax +
    ay * ay +
    az * az
  );
}


// =====================================================
// CALCULATE ANGLES
// =====================================================

float getAngleX()
{
  return atan2(
    ay,
    sqrt(ax * ax + az * az)
  ) * 180.0 / PI;
}


float getAngleY()
{
  return atan2(
    ax,
    sqrt(ay * ay + az * az)
  ) * 180.0 / PI;
}


// =====================================================
// CHECK MPU6050
// =====================================================

bool checkMPU6050()
{
  Wire.beginTransmission(MPU6050_ADDR);

  return Wire.endTransmission() == 0;
}


// =====================================================
// OLED - MONITORING
// =====================================================

void showMonitoring()
{
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  const char *text = "MONITORING";

  int16_t x1;
  int16_t y1;

  uint16_t textWidth;
  uint16_t textHeight;


  display.getTextBounds(
    text,
    0,
    0,
    &x1,
    &y1,
    &textWidth,
    &textHeight
  );


  int x =
    (SCREEN_WIDTH - textWidth) / 2;

  int y =
    (SCREEN_HEIGHT - textHeight) / 2;


  display.setCursor(x, y);

  display.print(text);

  display.display();
}


// =====================================================
// OLED - FALL DETECTED
// =====================================================

void showFallDetected()
{
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);


  display.setCursor(35, 10);
  display.println("FALL");


  display.setCursor(10, 38);
  display.println("DETECTED!");


  display.display();
}


// =====================================================
// START ALARM
// =====================================================

void startAlarm()
{
  fallDetected = true;

  ledState = false;
  buzzerState = false;


  lastBlinkTime = millis();
  lastBuzzerTime = millis();


  digitalWrite(
    LED_PIN,
    LOW
  );


  ledcWriteTone(
    BUZZER_PIN,
    0
  );


  showFallDetected();


  Serial.println();
  Serial.println("============================");
  Serial.println("FALL DETECTED!");
  Serial.println("============================");
}


// =====================================================
// STOP ALARM
// =====================================================

void stopAlarm()
{
  fallDetected = false;

  possibleFall = false;

  impactDetected = false;

  movementDetected = false;


  ledState = false;

  digitalWrite(
    LED_PIN,
    LOW
  );


  buzzerState = false;

  ledcWriteTone(
    BUZZER_PIN,
    0
  );


  showMonitoring();


  Serial.println(
    "ALARM STOPPED - MONITORING"
  );
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(500);


  // LED
  pinMode(
    LED_PIN,
    OUTPUT
  );

  digitalWrite(
    LED_PIN,
    LOW
  );


  // RESET BUTTON
  pinMode(
    RESET_BUTTON,
    INPUT_PULLUP
  );


  // BUZZER PWM
  ledcAttach(
    BUZZER_PIN,
    BUZZER_FREQUENCY,
    8
  );

  ledcWriteTone(
    BUZZER_PIN,
    0
  );


  // I2C
  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  delay(200);


  // OLED
  if (
    display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  )
  {
    showMonitoring();

    Serial.println(
      "OLED READY"
    );
  }
  else
  {
    Serial.println(
      "OLED NOT FOUND"
    );
  }


  // MPU6050
  if (!checkMPU6050())
  {
    Serial.println(
      "MPU6050 NOT FOUND"
    );

    while (true)
    {
      digitalWrite(
        LED_PIN,
        HIGH
      );

      delay(100);


      digitalWrite(
        LED_PIN,
        LOW
      );

      delay(100);
    }
  }


  Serial.println(
    "MPU6050 DETECTED"
  );


  // Wake MPU6050
  writeMPU6050(
    PWR_MGMT_1,
    0x00
  );

  delay(100);


  // Accelerometer ±4g
  writeMPU6050(
    ACCEL_CONFIG,
    0x08
  );


  // Gyroscope ±500 deg/sec
  writeMPU6050(
    GYRO_CONFIG,
    0x08
  );

  delay(500);


  // Get initial orientation
  readMPU6050();

  angleX = getAngleX();
  angleY = getAngleY();

  referenceAngleX = angleX;
  referenceAngleY = angleY;


  Serial.println();
  Serial.println("============================");
  Serial.println("SYSTEM READY");
  Serial.println("MONITORING STARTED");
  Serial.println("============================");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{

  // ===================================================
  // RESET BUTTON
  // ===================================================

  if (
    fallDetected &&
    digitalRead(RESET_BUTTON) == LOW
  )
  {
    delay(30);


    if (
      digitalRead(RESET_BUTTON) == LOW
    )
    {
      stopAlarm();


      // Wait for button release
      while (
        digitalRead(RESET_BUTTON) == LOW
      )
      {
        delay(10);
      }


      delay(200);
    }
  }


  // ===================================================
  // ALARM MODE
  // ===================================================

  if (fallDetected)
  {

    // Rapid pulsing buzzer

    if (
      millis() -
      lastBuzzerTime >=
      BUZZER_INTERVAL
    )
    {
      lastBuzzerTime =
        millis();


      buzzerState =
        !buzzerState;


      if (buzzerState)
      {
        ledcWriteTone(
          BUZZER_PIN,
          BUZZER_FREQUENCY
        );
      }
      else
      {
        ledcWriteTone(
          BUZZER_PIN,
          0
        );
      }
    }


    // LED blinking

    if (
      millis() -
      lastBlinkTime >=
      BLINK_INTERVAL
    )
    {
      lastBlinkTime =
        millis();


      ledState =
        !ledState;


      digitalWrite(
        LED_PIN,
        ledState
      );
    }


    return;
  }


  // ===================================================
  // READ SENSOR
  // ===================================================

  if (!readMPU6050())
  {
    return;
  }


  acceleration =
    getAcceleration();


  angleX =
    getAngleX();


  angleY =
    getAngleY();


  float gyroMagnitude =
    sqrt(
      gx * gx +
      gy * gy +
      gz * gz
    );


  // ===================================================
  // SERIAL DEBUG
  // ===================================================

  static unsigned long lastPrint = 0;


  if (
    millis() -
    lastPrint >=
    500
  )
  {
    lastPrint =
      millis();


    Serial.print("ACC: ");

    Serial.print(
      acceleration,
      2
    );


    Serial.print(
      " g | GYRO: "
    );

    Serial.print(
      gyroMagnitude,
      0
    );


    Serial.print(
      " | X: "
    );

    Serial.print(
      angleX,
      1
    );


    Serial.print(
      " | Y: "
    );

    Serial.println(
      angleY,
      1
    );
  }


  // ===================================================
  // FALL DETECTION
  // ===================================================


  // ---------------------------------------------------
  // NORMAL STATE
  // ---------------------------------------------------

  if (!possibleFall)
  {

    // Continuously update normal orientation

    referenceAngleX =
      angleX;

    referenceAngleY =
      angleY;


    // Start possible fall when either strong impact
    // OR strong sudden movement occurs

    if (
      acceleration >= IMPACT_LEVEL ||
      gyroMagnitude >= GYRO_CHANGE
    )
    {

      possibleFall = true;

      possibleFallTime =
        millis();


      impactDetected = false;

      movementDetected = false;


      Serial.println(
        "POSSIBLE FALL MOVEMENT"
      );
    }
  }


  // ---------------------------------------------------
  // POSSIBLE FALL STATE
  // ---------------------------------------------------

  if (possibleFall)
  {

    // Strong impact

    if (
      acceleration >=
      IMPACT_LEVEL
    )
    {
      impactDetected =
        true;

      Serial.println(
        "IMPACT DETECTED"
      );
    }


    // Strong rotational movement

    if (
      gyroMagnitude >=
      GYRO_CHANGE
    )
    {
      movementDetected =
        true;

      Serial.println(
        "STRONG MOVEMENT DETECTED"
      );
    }


    // Calculate ACTUAL angle change

    float angleChangeX =
      fabs(
        angleX -
        referenceAngleX
      );


    float angleChangeY =
      fabs(
        angleY -
        referenceAngleY
      );


    // ------------------------------------------------
    // CONFIRM FALL
    // ------------------------------------------------

    if (
      (
        impactDetected ||
        movementDetected
      )
      &&
      (
        angleChangeX >=
        ANGLE_CHANGE

        ||

        angleChangeY >=
        ANGLE_CHANGE
      )
    )
    {

      Serial.println();
      Serial.println(
        "FALL CONFIRMED"
      );


      startAlarm();


      possibleFall =
        false;

      impactDetected =
        false;

      movementDetected =
        false;


      return;
    }


    // ------------------------------------------------
    // CANCEL IF TIME EXPIRES
    // ------------------------------------------------

    if (
      millis() -
      possibleFallTime >
      FALL_WINDOW
    )
    {

      possibleFall =
        false;

      impactDetected =
        false;

      movementDetected =
        false;


      Serial.println(
        "MOVEMENT CANCELLED - NO FALL"
      );
    }
  }


  delay(10);
}
