# DFRobot_GMA (Python, Raspberry Pi)

* [中文版本](./README_CN.md)

This Gravity MEMS gas sensor module (SKU: SEN0757 / SEN0758) runs gas sampling and concentration conversion on an onboard MCU, and the host reads the results over I2C. The module combines a MEMS gas-sensing element with a controller that handles ADC sampling, clean-air baseline (R0) calibration, Rs/R0 based ppm conversion and Flash storage. The host does not calculate gas curves itself; it reads status flags, raw ADC voltage, baseline voltage and gas concentration registers. Hardware models include GMA2021B (`0x21`, CO and NO2) and GMA3021B (`0x31`, CO, NO2 and NH3). The I2C address is selected by the ADD_A1 / ADD_A0 switches in the range `0x75`–`0x78`, and the default is `ADDRESS_3` (`0x78`, ADD_A1=1, ADD_A0=1). This is the Raspberry Pi Python driver. It communicates only with the module's I2C register interface: it reads CO, NO2 and NH3 concentrations, and handles warm-up and stability checking, baseline clearing, sleep/wake and soft reset. Example `get_gas_data.py` and `get_all_gas_data.py` read concentrations; `sleep_wake.py` shows sleep and wake-up; `calibrate_baseline.py` creates a new R0 in verified clean air.

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

This library reads CO, NO2 and NH3 on Raspberry Pi, and handles warm-up, baseline clearing, sleep and wake-up. Call `begin()`, then `warm_up_time()`, then read the gas data. For daily ppm use `get_gas_data.py` / `get_all_gas_data.py` (they warm up themselves; no prior calibration script is required). Use `calibrate_baseline.py` only to force a new clean-air R0. `sleep_wake.py` shows the onboard MCU stopping and resuming sampling (compare power state and ppm before/after sleep; on SEN0757/SEN0758 the heater and LDO stay on, so module current does not drop much).

## Installation

Install the required packages on Raspberry Pi:

```bash
sudo apt update
sudo apt install -y i2c-tools
pip install smbus2
```

Fallback when `smbus2` cannot be installed:

```bash
sudo apt install -y python3-smbus
```

Enable I2C before running the examples:

```bash
sudo raspi-config
# Interface Options -> I2C -> Enable
```

To run an example:

```bash
cd python/raspberrypi/examples
python get_gas_data.py
```

Examples set and read back a 10mV stability threshold before warm-up. Change `warm_up_stable_threshold_mv` for debugging; the valid range is 1–50mV. The driver retries a failed or incomplete I2C transfer up to three times. If the host restarts while the powered sensor still exposes an old `COMPLETE` state, a fresh warm-up is requested and that old snapshot is not accepted as the result of the new request.

## Methods

Firmware starts a 3-minute warm-up automatically at power-on. At the check point it uses the latest 30 one-second samples from each enabled channel, removes the two highest and two lowest values, and requires every retained sample to be within the current threshold (50 mV by default) of the trimmed mean. An unstable window starts another 1-minute period and repeats the check until stable. `warm_up_time(3)` configures and polls this complete process; firmware V1.3 and later allow temporary threshold changes through `set_warm_up_stable_threshold_mv()`.

After stability is confirmed, an existing R0 is preserved and used for concentration output. If no R0 exists, the firmware automatically saves the trimmed mean of the final stable 30-second window as the clean-air baseline. Therefore a sensor without R0 must be warmed up in verified clean air. `clear_baseline()` only removes the old R0; the following `warm_up_time()` creates the replacement after the sensor becomes stable. Stability alone cannot determine whether the surrounding air is clean.

Gas concentration data is valid only after warm-up and baseline preparation are complete. Before calibration, during warm-up, during sleep mode or after an I2C read failure, ppm reads may return `VALUE_NOT_READY` (`-8888.0`). When a gas type is not supported by the current hardware, the library returns `NO_SUPPORT_GAS` (`-9999.0`).

Hardware model values returned by `get_hardware_model()`:

| Return value | Constant | Module | Supported gases |
| --- | --- | --- | --- |
| `0x21` | `HW_GMA2021B` | GMA2021B | CO, NO2 |
| `0x31` | `HW_GMA3021B` | GMA3021B | CO, NO2, NH3 |
| `0x00` | - | Invalid or read failed | - |

