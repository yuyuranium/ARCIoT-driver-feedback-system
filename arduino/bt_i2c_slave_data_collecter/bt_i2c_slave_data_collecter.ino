#include <TimerOne.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define SLAVE_ADDRESS 0x12
#define L 0
#define R 1

#define SERIAL_BAUD 115200
#define BTSERIAL_BAUD 38400

const int kFractionalBits = 5;
const uint8_t kData = 7;  // ax, ay, az, jx, jy, jz, class
const uint8_t kLed = 2;
const uint8_t kHimaxPinOut = 2;
const uint8_t kSdPinOut = 5;
const uint8_t kLedPinOut[kLed] = { 7, 8 };

const unsigned long kDebounceDuration = 2000;  // 2 s
unsigned long regret_trigger_time;

const char *kDataName[kData] = { "ax", "ay", "az", "jx", "jy", "jz", "class" };

uint8_t class_type;
uint8_t file_counter;

boolean busy;

SoftwareSerial BTSerial(A2, A3); // 宣告10腳位為Arduino的RX 、11為Arduino的 TX
char val;  //儲存接受到的資料變數

File data_entry;

inline float fixed_to_float(int8_t input) {
  return ((float)input / (float)(1 << kFractionalBits));
}

void updateLedsIsr() {
  
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  // bluetooth setup
  BTSerial.begin(BTSERIAL_BAUD);

  // SD card setup
  while (!SD.begin(kSdPinOut)) {
    ;
  }

  delay(200);

  // himax eable setup
  pinMode(kHimaxPinOut, OUTPUT);
  digitalWrite(kHimaxPinOut, LOW);

  // i2c setup
  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveEvent);

  // led setup
  for (int i = 0; i < kLed; ++i) {
    pinMode(kLedPinOut[i], OUTPUT);
    digitalWrite(kLedPinOut[i], LOW);
  }
//  Timer1.initialize(100000);
//  Timer1.attachInterrupt(updateLedsIsr);

  // initialize variables
  busy = false;
  class_type = -1;
  file_counter = 0;
  char filename[10];
  do {
    sprintf(filename, "%d.csv", ++file_counter);
  } while (SD.exists(filename));  // initialize file_counter with latest file count
  regret_trigger_time = millis();
}

void loop() {
  if (BTSerial.available()) {
    val = BTSerial.read();
    if (busy) {
      if (val == 's') {
        digitalWrite(kHimaxPinOut, LOW);
        digitalWrite(kLedPinOut[L], LOW);
        digitalWrite(kLedPinOut[R], LOW);
        data_entry.flush();
        data_entry.close();
        class_type = -1;
        busy = false;
        Serial.println("Done");
      } else if (val == 'r') {
        // Cannot happen
      } else {
        if (val <= '5' && val >= '0')
          class_type = val - '0';
      }
    } else {
      if (val == 's') {
        // The same
        busy = false;
      } else if (val == 'r') {
        // TODO: regret, remove the latest file
        if (debounce_regret()) {
          char filename[10];
          sprintf(filename, "%d.csv", file_counter - 1);
          if (SD.exists(filename)) {
            Serial.print("Deleting: ");
            Serial.println(filename);
            if (SD.remove(filename)) {
              --file_counter;
              Serial.println("Done");
              // lit the led to indicate job done
              digitalWrite(kLedPinOut[L], HIGH);
              digitalWrite(kLedPinOut[R], HIGH);
              delay(1000);
              digitalWrite(kLedPinOut[L], LOW);
              digitalWrite(kLedPinOut[R], LOW);
            }
          } else {
            // fast blink 2 times to indicate error
            for (int i = 0; i < 4; ++i) {
              digitalWrite(kLedPinOut[L], !digitalRead(kLedPinOut[R]));  // sync with R
              digitalWrite(kLedPinOut[R], !digitalRead(kLedPinOut[R]));
              delay(100);
            }
          }
        }
      } else {
        char filename[10];
        sprintf(filename, "%d.csv", file_counter++);
        data_entry = SD.open(filename, FILE_WRITE);
        Serial.print("Writing: ");
        Serial.println(filename);
        int i;
        for (i = 0; i < kData - 1; ++i) {
          data_entry.print(kDataName[i]);
          data_entry.print(',');
        }
        data_entry.println(kDataName[i]);
        class_type = val - '0';
        busy = true;
        digitalWrite(kHimaxPinOut, HIGH);
      }
    }
  }
}

void receiveEvent(int count) {
  if (class_type != -1) {
    int8_t ax, ay, az, jx, jy, jz;
    ax = Wire.read();
    ay = Wire.read();
    az = Wire.read();
    jx = Wire.read();
    jy = Wire.read();
    jz = Wire.read();
    String data_buff = "";
    data_buff += String(fixed_to_float(ax)) + ",";
    data_buff += String(fixed_to_float(ay)) + ",";
    data_buff += String(fixed_to_float(az)) + ",";
    data_buff += String(fixed_to_float(jx)) + ",";
    data_buff += String(fixed_to_float(jy)) + ",";
    data_buff += String(fixed_to_float(jz)) + ",";
    data_buff += String(class_type);
    // Serial.println(data_buff);
    if (data_entry)
      data_entry.println(data_buff);
    switch (class_type) {
      case 0:
        // blink 2
        digitalWrite(kLedPinOut[L], !digitalRead(kLedPinOut[R]));  // sync with R
        digitalWrite(kLedPinOut[R], !digitalRead(kLedPinOut[R]));
        break;
      case 1:
        digitalWrite(kLedPinOut[L], LOW);
        digitalWrite(kLedPinOut[R], HIGH);
        break; 
      case 2:
        digitalWrite(kLedPinOut[L], HIGH);
        digitalWrite(kLedPinOut[R], LOW);
        break;
      case 3:
        digitalWrite(kLedPinOut[L], HIGH);
        digitalWrite(kLedPinOut[R], HIGH);
        break;
      case 4:
        // blink L
        digitalWrite(kLedPinOut[L], !digitalRead(kLedPinOut[L]));
        digitalWrite(kLedPinOut[R], LOW);
        break;
      case 5:
        // blink R
        digitalWrite(kLedPinOut[L], LOW);
        digitalWrite(kLedPinOut[R], !digitalRead(kLedPinOut[R]));
        break;
      default:
        digitalWrite(kLedPinOut[L], LOW);
        digitalWrite(kLedPinOut[R], LOW);
        break;
    }
  } else {
    while (Wire.available())
      Wire.read();
  }
}

boolean debounce_regret() {
  unsigned long now = millis();
  if (now - regret_trigger_time > kDebounceDuration) {
    regret_trigger_time = now;
    return true;
  } else {
    regret_trigger_time = now;
    return false;
  }
}
