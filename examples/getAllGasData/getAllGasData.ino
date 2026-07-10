/*!
 * @file getAllGasData.ino
 * @brief Read all gas concentration data from the GMA sensor.
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
bool gmaSupportNH3 = false;

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

  return hardwareModel;
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

void printGasPpm(const char *gasName, float value)
{
  Serial.print(gasName);
  Serial.print(": ");

  if(value == GMA_NO_SUPPORT_GAS){
    Serial.print("not supported");
  }else if(value == GMA_VALUE_NOT_READY){
    Serial.print("not ready");
  }else{
    Serial.print(value);
    Serial.print(" ppm");
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

  gmaSupportNH3 = (printHardwareModelInfo() == GMA_HW_GMA3021B);

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
  float co, no2, nh3;
  bool readResult;

  if(gmaSupportNH3 == true){
    readResult = gma.getAllGasData(co, no2, nh3);
  }else{
    readResult = gma.getAllGasData(co, no2);
    nh3 = GMA_NO_SUPPORT_GAS;
  }

  if(readResult == true){
    printGasPpm("CO", co);
    Serial.print(", ");
    printGasPpm("NO2", no2);
    Serial.print(", ");
    printGasPpm("NH3", nh3);
    Serial.println();
  }else{
    Serial.println("Read failed.");
  }

  delay(1000);
}
