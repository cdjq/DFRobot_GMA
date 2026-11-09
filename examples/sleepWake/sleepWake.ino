/*!
 * @file sleepWake.ino
 * @brief Example: stop and resume onboard MCU gas sampling with sleepMode / wakeUpMode.
 * @n Wiring (I2C, default Wire, address GMA_ADDRESS_3 / 0x78):
 * @n VCC → 3.3V or 5V, per the module rating
 * @n GND → GND
 * @n SCL → SCL
 * @n SDA → SDA
 * @n Serial: 115200
 * @details Purpose: show that sleep stops MCU acquisition (ppm becomes not ready),
 *          and wake-up resumes sampling. On SEN0757/SEN0758 the heater and LDO stay
 *          powered, so overall module current does not drop much; this is not a
 *          system-level low-power demo.
 * @n       Flow: `begin()` → warm-up → in `loop()`:
 *          print power state + CO ppm → `sleepMode()` → print again →
 *          `wakeUpMode()` → short wait → print again.
 *          Parameter ranges are in the method comments of `DFRobot_GMA.h`.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @License     The MIT License (MIT)
 * @author [thdyyl](yuanlong.yu@dfrobot.com)
 * @version  V1.0.0
 * @date  2026-07-23
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
const uint8_t GMA_WARMUP_STABLE_THRESHOLD_MV = 10;
/* Wiring: VCC, GND, SCL, SDA. Default address GMA_ADDRESS_3 (0x78). */
DFRobot_GMA gma(&Wire, GMA_I2C_ADDRESS);

/*
 * Hardware model return value:
 * GMA_HW_GMA2021B (0x21) -> GMA2021B, supports CO and NO2.
 * GMA_HW_GMA3021B (0x31) -> GMA3021B, supports CO, NO2 and NH3.
 * 0 or other values        -> Read failed or unknown hardware model.
 */
void printHardwareModelInfo()
{
  uint8_t hardwareModel = gma.getHardwareModel();

  Serial.print("Hardware model: 0x");
  Serial.print(hardwareModel, HEX);
  Serial.print(" (");

  switch(hardwareModel){
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
}

void printBaselineVoltageInfo()
{
  Serial.print("CO baseline voltage: ");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasCO));
  Serial.println(" mV");

  Serial.print("NO2 baseline voltage: ");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasNO2));
  Serial.println(" mV");

  if(gma.getHardwareModel() == GMA_HW_GMA3021B){
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

  if(gma.getHardwareModel() == GMA_HW_GMA3021B){
    Serial.print(", NH3=");
    Serial.print(gma.getADCVoltageMv(gma.eGasNH3));
    Serial.print("mV");
  }
  Serial.println();
}

/*
 * Warm-up before the sleep/wake demo. Remain 0 s alone is not finished.
 * If no R0 exists yet, warm up in clean air so a baseline can be created.
 */
void waitForWarmUp()
{
  bool isBaseline = gma.hasValidBaseline();
  uint16_t lastS = 0xFFFF;
  while(gma.warmUpTime(3) == false){
    uint16_t remainS = gma.getWarmUpRemainSeconds();
    if((lastS != 0xFFFF) && (remainS > lastS)){
      Serial.println("Warm-up incomplete. Restart 1-minute warm-up.");
    }
    printRawVoltageInfo();
    Serial.print("Warm-up remain: ");
    Serial.print(remainS);
    Serial.println(" s");
    lastS = remainS;
    delay(1000);
  }

  if(isBaseline == true){
    Serial.println("Warm-up complete.");
  }else if(gma.hasValidBaseline() == true){
    Serial.println("Warm-up complete. The final stable voltage window has been saved as the clean-air baseline.");
  }else{
    Serial.println("Warm-up complete, but the clean-air baseline could not be saved.");
  }
}

void printPowerState()
{
  uint8_t powerState = gma.getPowerState();

  Serial.print("Power state: ");
  if(powerState == GMA_SLEEP_MODE){
    Serial.print("sleep");
  }else if(powerState == GMA_WAKE_UP_MODE){
    Serial.print("wake-up");
  }else{
    Serial.print("read failed, raw=0x");
    Serial.print(powerState, HEX);
  }
}

/*
 * Print MCU sampling state and one CO sample.
 * While sleeping, getGasData() is expected to report not ready.
 */
void printSamplingSnapshot(const char *phase)
{
  float co = gma.getGasData(gma.eGasCO);

  Serial.print(phase);
  Serial.print(" | ");
  printPowerState();
  Serial.print(" | CO: ");

  if(co == GMA_NO_SUPPORT_GAS){
    Serial.println("not supported");
  }else if(co == GMA_VALUE_NOT_READY){
    Serial.println("not ready");
  }else{
    Serial.print(co);
    Serial.println(" ppm");
  }
}

void setup()
{
  Serial.begin(115200);
  while(!Serial){
  }

  while(gma.begin() == false){
    Serial.println("GMA sensor not found. Check ADD_A1/ADD_A0 address switches.");
    delay(1000);
  }

  if(gma.setWarmUpStableThresholdMv(GMA_WARMUP_STABLE_THRESHOLD_MV) == true){
    Serial.print("Warm-up stable threshold: ");
    Serial.print(gma.getWarmUpStableThresholdMv());
    Serial.println(" mV");
  }else{
    Serial.println("Failed to set the warm-up stable threshold. Using the firmware default.");
  }

  printHardwareModelInfo();

  waitForWarmUp();
  printBaselineVoltageInfo();
  /* Heater/LDO stay on in sleep; this demo shows sampling stop/resume, not current savings. */
}

void loop()
{
  printSamplingSnapshot("Before sleep");

  gma.sleepMode();
  printSamplingSnapshot("After sleep");
  delay(3000);

  gma.wakeUpMode();
  /* Give the MCU a short time to resume acquisition before reading ppm. */
  delay(2000);
  printSamplingSnapshot("After wake-up");

  delay(5000);
}
