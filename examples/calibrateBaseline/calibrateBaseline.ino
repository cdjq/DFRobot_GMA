/*!
 * @file calibrateBaseline.ino
 * @brief Clear an old R0 when necessary, then let warmUpTime() create a new clean-air baseline.
 * @copyright Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @version V1.3.0
 * @date 2026-07-23
 */

#include "DFRobot_GMA.h"

const uint8_t GMA_I2C_ADDRESS = GMA_ADDRESS_3;
const uint8_t GMA_WARMUP_STABLE_THRESHOLD_MV = 10;

DFRobot_GMA gma(&Wire, GMA_I2C_ADDRESS);
bool supportsNH3 = false;

void printRawVoltageInfo()
{
  Serial.print("Raw voltage: CO=");
  Serial.print(gma.getADCVoltageMv(gma.eGasCO));
  Serial.print("mV, NO2=");
  Serial.print(gma.getADCVoltageMv(gma.eGasNO2));
  Serial.print("mV");
  if(supportsNH3 == true){
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
  if(supportsNH3 == true){
    Serial.print(", NH3=");
    Serial.print(gma.getBaselineVoltageMv(gma.eGasNH3));
    Serial.print("mV");
  }
  Serial.println();
}

/*
 * warmUpTime() performs the complete operation:
 * initial warm-up -> stability check -> 1-minute retries until stable.
 * Because this demo clears any old R0 first, the firmware automatically saves
 * the final stable window as the new clean-air baseline.
 */
void waitForWarmUp()
{
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
}

void setup()
{
  Serial.begin(115200);
  while(!Serial){
  }

  while(gma.begin() == false){
    Serial.println("GMA sensor not found. Check the I2C address switches.");
    delay(1000);
  }

  if(gma.setWarmUpStableThresholdMv(GMA_WARMUP_STABLE_THRESHOLD_MV) == true){
    Serial.print("Warm-up stable threshold: ");
    Serial.print(gma.getWarmUpStableThresholdMv());
    Serial.println(" mV");
  }else{
    Serial.println("Failed to set the warm-up stable threshold. Using the firmware default.");
  }

  supportsNH3 = (gma.getHardwareModel() == GMA_HW_GMA3021B);
  Serial.println("Clean-air calibration requested.");
  Serial.println("Remove all target gases and keep airflow, temperature and humidity stable.");

  /* Running this dedicated example is the user's explicit request to recalibrate. */
  if(gma.hasValidBaseline() == true){
    if(gma.clearBaseline() == false){
      Serial.println("Failed to clear the old R0. Calibration stopped.");
      return;
    }
    Serial.println("Old R0 cleared. The sensor is now uncalibrated.");
  }

  waitForWarmUp();

  if(gma.hasValidBaseline() == true){
    Serial.println("Clean-air R0 calibration complete. The final stable window was saved automatically.");
    printBaselineVoltageInfo();
  }else{
    Serial.println("Warm-up completed, but the clean-air R0 could not be saved.");
  }
}

void loop()
{
  delay(1000);
}
