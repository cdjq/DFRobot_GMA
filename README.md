# DFRobot_GMA

* [中文版本](./README_CN.md)

This Gravity MEMS gas sensor module (SKU: SEN0757 / SEN0758) runs gas sampling and concentration conversion on an onboard MCU, and the host reads the results over I2C. The module combines a MEMS gas-sensing element with a controller that handles ADC sampling, clean-air baseline (R0) calibration, Rs/R0 based ppm conversion and Flash storage. The host does not calculate gas curves itself; it reads status flags, raw ADC voltage, baseline voltage and gas concentration registers. Hardware models include GMA2021B (`0x21`, CO and NO2) and GMA3021B (`0x31`, CO, NO2 and NH3). The I2C address is selected by the ADD_A1 / ADD_A0 switches in the range `0x75`–`0x78`, and the default is `GMA_ADDRESS_3` (`0x78`, ADD_A1=1, ADD_A0=1). This library drives the module from Arduino and similar hosts: it reads CO, NO2 and NH3 concentrations, and handles warm-up and stability checking, baseline clearing, sleep/wake and soft reset. Example `getGasData` and `getAllGasData` read concentrations; `sleepWake` shows sleep and wake-up; `calibrateBaseline` creates a new R0 in verified clean air.

| ADD_A1 | ADD_A0 | Address |
| --- | --- | --- |
| 0 | 0 | `0x75` |
| 0 | 1 | `0x76` |
| 1 | 0 | `0x77` |
| 1 | 1 | `0x78` |

## Product Link ([https://www.dfrobot.com](https://www.dfrobot.com))

SKU: SEN0757 / SEN0758

## Table of Contents

