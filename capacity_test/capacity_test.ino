#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===================== UART =====================
SoftwareSerial ZPB(10, 11);          // RX, TX dari ZPB30A1
SoftwareSerial ESP32(255, 13);       // TX ke ESP32

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ===================== PIN ======================
const byte PB_RUN       = 4;
const byte RELAY_RUN    = 5;
const byte RELAY_CUTOFF = 8;
const byte RELAY_INH    = 2;
const byte TIMER_FB     = 7;

// ===================== SETTING ==================
const float CUTOFF_VOLTAGE = 5.00;
const float BATTERY_WH     = 30.0;

// ===================== STATUS ===================
bool runState = false;
bool started  = false;
bool cutOff   = false;
bool lastPB   = HIGH;

// ===================== DATA =====================
float voltage = 0;
float current = 0;

float lastValidVoltage = 0;
float lastValidCurrent = 0;

float finalVoltage = 0;
float finalCurrent = 0;

// ===================== ENERGY ===================
float whActual = 0;
unsigned long lastEnergyTime = 0;

// ===================== INTERVAL KIRIM ===========
unsigned long lastSendTime = 0;

// =================================================
// BCD TO DECIMAL
// =================================================

byte bcdToDec(byte v) {
  return ((v >> 4) * 10) + (v & 0x0F);
}

// =================================================
// LCD MAIN SCREEN
// =================================================

void showMainScreen() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Battery Capacity");

  lcd.setCursor(0, 1);
  lcd.print("V : 0.00 V");

  lcd.setCursor(0, 2);
  lcd.print("I : 0.00 A");

  lcd.setCursor(0, 3);
  lcd.print("Status : OFF");
}

// =================================================
// LCD RESULT
// =================================================

void showResult(const char* title) {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(title);

  lcd.setCursor(0, 1);
  lcd.print("Last V : ");
  lcd.print(finalVoltage, 2);
  lcd.print("V");

  lcd.setCursor(0, 2);
  lcd.print("Energy : ");
  lcd.print(whActual, 2);
  lcd.print("Wh");

  float cap = (whActual / BATTERY_WH) * 100.0;

  lcd.setCursor(0, 3);
  lcd.print("Cap : ");
  lcd.print(cap, 1);
  lcd.print("%");
}

// =================================================
// KIRIM DATA KE ESP32
// Format:
// Voltage,Current,Energy,Capacity,Status
// =================================================

void sendESP32() {

  float cap = (whActual / BATTERY_WH) * 100.0;

  ESP32.print(voltage, 2);
  ESP32.print(",");

  ESP32.print(current, 2);
  ESP32.print(",");

  ESP32.print(whActual, 3);
  ESP32.print(",");

  ESP32.print(cap, 2);
  ESP32.print(",");

  if (cutOff) {
    ESP32.println("CUTOFF");
  }
  else if (runState) {
    ESP32.println("RUN");
  }
  else {
    ESP32.println("OFF");
  }
}

// =================================================
// SETUP
// =================================================

void setup() {

  Serial.begin(9600);
  ZPB.begin(9600);
  ESP32.begin(9600);

  // =================================================
  // PUSH BUTTON
  // Tombol antara D4 dan GND
  // Tidak ditekan = HIGH
  // Ditekan = LOW
  // =================================================

  pinMode(PB_RUN, INPUT_PULLUP);

  // Timer feedback
  pinMode(TIMER_FB, INPUT_PULLUP);

  // Relay
  pinMode(RELAY_RUN, OUTPUT);
  pinMode(RELAY_CUTOFF, OUTPUT);
  pinMode(RELAY_INH, OUTPUT);

  // Semua relay OFF saat awal
  digitalWrite(RELAY_RUN, HIGH);
  digitalWrite(RELAY_CUTOFF, HIGH);
  digitalWrite(RELAY_INH, HIGH);

  // LCD
  lcd.init();
  lcd.backlight();

  showMainScreen();

  // Pastikan status awal
  runState = false;
  started = false;
  cutOff = false;

  // Baca kondisi tombol awal
  lastPB = digitalRead(PB_RUN);

  Serial.println("==============================");
  Serial.println("Battery Capacity Ready");
  Serial.println("System Status : OFF");
  Serial.println("==============================");
}

// =================================================
// LOOP
// =================================================

