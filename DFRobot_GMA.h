/*!
 * @file DFRobot_GMA.h
 * @brief I2C driver for DFRobot GMA gas sensor modules.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author      thdyyl(yuanlong.yu@dfrobot.com)
 * @version  V1.0.0
 * @date  2026-07-07
 * @url https://github.com/DFRobot/DFRobot_GMA
 */

#ifndef __DFRobot_GMA_H__
#define __DFRobot_GMA_H__

#include <Arduino.h>
#include <Wire.h>

#define GMA_ADDRESS_0                 ((uint8_t)0x75) ///< ADD_A1=0, ADD_A0=0.
#define GMA_ADDRESS_1                 ((uint8_t)0x76) ///< ADD_A1=0, ADD_A0=1.
#define GMA_ADDRESS_2                 ((uint8_t)0x77) ///< ADD_A1=1, ADD_A0=0.
#define GMA_ADDRESS_3                 ((uint8_t)0x78) ///< ADD_A1=1, ADD_A0=1.

#define GMA_HW_GMA2021B               ((uint8_t)0x21) ///< Hardware model returned by getHardwareModel(): GMA2021B, supports CO and NO2.
#define GMA_HW_GMA3021B               ((uint8_t)0x31) ///< Hardware model returned by getHardwareModel(): GMA3021B, supports CO, NO2 and NH3.

#define GMA_SLEEP_MODE                ((uint8_t)0x00) ///< The onboard MCU is in sleep mode.
#define GMA_WAKE_UP_MODE              ((uint8_t)0x01) ///< The onboard MCU is in normal mode.

#define GMA_NO_SUPPORT_GAS            (-9999.0f)      ///< The current hardware does not support this gas.
#define GMA_VALUE_NOT_READY           (-8888.0f)      ///< The gas data is not ready.

#define GMA_STATUS_BASELINE_VALID     ((uint8_t)0x01) ///< Clean-air baseline is valid.
#define GMA_STATUS_WARMING            ((uint8_t)0x02) ///< Warm-up calibration is running.
#define GMA_STATUS_DATA_VALID         ((uint8_t)0x04) ///< Gas ppm data is valid.
#define GMA_STATUS_SLEEP              ((uint8_t)0x08) ///< The onboard MCU is sleeping.
#define GMA_STATUS_ADC_ERROR          ((uint8_t)0x10) ///< ADC sampling failed.
#define GMA_STATUS_FLASH_ERROR        ((uint8_t)0x20) ///< Flash storage failed.
#define GMA_STATUS_FORMULA_MISSING    ((uint8_t)0x40) ///< Gas formula is not configured in firmware.

#define GMA_WARMUP_DEFAULT_MINUTE     ((uint8_t)3)    ///< Recommended default warm-up time.

class DFRobot_GMA
{
public:
  typedef enum {
    eGasCO = 0x01,  ///< CO gas type.
    eGasNO2 = 0x02, ///< NO2 gas type.
    eGasNH3 = 0x03  ///< NH3 gas type.
  } eGasType_t;

  /**
   * @fn DFRobot_GMA
   * @brief Constructor, bind the I2C bus and sensor I2C address.
   * @param pWire I2C bus object, default is &Wire.
   * @param i2cAddr Sensor I2C address, GMA_ADDRESS_0 ~ GMA_ADDRESS_3. Default is GMA_ADDRESS_3.
   */
  DFRobot_GMA(TwoWire *pWire = &Wire, uint8_t i2cAddr = GMA_ADDRESS_3);

  /**
   * @fn begin
   * @brief Initialize I2C communication and read the device ID and hardware model.
   * @return Initialization result.
   * @retval true The GMA device is detected.
   * @retval false No device is detected or I2C communication failed.
   */
  bool begin(void);

  /**
   * @fn getHardwareModel
   * @brief Read the hardware model stored by the onboard MCU.
   * @return Hardware model register value.
   * @retval GMA_HW_GMA2021B 0x21, GMA2021B dual-gas module, supports CO and NO2.
   * @retval GMA_HW_GMA3021B 0x31, GMA3021B triple-gas module, supports CO, NO2 and NH3.
   * @retval 0 Read failed or the hardware model is invalid.
   */
  uint8_t getHardwareModel(void);

  /**
   * @fn warmUpTime
   * @brief Start and poll clean-air zero-point warm-up calibration.
   * @param minute Warm-up calibration time in minutes. Values below 1 are treated as 1 minute.
   * @return Calibration state.
   * @retval true Warm-up calibration is complete.
   * @retval false Warm-up is still running or the command write failed.
   * @note This function does not block for the full warm-up time. Call it repeatedly in loop() or setup().
   */
  bool warmUpTime(uint8_t minute = GMA_WARMUP_DEFAULT_MINUTE);

