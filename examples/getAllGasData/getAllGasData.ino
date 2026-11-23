/*!
 * @file getAllGasData.ino
 * @brief Example: warm up the GMA module, then read CO / NO2 / NH3 ppm in one register burst.
 * @n Wiring (I2C, default Wire, address GMA_ADDRESS_3 / 0x78):
 * @n VCC → 3.3V or 5V, per the module rating
 * @n GND → GND
 * @n SCL → SCL
 * @n SDA → SDA
 * @n Serial: 115200
 * @details Same role as `getGasData` for daily ppm reading; no prior
 *          `calibrateBaseline` is required. Difference: `loop()` uses
 *          `getAllGasData(co, no2, nh3)` to fetch all registers in one burst.
 * @n       On I2C failure it prints "Read failed.". GMA2021B reports NH3 as not supported.
 *          `Warm-up remain: 0 s` alone is not finished; wait for "Warm-up complete...".
 *          Parameter ranges are in `DFRobot_GMA.h`.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @License     The MIT License (MIT)
 * @author [thdyyl](yuanlong.yu@dfrobot.com)
 * @version  V1.0.0
 * @date  2026-11-23
 * @url https://github.com/DFRobot/DFRobot_GMA
 */

#include "DFRobot_GMA.h"

/*
 * I2C address switch table:
 * ADD_A1 ADD_A0 -> Address
 * 0      0      -> GMA_ADDRESS_0 (0x75)
 * 0      1      -> GMA_ADDRESS_1 (0x76)
 * 1      0      -> GMA_ADDRESS_2 (0x77)
 * 1      1      -> GMA_ADDRESS_3 (0x78)
 * Demo default: GMA_ADDRESS_3 (ADD_A1=1, ADD_A0=1, 0x78)
 */
const uint8_t GMA_I2C_ADDRESS = GMA_ADDRESS_3;
/* Demo threshold 10 mV (valid 1–50). Firmware default after reset is 50 mV. */
const uint8_t GMA_WARMUP_STABLE_THRESHOLD_MV = 10;
/* Wiring: VCC, GND, SCL, SDA. Default address GMA_ADDRESS_3 (0x78). */
DFRobot_GMA gma(&Wire, GMA_I2C_ADDRESS);
bool        gmaSupportNH3 = false;

/*
 * Hardware model return value:
 * GMA_HW_GMA2021B (0x21) -> GMA2021B, supports CO and NO2.
 * GMA_HW_GMA3021B (0x31) -> GMA3021B, supports CO, NO2 and NH3.
 * 0 or other values        -> Read failed or unknown hardware model.
 */
uint8_t printHardwareModelInfo()
{
  uint8_t hardwareModel = gma.getHardwareModel();

  Serial.print("Hardware model: 0x");
  Serial.print(hardwareModel, HEX);
  Serial.print(" (");

  switch (hardwareModel) {
    case GMA_HW_GMA2021B:
      Serial.println("GMA2021B, CO/NO2)");
      break;

    case GMA_HW_GMA3021B:
      Serial.println("GMA3021B, CO/NO2/NH3)");
      break;

    default:
      Serial.println("Unknown)");
      break;
  }

  return hardwareModel;
}

void printBaselineVoltageInfo()
{
  Serial.print("CO baseline voltage: ");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasCO));
  Serial.println(" mV");

  Serial.print("NO2 baseline voltage: ");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasNO2));
  Serial.println(" mV");

  if (gmaSupportNH3 == true) {
    Serial.print("NH3 baseline voltage: ");
    Serial.print(gma.getBaselineVoltageMv(gma.eGasNH3));
    Serial.println(" mV");
  }
}

void printRawVoltageInfo()
{
  Serial.print("Raw voltage: CO=");
  Serial.print(gma.getADCVoltageMv(gma.eGasCO));
  Serial.print("mV, NO2=");
  Serial.print(gma.getADCVoltageMv(gma.eGasNO2));
  Serial.print("mV");

  if (gmaSupportNH3 == true) {
    Serial.print(", NH3=");
    Serial.print(gma.getADCVoltageMv(gma.eGasNH3));
    Serial.print("mV");
  }
  Serial.println();
}

/*
 * Poll warmUpTime(3) until true. Remain 0 s alone is not finished (retry boundary).
 * Long unpowered time may extend warm-up. If no R0 exists yet, place the module in
 * clean air so the final stable window can be saved as the first baseline.
 */
void waitForWarmUp()
{
  bool     isBaseline = gma.hasValidBaseline();
  uint16_t lastS      = 0xFFFF;
  while (gma.warmUpTime(3) == false) {
    uint16_t remainS = gma.getWarmUpRemainSeconds();
    if ((lastS != 0xFFFF) && (remainS > lastS)) {
      Serial.println("Warm-up incomplete. Restart 1-minute warm-up.");
    }
    printRawVoltageInfo();
    Serial.print("Warm-up remain: ");
    Serial.print(remainS);
    Serial.println(" s");
    lastS = remainS;
    delay(1000);
  }

  if (isBaseline == true) {
    Serial.println("Warm-up complete. Output gas data.");
  } else if (gma.hasValidBaseline() == true) {
    Serial.println("Warm-up complete. The final stable voltage window has been saved as the clean-air baseline.");
  } else {
    Serial.println("Warm-up complete, but the clean-air baseline could not be saved.");
  }
}

void printGasPpm(const char *gasName, float value)
{
  Serial.print(gasName);
  Serial.print(": ");

  if (value == GMA_NO_SUPPORT_GAS) {
    Serial.print("not supported");
  } else if (value == GMA_VALUE_NOT_READY) {
    Serial.print("not ready");
  } else {
    Serial.print(value);
    Serial.print(" ppm");
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) {
  }

  while (gma.begin() == false) {
    Serial.println("GMA sensor not found. Check ADD_A1/ADD_A0 address switches.");
    delay(1000);
  }

  if (gma.setWarmUpStableThresholdMv(GMA_WARMUP_STABLE_THRESHOLD_MV) == true) {
    Serial.print("Warm-up stable threshold: ");
    Serial.print(gma.getWarmUpStableThresholdMv());
    Serial.println(" mV");
  } else {
    Serial.println("Failed to set the warm-up stable threshold. Using the firmware default.");
  }

  gmaSupportNH3 = (printHardwareModelInfo() == GMA_HW_GMA3021B);

  waitForWarmUp();
  printBaselineVoltageInfo();
}

void loop()
{
  float co, no2, nh3;
  bool  readResult = gma.getAllGasData(co, no2, nh3);

  if (readResult == true) {
    printGasPpm("CO", co);
    Serial.print(", ");
    printGasPpm("NO2", no2);
    Serial.print(", ");
    printGasPpm("NH3", nh3);
    Serial.println();
  } else {
    Serial.println("Read failed.");
  }

  delay(1000);
}
