/*!
 * @file sleepWake.ino
 * @brief Control sleep and wake-up modes of the GMA sensor.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author      thdyyl(yuanlong.yu@dfrobot.com)
 * @version  V1.0.0
 * @date  2026-07-07
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
  Serial.print(gma.getBaselineVoltageMv(DFRobot_GMA::eGasCO));
  Serial.println(" mV");

  Serial.print("NO2 baseline voltage: ");
  Serial.print(gma.getBaselineVoltageMv(DFRobot_GMA::eGasNO2));
  Serial.println(" mV");
}

void printPowerState(const char *label)
{
  uint8_t powerState = gma.getPowerState();

  Serial.print(label);
  Serial.print(": ");

  if(powerState == GMA_SLEEP_MODE){
    Serial.println("sleep");
  }else if(powerState == GMA_WAKE_UP_MODE){
    Serial.println("wake-up");
  }else{
    Serial.print("read failed, raw=0x");
    Serial.println(powerState, HEX);
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

  printHardwareModelInfo();

  Serial.println("Keep the sensor in clean air.");
  while(gma.warmUpTime(3) == false){
    Serial.print("Warm-up remain: ");
    Serial.print(gma.getWarmUpRemainSeconds());
    Serial.println(" s");
    delay(1000);
  }
  Serial.println("Warm-up calibration done.");
  printBaselineVoltageInfo();
}

void loop()
{
  gma.sleepMode();
  printPowerState("Power state after sleep");

  delay(5000);

  gma.wakeUpMode();
  printPowerState("Power state after wake-up");

  delay(5000);
}