void loop() {

  // =================================================
  // PUSH BUTTON
  // =================================================

  bool pb = digitalRead(PB_RUN);

  // Deteksi tombol ditekan:
  // sebelumnya HIGH -> sekarang LOW
  if (lastPB == HIGH && pb == LOW) {

    delay(40);

    // Pastikan benar-benar masih ditekan
    if (digitalRead(PB_RUN) == LOW) {

      runState = !runState;

      // =================================================
      // START RUN
      // =================================================

      if (runState) {

        started = false;
        cutOff = false;

        whActual = 0;

        lastValidVoltage = 0;
        lastValidCurrent = 0;

        finalVoltage = 0;
        finalCurrent = 0;

        lastEnergyTime = millis();

        // Relay RUN ON
        digitalWrite(RELAY_RUN, LOW);

        // Cutoff OFF
        digitalWrite(RELAY_CUTOFF, HIGH);

        // Timer INH OFF
        digitalWrite(RELAY_INH, HIGH);

        lcd.setCursor(0, 3);
        lcd.print("Status : RUN      ");

        Serial.println("==============================");
        Serial.println("RUN START");
        Serial.println("==============================");

        // Kirim status ke ESP32
        sendESP32();
      }

      // =================================================
      // STOP RUN
      // =================================================

      else {

        digitalWrite(RELAY_RUN, HIGH);
        digitalWrite(RELAY_CUTOFF, HIGH);
        digitalWrite(RELAY_INH, HIGH);

        started = false;

        lcd.setCursor(0, 3);
        lcd.print("Status : OFF      ");

        Serial.println("==============================");
        Serial.println("RUN STOP");
        Serial.println("==============================");

        // Kirim status OFF
        sendESP32();
      }

      // Debounce
      delay(200);
    }
  }

  // Simpan kondisi tombol terakhir
  lastPB = pb;

  // =================================================
  // TIMER FEEDBACK
  // =================================================

  if (runState && !cutOff && digitalRead(TIMER_FB) == LOW) {

    cutOff = true;

    finalVoltage = lastValidVoltage;
    finalCurrent = lastValidCurrent;

    // Cutoff
    digitalWrite(RELAY_CUTOFF, LOW);

    // INH timer
    digitalWrite(RELAY_INH, LOW);

    showResult("** TIMER CUTOFF **");

    Serial.println("==============================");
    Serial.println("TIMER CUTOFF");
    Serial.print("Final Voltage : ");
    Serial.print(finalVoltage, 2);
    Serial.println(" V");

    Serial.print("Final Current : ");
    Serial.print(finalCurrent, 2);
    Serial.println(" A");

    Serial.print("Energy : ");
    Serial.print(whActual, 3);
    Serial.println(" Wh");

    Serial.println("==============================");

    // Kirim status CUTOFF
    sendESP32();

    while (1) {
      delay(100);
    }
  }

  // =================================================
  // UART ZPB
  // =================================================

  if (!cutOff && ZPB.available() >= 4) {

    delay(20);

    byte data[4];

    for (int i = 0; i < 4; i++) {
      data[i] = ZPB.read();
    }

    // =================================================
    // PARSING DATA ZPB
    // =================================================

    voltage = data[0] + (bcdToDec(data[1]) / 100.0);
    current = data[2] + (bcdToDec(data[3]) / 100.0);

    // =================================================
    // SIMPAN DATA TERAKHIR
    // =================================================

    if (runState && voltage > 0) {

      lastValidVoltage = voltage;
      lastValidCurrent = current;
    }

    // =================================================
    // TEST START
    // =================================================

    if (runState && !started && voltage > 1.0) {

      started = true;

      lastEnergyTime = millis();

      lcd.setCursor(0, 3);
      lcd.print("Status : TEST     ");

      Serial.println("==============================");
      Serial.println("TEST START");
      Serial.println("==============================");
    }

    // =================================================
    // HITUNG ENERGY
    // =================================================

    if (runState && started) {

      unsigned long now = millis();
      unsigned long dt = now - lastEnergyTime;

      if (dt >= 200) {

        whActual +=
          (voltage * current * (dt / 1000.0)) / 3600.0;

        lastEnergyTime = now;
      }
    }

    // =================================================
    // LCD VOLTAGE
    // =================================================

    lcd.setCursor(4, 1);
    lcd.print("     ");

    lcd.setCursor(4, 1);
    lcd.print(voltage, 2);
    lcd.print("V");

    // =================================================
    // LCD CURRENT
    // =================================================

    lcd.setCursor(4, 2);
    lcd.print("     ");

    lcd.setCursor(4, 2);
    lcd.print(current, 2);
    lcd.print("A");

    // =================================================
    // SERIAL DEBUG
    // =================================================

    Serial.print(voltage, 2);
    Serial.print(" V  ");

    Serial.print(current, 2);
    Serial.print(" A  ");

    Serial.print(whActual, 3);
    Serial.println(" Wh");

    // =================================================
    // KIRIM DATA KE ESP32 SETIAP 5 DETIK
    // =================================================

    unsigned long nowTime = millis();

    if (nowTime - lastSendTime >= 5000) {

      sendESP32();

      lastSendTime = nowTime;
    }

    // =================================================
    // LOW VOLTAGE CUTOFF
    // =================================================

    if (runState &&
        started &&
        voltage <= CUTOFF_VOLTAGE) {

      cutOff = true;

      finalVoltage = voltage;
      finalCurrent = current;

      // Aktifkan cutoff
      digitalWrite(RELAY_CUTOFF, LOW);

      // Inhibit timer
      digitalWrite(RELAY_INH, LOW);

      // Tetap mempertahankan relay RUN
      digitalWrite(RELAY_RUN, LOW);

      showResult("** LOW CUTOFF **");

      Serial.println("==============================");
      Serial.println("LOW CUTOFF");

      Serial.print("Final Voltage : ");
      Serial.print(finalVoltage, 2);
      Serial.println(" V");

      Serial.print("Final Current : ");
      Serial.print(finalCurrent, 2);
      Serial.println(" A");

      Serial.print("Energy : ");
      Serial.print(whActual, 3);
      Serial.println(" Wh");

      float capacity =
        (whActual / BATTERY_WH) * 100.0;

      Serial.print("Capacity : ");
      Serial.print(capacity, 2);
      Serial.println(" %");

      Serial.println("==============================");

      // Kirim status CUTOFF
      sendESP32();

      while (1) {
        delay(100);
      }
    }
  }
}