* [Product Link](#product-link-httpswwwdfrobotcom)
* [Summary](#summary)
* [Installation](#installation)
* [Methods](#methods)
* [Compatibility](#compatibility)
* [History](#history)
* [Credits](#credits)

## Summary

This library reads CO, NO2 and NH3, and handles warm-up, baseline clearing, sleep and wake-up. Call `begin()`, then `warmUpTime()`, then read the gas data. For daily ppm use `getGasData` / `getAllGasData` (they warm up themselves; no prior calibration sketch is required). Use `calibrateBaseline` only to force a new clean-air R0. `sleepWake` shows the onboard MCU stopping and resuming sampling (compare power state and ppm before/after sleep; on SEN0757/SEN0758 the heater and LDO stay on, so module current does not drop much).

## Installation

There are two methods:

1. Search for DFRobot_GMA in the Arduino Library Manager and install it.
2. Download the library, extract it into the Arduino `libraries` directory, then open the `examples` folder and run a sketch.

This library depends only on the Arduino core and `Wire`.

A Raspberry Pi / Python version is in [`python/raspberrypi`](./python/raspberrypi/README.md).

## Methods

Firmware starts a 3-minute warm-up automatically at power-on. At the check point it uses the latest 30 one-second samples from each enabled channel, removes the two highest and two lowest values, and requires every retained sample to be within the current threshold (50mV by default) of the trimmed mean. An unstable window starts another 1-minute period and repeats the check until stable. `warmUpTime(3)` configures and polls this complete process; firmware V1.3 and later allow temporary threshold changes through `setWarmUpStableThresholdMv()`.

After stability is confirmed, an existing R0 is preserved and used for concentration output. If no R0 exists, the firmware automatically saves the trimmed mean of the final stable 30-second window as the clean-air baseline. Therefore a sensor without R0 must be warmed up in verified clean air. `clearBaseline()` only removes the old R0; the following `warmUpTime()` creates the replacement after the sensor becomes stable. Stability alone cannot determine whether the surrounding air is clean.

Gas concentration data is valid only after warm-up and baseline preparation are complete. Before calibration, during warm-up, during sleep mode or after an I2C read failure, ppm reads may return `GMA_VALUE_NOT_READY` (`-8888.0`). When a gas type is not supported by the current hardware, the library returns `GMA_NO_SUPPORT_GAS` (`-9999.0`). For example, GMA2021B does not support NH3, so `gma.getGasData(gma.eGasNH3)` returns `GMA_NO_SUPPORT_GAS`, and `getAllGasData(co, no2, nh3)` sets `nh3` to `GMA_NO_SUPPORT_GAS`.

Gas type is defined as a public enum inside `DFRobot_GMA`. In sketches and examples, use `gma.eGasCO`, `gma.eGasNO2` or `gma.eGasNH3` for better Arduino IDE autocomplete.

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
 * @param pWire I2C bus object.
 * @param i2cAddr Sensor I2C address.
 * @n pWire defaults to &Wire.
 * @n i2cAddr is GMA_ADDRESS_0 ~ GMA_ADDRESS_3. Default is GMA_ADDRESS_3 (0x78).
 */
DFRobot_GMA(TwoWire *pWire = &Wire, uint8_t i2cAddr = GMA_ADDRESS_3);

/**
 * @fn begin
 * @brief Initialize I2C communication and read the device ID and hardware model.
 * @n Call it once in setup() before any other method; it starts the I2C bus itself,
 * @n so the sketch does not need a separate Wire.begin(). The module must be powered
 * @n and the ADD_A1 / ADD_A0 switches must match i2cAddr.
 * @n Returns false when the device ID is not 0xA5 or the hardware model is neither
 * @n GMA_HW_GMA2021B nor GMA_HW_GMA3021B.
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
 * @n Firmware starts a 3-minute warm-up automatically at power-on. If the host
 * @n restarts while the sensor stays powered and reports COMPLETE, the first
 * @n warmUpTime() call starts a new warm-up. A running firmware warm-up is
 * @n adopted without resetting its countdown.
 * @n An existing R0 is preserved; when R0 is absent, the final stable window is saved automatically as the new clean-air baseline.
 * @n When no R0 exists, run this process only in verified clean air. The firmware cannot identify whether the surrounding air is actually clean.
 * @return Warm-up state.
 * @retval true Warm-up and stability checking are complete. If R0 was absent, the final stable window has been used to create it.
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
 * @brief Clear the saved R0 and enter the uncalibrated state.
 * @n This function only clears R0. Call warmUpTime() in verified clean air afterward; because no baseline exists, its final stable window is saved automatically as the new R0.
 * @return Clear result.
 * @retval true The Flash record and active R0 were cleared.
 * @retval false The I2C command or Flash erase failed.
 */
bool clearBaseline(void);

/**
 * @fn hasValidBaseline
 * @brief Check whether a clean-air R0 baseline is available.
 * @n Reads the status register on every call, so it costs one I2C transaction.
 * @n A register read failure is reported as false; treat that as uncalibrated and do
 * @n not use getBaselineVoltageMv() until a later call returns true.
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
 * @n On SEN0757/SEN0758 the heater and LDO stay powered, so overall module current does not drop much. Sleep only pauses MCU acquisition, not a system low-power cut.
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
 * @n The saved R0 is kept, but the onboard warm-up starts again from the beginning,
 * @n so call warmUpTime() and wait for true before reading ppm. The function waits
 * @n 80 ms after the command for the MCU to restart.
 * @return Command write result.
 * @retval true Command write succeeded.
 * @retval false I2C communication failed.
 */
bool softReset(void);

/**
 * @fn getStatus
 * @brief Read onboard MCU status bits.
 * @n Bits are GMA_STATUS_BASELINE_VALID, GMA_STATUS_WARMING, GMA_STATUS_DATA_VALID,
 * @n GMA_STATUS_SLEEP, GMA_STATUS_ADC_ERROR, GMA_STATUS_FLASH_ERROR and GMA_STATUS_FORMULA_MISSING.
 * @n A register read failure also returns 0, the same value as "no flag set"; use
 * @n getPowerState() (0xFF on failure) to tell a dead bus apart.
 * @return Bit mask of GMA_STATUS_xxx values, or 0 when the read fails.
 */
uint8_t getStatus(void);

/**
 * @fn getWarmUpRemainSeconds
 * @brief Read the remaining seconds in the current warm-up phase.
 * @n Returns the remaining seconds in the current initial or retry phase.
 * @n The firmware exposes 0 at an unstable-window boundary before starting the
 * @n next 1-minute retry. A completed or idle warm-up also returns 0, so use
 * @n warmUpTime() to decide whether the whole process is complete.
 * @n A register read failure also returns 0.
 * @return Remaining seconds, or 0 when warm-up is idle/complete or the read fails.
 */
uint16_t getWarmUpRemainSeconds(void);

/**
 * @fn getCommandStatus
 * @brief Read the execution status of the latest command.
 * @n 0 idle, 1 busy, 2 done, 0x80 error.
 * @n A register read failure returns 0x80, the same value as a failed command.
 * @return Command status, or 0x80 when the read fails.
 */
uint8_t getCommandStatus(void);

/**
 * @fn getADCVoltageMv
 * @brief Read the current ADC voltage of the selected gas channel.
 * @param gasType Gas channel.
 * @n eGasCO reads AIN0/ADC_CO, eGasNO2 reads AIN1/ADC_NO2, eGasNH3 reads AIN2/ADC_NH3 on GMA3021B.
 * @n Unsupported gas types return 0.
 * @n A register read failure also returns 0.
 * @n Raw ADC voltage is mainly used for diagnostics. Gas concentration is calculated by the onboard MCU from the Rs/R0 ratio.
 * @return Current voltage in mV, or 0 for an unsupported gas type or read failure.
 */
uint16_t getADCVoltageMv(eGasType_t gasType);

/**
 * @fn getBaselineVoltageMv
 * @brief Read the clean-air baseline voltage of the selected gas channel.
 * @param gasType Gas channel.
 * @n eGasCO reads the AIN0 baseline, eGasNO2 reads the AIN1 baseline, eGasNH3 reads the AIN2 baseline on GMA3021B.
 * @n Unsupported gas types return 0.
 * @n A register read failure also returns 0.
 * @n Read this value only after hasValidBaseline() returns true. When R0 was absent, a successful warm-up normally creates it, but a Flash failure can leave the baseline invalid.
 * @return Baseline voltage in mV, or 0 for an unsupported gas type, missing R0, or read failure.
 */
uint16_t getBaselineVoltageMv(eGasType_t gasType);
```

Status bits returned by `getStatus()`:

| Bit | Macro | Description |
| --- | --- | --- |
| `0x01` | `GMA_STATUS_BASELINE_VALID` | Clean-air baseline is valid |
| `0x02` | `GMA_STATUS_WARMING` | Warm-up/stability checking is running; R0 is not being generated |
| `0x04` | `GMA_STATUS_DATA_VALID` | Gas ppm data is valid |
| `0x08` | `GMA_STATUS_SLEEP` | Onboard MCU sampling stopped (sleep; heater/LDO still powered) |
| `0x10` | `GMA_STATUS_ADC_ERROR` | ADC sampling failed |
| `0x20` | `GMA_STATUS_FLASH_ERROR` | Flash storage failed |
| `0x40` | `GMA_STATUS_FORMULA_MISSING` | Gas formula is not configured in firmware |

## Compatibility

| Board              | Work Well | Work Wrong | Untested | Remarks |
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

- 2026/11/23 - V1.0.0 version

## Credits

Written by thdyyl(yuanlong.yu@dfrobot.com), 2026. (Welcome to our [website](https://www.dfrobot.com/))
