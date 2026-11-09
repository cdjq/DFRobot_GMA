/*!
 * @file DFRobot_GMA.h
 * @brief I2C driver for DFRobot GMA gas sensor modules.
 * @details Reads CO, NO2 and NH3 from the onboard controller. The module handles ADC sampling, baseline storage and ppm conversion.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @License     The MIT License (MIT)
 * @author [thdyyl](yuanlong.yu@dfrobot.com)
 * @version  V1.0.0
 * @date  2026-07-23
 * @url https://github.com/DFRobot/DFRobot_GMA
 */

#ifndef __DFRobot_GMA_H__
#define __DFRobot_GMA_H__

#include "Arduino.h"
#include "Wire.h"

#define GMA_ADDRESS_0                 ((uint8_t)0x75) ///< ADD_A1=0, ADD_A0=0.
#define GMA_ADDRESS_1                 ((uint8_t)0x76) ///< ADD_A1=0, ADD_A0=1.
#define GMA_ADDRESS_2                 ((uint8_t)0x77) ///< ADD_A1=1, ADD_A0=0.
#define GMA_ADDRESS_3                 ((uint8_t)0x78) ///< ADD_A1=1, ADD_A0=1.

#define GMA_HW_GMA2021B               ((uint8_t)0x21) ///< Hardware model returned by getHardwareModel(): GMA2021B, supports CO and NO2.
#define GMA_HW_GMA3021B               ((uint8_t)0x31) ///< Hardware model returned by getHardwareModel(): GMA3021B, supports CO, NO2 and NH3.

#define GMA_SLEEP_MODE                ((uint8_t)0x00) ///< Onboard MCU stopped gas sampling (heater/LDO still powered).
#define GMA_WAKE_UP_MODE              ((uint8_t)0x01) ///< Onboard MCU is sampling normally.

#define GMA_NO_SUPPORT_GAS            (-9999.0f)      ///< The current hardware does not support this gas.
#define GMA_VALUE_NOT_READY           (-8888.0f)      ///< The gas data is not ready.

#define GMA_STATUS_BASELINE_VALID     ((uint8_t)0x01) ///< Clean-air baseline is valid.
#define GMA_STATUS_WARMING            ((uint8_t)0x02) ///< Warm-up/stability checking is running; this does not imply R0 calibration.
#define GMA_STATUS_DATA_VALID         ((uint8_t)0x04) ///< Gas ppm data is valid.
#define GMA_STATUS_SLEEP              ((uint8_t)0x08) ///< Onboard MCU sampling is stopped (sleep).
#define GMA_STATUS_ADC_ERROR          ((uint8_t)0x10) ///< ADC sampling failed.
#define GMA_STATUS_FLASH_ERROR        ((uint8_t)0x20) ///< Flash storage failed.
#define GMA_STATUS_FORMULA_MISSING    ((uint8_t)0x40) ///< Gas formula is not configured in firmware.
#define GMA_WARMUP_DEFAULT_MINUTE     ((uint8_t)3)    ///< Recommended default warm-up time.

/**
 * @class DFRobot_GMA
 * @brief Driver for the Gravity MEMS gas sensor module over I2C.
 */
class DFRobot_GMA
{
public:
  /**
   * @enum eGasType_t
   * @brief Gas channel used by the concentration and voltage readers.
   */
  typedef enum {
    eGasCO = 0x01,  ///< CO gas type.
    eGasNO2 = 0x02, ///< NO2 gas type.
    eGasNH3 = 0x03  ///< NH3 gas type.
  } eGasType_t;

  /**
   * @fn DFRobot_GMA
   * @brief Constructor, bind the I2C bus and sensor I2C address.
   * @param pWire I2C bus object.
   * @param i2cAddr Sensor I2C address.
   * @n pWire defaults to &Wire.
   * @n i2cAddr is GMA_ADDRESS_0 ~ GMA_ADDRESS_3. Default is GMA_ADDRESS_3 (0x78).
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
   * @brief Configure and poll the onboard warm-up/stability process.
   * @param minute Initial warm-up time in minutes.
   * @n Values below 1 are treated as 1 minute. Default is GMA_WARMUP_DEFAULT_MINUTE (3).
   * @n Firmware starts a 3-minute warm-up automatically at power-on. If the host restarts while the sensor stays powered and reports COMPLETE, the first warmUpTime() call starts a new warm-up. A running firmware warm-up is adopted without resetting its countdown.
   * @n An existing R0 is preserved; when R0 is absent, the final stable window is saved automatically as the new clean-air baseline.
   * @n When no R0 exists, run this process only in verified clean air. The firmware cannot identify whether the surrounding air is actually clean.
   * @return Warm-up state.
   * @retval true Warm-up and stability checking are complete. If no R0 existed, the firmware has attempted to save the final stable window as R0.
   * @retval false Warm-up is still running or the command write failed.
   */
  bool warmUpTime(uint8_t minute = GMA_WARMUP_DEFAULT_MINUTE);

  /**
   * @fn setWarmUpStableThresholdMv
   * @brief Set the voltage-deviation threshold used by the warm-up stability check.
   * @param value Threshold in mV.
   * @n Valid range 1–50. The setting is volatile and resets to 50 mV after a firmware reset.
   * @n If warm-up is already complete, the next warmUpTime() call starts a new warm-up so the new threshold is verified. A running warm-up continues and uses the new threshold at its next check.
   * @return Set result.
   * @retval true The firmware accepted the value and readback matches it.
   * @retval false The value is out of range, the command failed or readback did not match.
   */
  bool setWarmUpStableThresholdMv(uint8_t value);

