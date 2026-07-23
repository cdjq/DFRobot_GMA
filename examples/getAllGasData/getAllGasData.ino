/*!
 * @file getAllGasData.ino
 * @brief Read all gas concentration data from the GMA sensor.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author      thdyyl(yuanlong.yu@dfrobot.com)
 * @version  V1.3.0
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
  Serial.print(gma.getBaselineVoltageMv(gma.eGasCO));
  Serial.println(" mV");

  Serial.print("NO2 baseline voltage: ");
  Serial.print(gma.getBaselineVoltageMv(gma.eGasNO2));
  Serial.println(" mV");

  if(gmaSupportNH3 == true){
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

  if(gmaSupportNH3 == true){
    Serial.print(", NH3=");
    Serial.print(gma.getADCVoltageMv(gma.eGasNH3));
    Serial.print("mV");
  }
  Serial.println();
}

void waitForWarmUp()
{
  Serial.println("Warming up. Warm-up duration may be extended if the sensor has been unpowered for a long period.");
  Serial.println("If the sensor has never been calibrated, place it in clean air for warm-up.");

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
    Serial.println("Warm-up complete. Output gas data.");
  }else if(gma.hasValidBaseline() == true){
    Serial.println("Warm-up complete. The final stable voltage window has been saved as the clean-air baseline.");
    Serial.println("Output gas data.");
  }else{
    Serial.println("Warm-up complete, but the clean-air baseline could not be saved.");
  }
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

  if(gma.setWarmUpStableThresholdMv(GMA_WARMUP_STABLE_THRESHOLD_MV) == true){
    Serial.print("Warm-up stable threshold: ");
    Serial.print(gma.getWarmUpStableThresholdMv());
    Serial.println(" mV");
  }else{
    Serial.println("Failed to set the warm-up stable threshold. Using the firmware default.");
  }

  gmaSupportNH3 = (printHardwareModelInfo() == GMA_HW_GMA3021B);

  waitForWarmUp();
  printBaselineVoltageInfo();
}

void loop()
{
  float co, no2, nh3;
  bool readResult = gma.getAllGasData(co, no2, nh3);

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
