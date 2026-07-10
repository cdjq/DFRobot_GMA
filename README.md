# DFRobot_GMA

* [中文版](./README_CN.md)

DFRobot_GMA is an Arduino library for DFRobot Gravity MEMS gas sensor modules. It reads CO, NO2 and NH3 gas concentration data from the onboard GMA controller through I2C.

The module uses an onboard MCU to manage ADC sampling, clean-air baseline calibration, Rs/R0 based ppm conversion and Flash storage. This library communicates with the module firmware through a simple I2C register interface.

The library does not directly calculate gas curves on the host side. The onboard MCU stores the clean-air baseline and updates the ppm registers. The host reads status flags, raw ADC voltage, baseline voltage and gas concentration values from the module.

## Product Link ([https://www.dfrobot.com](https://www.dfrobot.com))

    SKU: SEN0757 / SEN0758

## Table of Contents

* [Summary](#summary)
* [Installation](#installation)
* [I2C Address](#i2c-address)
* [Methods](#methods)
* [Examples](#examples)
* [Compatibility](#compatibility)
* [History](#history)
* [Credits](#credits)

## Summary

* Supports GMA2021B modules for CO and NO2 measurement
* Supports GMA3021B modules for CO, NO2 and NH3 measurement
* Communicates with the onboard controller through I2C addresses `0x75` to `0x78`
* Starts and polls clean-air warm-up baseline calibration
* Reads single-gas ppm data or all gas ppm registers at once
* Reads hardware model, status bits, warm-up remaining time, command status, ADC voltage and baseline voltage
* Supports sleep mode, wake-up mode and soft reset
* Uses `GMA_NO_SUPPORT_GAS` and `GMA_VALUE_NOT_READY` to report unsupported gas types or invalid data states

## Installation

To use this library, download the zip file and extract it to the `libraries` directory under your Arduino sketchbook directory, or install it through the Arduino Library Manager when available.

1. Open Arduino IDE
2. Search for `DFRobot_GMA` in Library Manager
3. Click Install

Or install manually:

```bash
git clone https://github.com/DFRobot/DFRobot_GMA.git
```

This Arduino library only depends on the standard `Wire` library.

For Raspberry Pi usage, see `python/raspberrypi/README.md`.

## I2C Address

| ADD_A1 | ADD_A0 | Address |
| --- | --- | --- |
| 0 | 0 | `0x75` |
| 0 | 1 | `0x76` |
| 1 | 0 | `0x77` |
| 1 | 1 | `0x78` |

The default address used by the library constructor and all examples is `GMA_ADDRESS_3` (`0x78`), which corresponds to `ADD_A1=1` and `ADD_A0=1`. If the address switches are changed, pass the matching address constant to the constructor.

## Methods

Gas concentration data is valid only after clean-air baseline calibration is complete. Before calibration, during warm-up, during sleep mode or after an I2C read failure, ppm reads may return `GMA_VALUE_NOT_READY` (`-8888.0`). When a gas type is not supported by the current hardware, the library returns `GMA_NO_SUPPORT_GAS` (`-9999.0`). For example, GMA2021B does not support NH3, so `getGasData(eGasNH3)` returns `GMA_NO_SUPPORT_GAS`, and `getAllGasData(co, no2, nh3)` sets `nh3` to `GMA_NO_SUPPORT_GAS`.

Gas type is defined as a public enum inside `DFRobot_GMA`. Use `DFRobot_GMA::eGasCO`, `DFRobot_GMA::eGasNO2` or `DFRobot_GMA::eGasNH3`.

Hardware model values returned by `getHardwareModel()`:

| Return value | Macro | Module | Supported gases |
| --- | --- | --- | --- |
| `0x21` | `GMA_HW_GMA2021B` | GMA2021B | CO, NO2 |
| `0x31` | `GMA_HW_GMA3021B` | GMA3021B | CO, NO2, NH3 |
| `0x00` | - | Invalid or read failed | - |

```C++

typedef enum {
  eGasCO = 0x01,
  eGasNO2 = 0x02,
  eGasNH3 = 0x03
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
 * @note Raw ADC voltage is mainly used for diagnostics. Gas concentration is calculated by the onboard MCU from the Rs/R0 ratio.
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
```

Status bits returned by `getStatus()`:

| Bit | Macro | Description |
| --- | --- | --- |
| `0x01` | `GMA_STATUS_BASELINE_VALID` | Clean-air baseline is valid |
| `0x02` | `GMA_STATUS_WARMING` | Warm-up calibration is running |
| `0x04` | `GMA_STATUS_DATA_VALID` | Gas ppm data is valid |
| `0x08` | `GMA_STATUS_SLEEP` | The onboard MCU is sleeping |
| `0x10` | `GMA_STATUS_ADC_ERROR` | ADC sampling failed |
| `0x20` | `GMA_STATUS_FLASH_ERROR` | Flash storage failed |
| `0x40` | `GMA_STATUS_FORMULA_MISSING` | Gas formula is not configured in firmware |

## Examples

All examples keep the ADD_A1/ADD_A0 address switch table as code comments, print the detected hardware model with its meaning, then run clean-air baseline calibration. Keep the sensor in clean air until warm-up calibration is complete. After calibration, the examples print the CO and NO2 clean-air baseline voltages in mV.

* `getGasData`: Calibrate the module, then read CO, NO2 and NH3 ppm values one by one.
* `getAllGasData`: Calibrate the module, then read gas ppm registers in one I2C transaction. Use `getAllGasData(co, no2)` when NH3 is not needed or unsupported.
* `sleepWake`: Calibrate the module, then enter sleep mode, check power state, and wake up the module.

## Compatibility

| MCU                | Work Well | Work Wrong | Untested | Remarks |
| ------------------ |:---------:|:----------:|:--------:| ------- |
| Arduino uno        |     √     |            |          |         |
| Mega2560           |     √     |            |          |         |
| Leonardo           |     √     |            |          |         |
| ESP32              |     √     |            |          |         |
| ESP8266            |     √     |            |          |         |
| FireBeetle M0      |     √     |            |          |         |
| micro:bit          |     √     |            |          |         |
| Raspberry Pi 4B    |     √     |            |          | Python driver |

## History

* Date 2026-07-07
* Version V1.0.0

## Credits

Written by thdyyl<yuanlong.yu@dfrobot.com>, 2026. (Welcome to our website)