  /**
   * @fn getWarmUpStableThresholdMv
   * @brief Read the voltage-deviation threshold used by the warm-up stability check.
   * @n Returns the threshold in mV, or 0 when the register read fails.
   * @return Threshold in mV.
   */
  uint8_t getWarmUpStableThresholdMv(void);

  /**
   * @fn clearBaseline
   * @brief Clear the saved clean-air baseline and enter the uncalibrated state.
   * @n This function only clears R0. Call warmUpTime() in verified clean air afterward; because no baseline exists, warmUpTime() will save its final stable window as the new R0.
   * @return Clear result.
   * @retval true The baseline was cleared.
   * @retval false The command or Flash erase failed.
   */
  bool clearBaseline(void);

  /**
   * @fn hasValidBaseline
   * @brief Check whether a clean-air R0 baseline is available.
   * @return Baseline state.
   * @retval true A valid baseline exists and can be used for concentration conversion.
   * @retval false The module is uncalibrated.
   */
  bool hasValidBaseline(void);

  /**
   * @fn getGasData
   * @brief Read the ppm concentration of the selected gas.
   * @param gasType Gas type, eGasCO / eGasNO2 / eGasNH3.
   * @n GMA2021B does not support eGasNH3 and returns GMA_NO_SUPPORT_GAS (-9999.0f).
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
   * @param nh3 NH3 concentration output, unit ppm.
   * @n GMA2021B sets nh3 to GMA_NO_SUPPORT_GAS (-9999.0f).
   * @n On I2C failure every output is set to GMA_VALUE_NOT_READY (-8888.0f).
   * @return Read result.
   * @retval true I2C read succeeded.
   * @retval false I2C communication failed. Outputs are set to GMA_VALUE_NOT_READY (-8888.0f).
   */
  bool getAllGasData(float &co, float &no2, float &nh3);

  /**
   * @fn sleepMode
   * @brief Stop onboard MCU gas sampling (sleep).
   * @n On SEN0757/SEN0758 the heater and LDO stay powered, so overall module current
   * @n does not drop much. Sleep only pauses MCU acquisition, not a system low-power cut.
   * @n This function waits briefly for the power-state register to update.
   */
  void sleepMode(void);

  /**
   * @fn wakeUpMode
   * @brief Resume onboard MCU gas sampling after sleep.
   * @n Heater/LDO were already on; this restarts acquisition on the MCU.
   * @n This function waits briefly for the power-state register to update.
   */
  void wakeUpMode(void);

  /**
   * @fn getPowerState
   * @brief Read whether the onboard MCU is sampling or stopped.
   * @n This reads the latest power-state register snapshot from the module.
   * @return Power state.
   * @retval GMA_SLEEP_MODE MCU sampling stopped.
   * @retval GMA_WAKE_UP_MODE MCU sampling normally.
   * @retval 0xFF I2C communication failed.
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
   * @brief Read the remaining seconds in the current warm-up phase.
   * @n Returns the remaining seconds in the current initial or retry phase.
   * @n The firmware exposes 0 at an unstable-window boundary before starting the next 1-minute retry. A completed or idle warm-up also returns 0, so use warmUpTime() to decide whether the whole process is complete.
   * @return Remaining seconds.
   */
  uint16_t getWarmUpRemainSeconds(void);

  /**
   * @fn getCommandStatus
   * @brief Read the execution status of the latest command.
   * @n 0 idle, 1 busy, 2 done, 0x80 error.
   * @return Command status.
   */
  uint8_t getCommandStatus(void);

  /**
   * @fn getADCVoltageMv
   * @brief Read the current ADC voltage of the selected gas channel.
   * @param gasType Gas channel.
   * @n eGasCO reads AIN0/ADC_CO, eGasNO2 reads AIN1/ADC_NO2, eGasNH3 reads AIN2/ADC_NH3 on GMA3021B.
   * @n Unsupported gas types return 0.
   * @return Current voltage in mV.
   */
  uint16_t getADCVoltageMv(eGasType_t gasType);

  /**
   * @fn getBaselineVoltageMv
   * @brief Read the clean-air baseline voltage of the selected gas channel.
   * @param gasType Gas channel.
   * @n eGasCO reads the AIN0 baseline, eGasNO2 reads the AIN1 baseline, eGasNH3 reads the AIN2 baseline on GMA3021B.
   * @n Unsupported gas types return 0.
   * @n Read this value only after hasValidBaseline() returns true. When R0 was absent, a successful warm-up normally creates it, but a Flash failure can leave the baseline invalid.
   * @return Baseline voltage in mV.
   */
  uint16_t getBaselineVoltageMv(eGasType_t gasType);

private:
  /**
   * @union uFloatData_t
   * @brief Four-byte view of one little-endian float register.
   */
  typedef union {
    uint32_t value;
    uint8_t bytes[4];
    float floatValue;
  } uFloatData_t;

  bool readReg(uint8_t reg, uint8_t *pBuf, uint8_t len);
  bool writeReg(uint8_t reg, const uint8_t *pBuf, uint8_t len);
  bool writeCommand(uint8_t command, uint8_t param);
  bool waitCommandDone(uint16_t timeoutMs);
  uint8_t getWarmUpMinute(void);
  uint8_t getWarmUpState(void);
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
  bool _warmUpRestartPending;
};

#endif
