#include <Wire.h>

#define CCS811_ADDR 0x5B // 7-bit unshifted default I2C Address

// Register addresses
#define CCS811_STATUS 0x00
#define CCS811_MEAS_MODE 0x01
#define CCS811_ALG_RESULT_DATA 0x02
#define CCS811_RAW_DATA 0x03
#define CCS811_ENV_DATA 0x05
#define CCS811_NTC 0x06
#define CCS811_THRESHOLDS 0x10
#define CCS811_BASELINE 0x11
#define CCS811_HW_ID 0x20
#define CCS811_HW_VERSION 0x21
#define CCS811_FW_BOOT_VERSION 0x23
#define CCS811_FW_APP_VERSION 0x24
#define CCS811_ERROR_ID 0xE0
#define CCS811_APP_START 0xF4
#define CCS811_SW_RESET 0xFF

// sensor values
unsigned int tVOC = 0;
unsigned int CO2 = 0;


void setup() {
  
  Serial.begin(9600);
  Serial.println("Hello");
  delay(5000);

  Wire.begin();
  
  configure();

  Serial.print("Sensor baseline: 0x");
  unsigned int baseline = getBaseline();
  if (baseline < 0x0100) {
    Serial.print("0");
  }
  Serial.println(baseline, HEX);
  

}

void loop() {
  if (dataAvailable()) {
    readResults();
    // Serial.print("CO2:");
    Serial.print(CO2);
    // Serial.print(" tVOC:");
    Serial.print("\t");
    Serial.print(tVOC);
    // Serial.print(" millis:");
    Serial.print("\t");
    Serial.print(millis());
    Serial.println();
  } else if (checkForError()) {
    printError("Loop"); 
  }
  delay(1000);
}

void readResults() {
  
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CCS811_ALG_RESULT_DATA);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 4); // Get four bytes

  byte co2MSB = Wire.read();
  byte co2LSB = Wire.read();
  byte tvocMSB = Wire.read();
  byte tvocLSB = Wire.read();

  CO2 = ((unsigned int) co2MSB << 8) | co2LSB;
  tVOC = ((unsigned int) tvocMSB << 8) | tvocLSB;
}

void configure() {

  Serial.println("Enter configure()");
  delay(1000);
  
  // Check for correct HW ID
  if (readRegister(0x20) != 0x81) {
    Serial.println("CCS811 hardware ID not found!");
    bail(0);
  }

  if (checkForError()) {
    printError("Initialization");
    bail(1);
  }

  if (!appValid()) {
    Serial.println("App validation failed!");
    bail(2);
  }

  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CCS811_APP_START);
  Wire.endTransmission();
  
  if (checkForError()) {
    printError("App start");
    bail(3);
  }

  // 1 second read interval
  setDriveMode(1);

  if (checkForError()) {
    printError("Drive mode");
    bail(4);
  }
}

boolean checkForError() {
  byte b = readRegister(CCS811_STATUS);
  return (b & 1 << 0);
}

// Reads ERROR register, should clear ERROR_ID register (?)
void printError(const char *reason) {
  byte error = readRegister(CCS811_ERROR_ID);

  Serial.print("Error: ");
  if (reason && *reason) {
    Serial.print(reason);
    Serial.print(" ");
  }
  Serial.print("[ ");
  if (error & 1 << 5) Serial.print("HeaterSupply ");
  if (error & 1 << 4) Serial.print("HeaterFault ");
  if (error & 1 << 3) Serial.print("MaxResistance ");
  if (error & 1 << 2) Serial.print("MeasModeInvalid ");
  if (error & 1 << 1) Serial.print("ReadRegInvalid ");
  if (error & 1 << 0) Serial.print("MsgInvalid ");
  Serial.println("]");
}

// Returns the baseline value
// Used for telling sensor what 'clean' air is
// You must put the sensor in clean air and record this value
unsigned int getBaseline() {
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CCS811_BASELINE);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 2); // Get two bytes
  byte baselineMSB = Wire.read();
  byte baselineLSB = Wire.read();

  unsigned int baseline = ((unsigned int) baselineMSB << 8) | baselineLSB;
  return baseline;
}

boolean dataAvailable() {
  byte value = readRegister(CCS811_STATUS);
  return value & 1 << 3;
}

boolean appValid() {
  byte value = readRegister(CCS811_STATUS);
  return value & 1 << 4;
}

void enableInterrupts() {
  byte value = readRegister(CCS811_MEAS_MODE);
  value |= (1 << 3);    // INTERRUPT bit
  writeRegister(CCS811_MEAS_MODE, value); 
}

void disableInterrupts() {
  byte value = readRegister(CCS811_MEAS_MODE);
  value |= ~(1 << 3);    // INTERRUPT bit
  writeRegister(CCS811_MEAS_MODE, value); 
}

// Firmware modes...
// Mode 0 = Idle
// Mode 1 = read every 1s
// Mode 2 = every 10s
// Mode 3 = every 60s
// Mode 4 = RAW mode
void setDriveMode(byte mode)
{
  if (mode > 4) {
    mode = 4; // Error correction
  }
  
  byte setting = readRegister(CCS811_MEAS_MODE); 
  setting &= ~(0b00000111 << 4); // Clear DRIVE_MODE bits
  setting |= (mode << 4); // Mask in mode
  writeRegister(CCS811_MEAS_MODE, setting);
}

// Set compensation values 
void setEnvironmentalData(float relativeHumidity, float temperature)
{
  int rH = relativeHumidity * 1000;   // 42.348 becomes 42348
  int temp = temperature * 1000;      // 23.2 becomes 23200

  byte envData[4];

  // Split value into 7-bit integer and 9-bit fractional
  envData[0] = ((rH % 1000) / 100) > 7 ? (rH / 1000 + 1) << 1 : (rH / 1000) << 1;
  envData[1] = 0;     // CCS811 only supports increments of 0.5 so bits 7-0 will always be zero
  if (((rH % 1000) / 100) > 2 && (((rH % 1000) / 100) < 8)) {
    envData[0] |= 1;  // Set 9th bit of fractional to indicate 0.5%
  }

  temp += 25000; // Add the 25C offset
  
  // Split value into 7-bit integer and 9-bit fractional
  envData[2] = ((temp % 1000) / 100) > 7 ? (temp / 1000 + 1) << 1 : (temp / 1000) << 1;
  envData[3] = 0;
  if (((temp % 1000) / 100) > 2 && (((temp % 1000) / 100) < 8)) {
    envData[2] |= 1;  // Set 9th bit of fractional to indicate 0.5C
  }

  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CCS811_ENV_DATA);
  Wire.write(envData[0]);
  Wire.write(envData[1]);
  Wire.write(envData[2]);
  Wire.write(envData[3]);
}

byte readRegister(byte addr) {
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(addr);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 1);
 
  unsigned int value = Wire.read();
  return value;
}

void writeRegister(byte addr, byte val) {
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(addr);
  Wire.write(val);
  Wire.endTransmission();
}

void bail(int exitcode) {
  Serial.print("Bailing out with code ");
  Serial.print(exitcode);
  Serial.println("");
  while (1) delay(1000);
}