  /**
   * @fn getGasData
   * @brief Read the ppm concentration of the selected gas.
   * @param gasType Gas type, eGasCO / eGasNO2 / eGasNH3.
   * @return Gas concentration in ppm.
   * @retval GMA_NO_SUPPORT_GAS The current hardware does not support this gas. For example, GMA2021B returns this value (-9999.0f) when reading eGasNH3.
   * @retval GMA_VALUE_NOT_READY Data is not ready, or the module is uncalibrated, warming up or sleeping.
   */
  float getGasData(eGasType_t gasType);

  /**
   * @fn getAllGasData
   * @brief Read all gas concentration registers at once.
   * @param co CO concentration output, unit ppm.
   * @param no2 NO2 concentration output, unit ppm.
   * @param nh3 NH3 concentration output. GMA2021B sets this output to GMA_NO_SUPPORT_GAS (-9999.0f).
   * @return Read result.
   * @retval true I2C read succeeded.
   * @retval false I2C communication failed. Outputs are set to GMA_VALUE_NOT_READY (-8888.0f).
   */
  bool getAllGasData(float &co, float &no2, float &nh3);

  /**
   * @fn getAllGasData
   * @brief Read CO and NO2 concentration registers at once.
   * @param co CO concentration output, unit ppm.
   * @param no2 NO2 concentration output, unit ppm.
   * @return Read result.
   * @retval true I2C read succeeded.
   * @retval false I2C communication failed. Outputs are set to GMA_VALUE_NOT_READY (-8888.0f).
   * @note Use this overload when NH3 is not needed or when using GMA2021B.
   */
  bool getAllGasData(float &co, float &no2);

  /**
   * @fn sleepMode
   * @brief Let the onboard MCU enter low-power sleep mode.
   * @note This function waits briefly for the power-state register to update.
   */
  void sleepMode(void);

  /**
   * @fn wakeUpMode
   * @brief Wake up the onboard MCU and resume gas sampling.
   * @note This function waits briefly for the power-state register to update.
   */
  void wakeUpMode(void);

  /**
   * @fn getPowerState
   * @brief Read the current power state of the onboard MCU.
   * @return Power state.
   * @retval GMA_SLEEP_MODE Sleep mode.
   * @retval GMA_WAKE_UP_MODE Normal sampling mode.
   * @retval 0xFF I2C communication failed.
   * @note This reads the latest power-state register snapshot from the module.
   */
  uint8_t getPowerState(void);

  /**
   * @fn softReset
   * @brief Reset the onboard MCU without clearing user calibration data.
   * @return Command write result.
   * @retval true Command write succeeded.
   * @retval false I2C communication failed.
   */
  bool softReset(void);

  /**
   * @fn getStatus
   * @brief Read onboard MCU status bits.
   * @return Bit mask of GMA_STATUS_xxx values.
   */
  uint8_t getStatus(void);

  /**
   * @fn getWarmUpRemainSeconds
   * @brief Read the remaining warm-up time.
   * @return Remaining seconds. It is usually 0 when warm-up is not running.
   */
  uint16_t getWarmUpRemainSeconds(void);

  /**
   * @fn getCommandStatus
   * @brief Read the execution status of the latest command.
   * @return Command status: 0 idle, 1 busy, 2 done, 0x80 error.
   */
  uint8_t getCommandStatus(void);

  /**
   * @fn getADCVoltageMv
   * @brief Read the current ADC voltage of the selected gas channel.
   * @param gasType eGasNO2 reads AIN0/ADC_NO2, eGasCO reads AIN1/ADC_CO.
   * @return Current voltage in mV. Unsupported gas types return 0.
   */
  uint16_t getADCVoltageMv(eGasType_t gasType);

  /**
   * @fn getBaselineVoltageMv
   * @brief Read the clean-air baseline voltage of the selected gas channel.
   * @param gasType eGasNO2 reads the AIN0 baseline, eGasCO reads the AIN1 baseline.
   * @return Baseline voltage in mV. Unsupported gas types return 0.
   * @note Read this value after warm-up calibration is complete. The exposed baseline registers are AIN0 and AIN1.
   */
  uint16_t getBaselineVoltageMv(eGasType_t gasType);

private:
  typedef union {
    uint32_t value;
    uint8_t bytes[4];
    float floatValue;
  } uFloatData_t;

  bool readReg(uint8_t reg, uint8_t *pBuf, uint8_t len);
  bool writeReg(uint8_t reg, const uint8_t *pBuf, uint8_t len);
  bool writeCommand(uint8_t command, uint8_t param);
  uint8_t readUInt8(uint8_t reg, uint8_t defaultValue);
  uint16_t readUInt16(uint8_t reg, uint16_t defaultValue);
  float readFloat(uint8_t reg, float defaultValue);
  float bytesToFloat(const uint8_t *pBuf);

  TwoWire *_pWire;
  uint8_t _i2cAddr;
  uint8_t _hwModel;
  uint8_t _warmUpMinute;
  bool _warmUpActive;
  bool _warmUpDone;
};

#endif
