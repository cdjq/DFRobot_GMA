/*!
 * @file calibrateBaseline.ino
 * @brief Example: force a new clean-air R0 by clearing the old baseline, then running warm-up.
 * @n Wiring (I2C, default Wire, address GMA_ADDRESS_3 / 0x78):
 * @n VCC → 3.3V or 5V, per the module rating
 * @n GND → GND
 * @n SCL → SCL
 * @n SDA → SDA
 * @n Serial: 115200
 * @details When to use this demo:
 *          - Use this sketch only when you need to force a new clean-air R0
 *            (wrong old baseline, new environment, etc.).
 *          - Daily ppm reading does NOT require this demo first. Use `getGasData`
 *            / `getAllGasData`; they warm up and keep an existing R0, or create one
 *            automatically if none exists.
 * @n       Run only in verified clean air (no target gas). Stability ≠ clean air.
 * @n       Flow: `begin()` → optional `setWarmUpStableThresholdMv()` → if R0 exists,
 *          `clearBaseline()` → poll `warmUpTime(3)` → firmware saves the final stable
 *          window as the new R0 → print `Saved R0 voltage`.
 * @n       How to read the serial output:
 *          - `Warm-up remain: 0 s` alone does NOT mean finished (retry boundary / busy).
 *          - Success ends with `Clean-air R0 calibration complete...` and R0 voltages.
 *          - `Failed to clear the old R0` stops here; power-cycle the module (not only
 *            the MCU) and retry if the sensor was still busy after a host reset.
 * @n       `loop()` is empty; this is a one-shot calibration, not a ppm reader.
 *          Parameter ranges are in the method comments of `DFRobot_GMA.h`.
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
bool        supportsNH3 = false;

void printRawVoltageInfo()
{
  Serial.print("Raw voltage: CO=");
  Serial.print(gma.getADCVoltageMv(gma.eGasCO));
  Serial.print("mV, NO2=");
  Serial.print(gma.getADCVoltageMv(gma.eGasNO2));
  Serial.print("mV");
  if (supportsNH3 == true) {
    Serial.print(", NH3=");
    Serial.print(gma.getADCVoltageMv(gma.eGasNH3));
    Serial.print("mV");
  }
  Serial.println();
}

void printBaselineVoltageInfo()
{
  Serial.print("Saved R0 voltage: CO=");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasCO));
  Serial.print("mV, NO2=");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasNO2));
  Serial.print("mV");
  if (supportsNH3 == true) {
    Serial.print(", NH3=");
    Serial.print(gma.getBaselineVoltageMv(gma.eGasNH3));
    Serial.print("mV");
  }
  Serial.println();
}

/*
 * warmUpTime(3) polls: initial warm-up -> stability check -> 1-minute retries.
 * After clearBaseline(), no R0 exists, so a successful warm-up saves the final
 * stable window as the new clean-air baseline automatically.
 * Exit this loop only when warmUpTime() returns true (not when remain hits 0).
 */
void waitForWarmUp()
{
  uint16_t lastS = 0xFFFF;

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
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) {
  }

  while (gma.begin() == false) {
    Serial.println("GMA sensor not found. Check the I2C address switches.");
    delay(1000);
  }

  /* Optional demo threshold. Failure is non-fatal (firmware keeps default 50 mV). */
  if (gma.setWarmUpStableThresholdMv(GMA_WARMUP_STABLE_THRESHOLD_MV) == true) {
    Serial.print("Warm-up stable threshold: ");
    Serial.print(gma.getWarmUpStableThresholdMv());
    Serial.println(" mV");
  } else {
    Serial.println("Failed to set the warm-up stable threshold. Using the firmware default.");
  }

  supportsNH3 = (gma.getHardwareModel() == GMA_HW_GMA3021B);

  /*
   * Force recalibration in verified clean air only.
   * Clear existing R0 so the next warm-up must create a new one.
   * If clear fails (e.g. module still busy after a host-only reset), stop here;
   * power-cycle the GMA module and run this sketch again.
   * If no R0 exists yet, skip clear and let warm-up create the first baseline.
   */
  if (gma.hasValidBaseline() == true) {
    if (gma.clearBaseline() == false) {
      Serial.println("Failed to clear the old R0. Calibration stopped.");
      return;
    }
    Serial.println("Old R0 cleared. The sensor is now uncalibrated.");
  }

  waitForWarmUp();

  /* Success: hasValidBaseline() true and Saved R0 voltages printed. Then use getGasData / getAllGasData for ppm. */
  if (gma.hasValidBaseline() == true) {
    Serial.println("Clean-air R0 calibration complete. The final stable window was saved automatically.");
    printBaselineVoltageInfo();
  } else {
    Serial.println("Warm-up completed, but the clean-air R0 could not be saved.");
  }
}

void loop()
{
  /* One-shot calibration sketch: nothing to do after setup(). */
  delay(1000);
}