```python

def __init__(self, bus=1, i2c_addr=ADDRESS_3):
  '''!
    @brief Module init.
    @param bus int Raspberry Pi I2C bus number.
    @param i2c_addr int Sensor I2C address.
    @n i2c_addr is ADDRESS_0 ~ ADDRESS_3. Default is ADDRESS_3 (0x78). bus defaults to 1.
  '''

def begin(self):
  '''!
    @brief Check whether the GMA sensor is connected.
    @n Call it once before any other method. The module must be powered and the
    @n ADD_A1 / ADD_A0 switches must match i2c_addr.
    @n Returns False when the device ID is not 0xA5 or the hardware model is neither
    @n HW_GMA2021B nor HW_GMA3021B.
    @return bool Init result.
    @retval True Device is detected.
    @retval False I2C communication failed or device ID mismatch.
  '''

def get_hardware_model(self):
  '''!
    @brief Read hardware model stored by the onboard MCU.
    @return int Hardware model register value.
    @retval HW_GMA2021B 0x21, GMA2021B dual-gas module, supports CO and NO2.
    @retval HW_GMA3021B 0x31, GMA3021B triple-gas module, supports CO, NO2 and NH3.
    @retval 0 Read failed or the hardware model is invalid.
  '''

def warm_up_time(self, minute=3):
  '''!
    @brief Configure and poll the automatic warm-up/stability process.
    @param minute int Initial warm-up duration in minutes.
    @n Values below 1 are treated as 1. Default is 3 minutes.
    @n Firmware starts a 3-minute warm-up automatically at power-on. If the host
    @n restarts while the sensor stays powered and reports COMPLETE, the first
    @n warm_up_time() call starts a new warm-up. A running firmware warm-up is
    @n adopted without resetting its countdown.
    @n Existing R0 is preserved; when R0 is absent, the final stable window is saved automatically.
    @n When R0 is absent, use this method only in verified clean air. Stability does not prove that the air is clean.
    @return bool Warm-up state.
    @retval True Warm-up and stability checking are complete; if R0 was absent, the firmware has attempted to save the final stable window as R0.
    @retval False Warm-up is running or the I2C command failed.
  '''

def set_warm_up_stable_threshold_mv(self, value):
  '''!
    @brief Set the voltage-deviation threshold used by the warm-up stability check.
    @param value int Threshold in mV.
    @n Valid range 1–50. The setting is volatile and resets to 50 mV after a firmware reset.
    @n If warm-up is already complete, the next warm_up_time() call starts a new warm-up so the new threshold is verified. A running warm-up continues and uses the new threshold at its next check.
    @return bool Set result.
    @retval True The firmware accepted the value and readback matches it.
    @retval False The value is out of range, the command failed or readback did not match.
  '''

def get_warm_up_stable_threshold_mv(self):
  '''!
    @brief Read the voltage-deviation threshold used by the warm-up stability check.
    @n Returns the threshold in mV, or 0 when the register read fails.
    @return int Threshold in mV.
  '''

def clear_baseline(self):
  '''!
    @brief Clear the saved R0 and enter the uncalibrated state.
    @n This method only clears R0. Run warm_up_time() in verified clean air afterward; because no baseline exists, its final stable window will be saved automatically as the new R0.
    @return bool Clear result.
    @retval True The Flash record and active R0 were cleared.
    @retval False The I2C command or Flash erase failed.
  '''

def has_valid_baseline(self):
  '''!
    @brief Check whether a clean-air R0 baseline is available.
    @n Reads the status register on every call, so it costs one I2C transaction.
    @n A register read failure is reported as False; treat that as uncalibrated and do
    @n not use get_baseline_voltage_mv() until a later call returns True.
    @return bool Baseline state.
    @retval True A valid R0 exists and can be used for concentration conversion.
    @retval False The module is uncalibrated.
  '''

def get_gas_data(self, gas_type):
  '''!
    @brief Read gas concentration in ppm.
    @param gas_type int GAS_CO, GAS_NO2 or GAS_NH3.
    @n GMA2021B does not support GAS_NH3 and returns NO_SUPPORT_GAS.
    @return float Gas concentration in ppm.
    @retval NO_SUPPORT_GAS The current hardware does not support this gas.
    @retval VALUE_NOT_READY Data is not ready, or the module is uncalibrated, warming up or sleeping.
  '''

def get_all_gas_data(self):
  '''!
    @brief Read all gas concentration registers.
    @return tuple (co, no2, nh3), unit ppm.
    @n GMA2021B returns NO_SUPPORT_GAS for nh3.
    @n An I2C failure returns VALUE_NOT_READY for all three values.
  '''

def sleep_mode(self):
  '''!
    @brief Stop onboard MCU gas sampling (sleep).
    @n On SEN0757/SEN0758 the heater and LDO stay powered, so overall module current
    @n does not drop much. Sleep only pauses MCU acquisition, not a system low-power cut.
    @n This function waits briefly for the power-state register to update.
  '''

def wake_up_mode(self):
  '''!
    @brief Resume onboard MCU gas sampling after sleep.
    @n Heater/LDO were already on; this restarts acquisition on the MCU.
    @n This function waits briefly for the power-state register to update.
  '''

def get_power_state(self):
  '''!
    @brief Read whether the onboard MCU is sampling or stopped.
    @n This reads the latest power-state register snapshot from the module.
    @return int SLEEP_MODE (sampling stopped), WAKE_UP_MODE (sampling), or 0xFF on read failure.
  '''

def soft_reset(self):
  '''!
    @brief Reset the onboard MCU without clearing user calibration data.
    @n The saved R0 is kept, but the onboard warm-up starts again from the beginning,
    @n so call warm_up_time() and wait for True before reading ppm. The method waits
    @n 80 ms after the command for the MCU to restart.
    @return bool Command write result.
    @retval True Command write succeeded.
    @retval False I2C communication failed.
  '''

def get_status(self):
  '''!
    @brief Read onboard MCU status bits.
    @n Bits are STATUS_BASELINE_VALID, STATUS_WARMING, STATUS_DATA_VALID,
    @n STATUS_SLEEP, STATUS_ADC_ERROR, STATUS_FLASH_ERROR and STATUS_FORMULA_MISSING.
    @n A register read failure also returns 0, the same value as "no flag set"; use
    @n get_power_state() (0xFF on failure) to tell a dead bus apart.
    @return int STATUS_xxx bit mask, or 0 when the read fails.
  '''

def get_warm_up_remain_seconds(self):
  '''!
    @brief Read the remaining seconds in the current warm-up phase.
    @n Returns the remaining seconds in the initial or retry phase.
    @n The firmware reports 0 at an unstable-window boundary before a 1-minute retry. Complete and idle states also return 0; use warm_up_time() to distinguish completion.
    @n A register read failure also returns 0.
    @return int Remaining seconds, or 0 when warm-up is idle/complete or the read fails.
  '''

def get_command_status(self):
  '''!
    @brief Read recent command status.
    @n 0 idle, 1 busy, 2 done, 0x80 error.
    @n A register read failure returns 0x80, the same value as a failed command.
    @return int Command status, or 0x80 when the read fails.
  '''

def get_adc_voltage_mv(self, gas_type):
  '''!
    @brief Read current ADC voltage.
    @param gas_type int Gas channel.
    @n GAS_CO reads AIN0, GAS_NO2 reads AIN1, GAS_NH3 reads AIN2 on GMA3021B.
    @n Unsupported gas types return 0.
    @n A register read failure also returns 0.
    @n Raw ADC voltage is mainly used for diagnostics. Gas concentration is calculated by the onboard MCU from the Rs/R0 ratio.
    @return int ADC voltage in mV, or 0 for an unsupported gas type or read failure.
  '''

def get_baseline_voltage_mv(self, gas_type):
  '''!
    @brief Read clean-air baseline voltage.
    @param gas_type int Gas channel.
    @n GAS_CO reads AIN0 baseline, GAS_NO2 reads AIN1 baseline, GAS_NH3 reads AIN2 baseline on GMA3021B.
    @n Unsupported gas types return 0.
    @n A register read failure also returns 0.
    @n Read this value only after has_valid_baseline() returns True. When R0 was absent, a successful warm-up normally creates it, but a Flash failure can leave it invalid.
    @return int Baseline voltage in mV, or 0 for an unsupported gas type, missing R0, or read failure.
  '''

```

## Compatibility

| MCU           | Work Well | Work Wrong | Untested | Remarks |
| ------------- | :-------: | :--------: | :------: | ------- |
| Raspberry Pi  |     √     |            |          |         |

* Python version

| Python  | Work Well | Work Wrong | Untested | Remarks |
| ------- | :-------: | :--------: | :------: | ------- |
| Python2 |           |            |    √     |         |
| Python3 |     √     |            |          |         |

## History

- 2026/11/23 - V1.0.0 version

## Credits

Written by thdyyl(yuanlong.yu@dfrobot.com), 2026. (Welcome to our [website](https://www.dfrobot.com/))
