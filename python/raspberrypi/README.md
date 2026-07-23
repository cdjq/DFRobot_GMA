# DFRobot_GMA (Python, Raspberry Pi)

* [中文版](./README_CN.md)

DFRobot_GMA is a Python library for DFRobot Gravity MEMS gas sensor modules on Raspberry Pi. It reads CO, NO2 and NH3 gas concentration data from the onboard GMA controller through I2C.

The module uses an onboard MCU to manage ADC sampling, clean-air baseline calibration, Rs/R0 based ppm conversion and Flash storage. This Python library communicates only with the module's I2C register interface.

The host does not directly calculate gas curves. The onboard MCU stores the clean-air baseline and updates the ppm registers. The Python driver reads status flags, raw ADC voltage, baseline voltage and gas concentration values from the module.

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

Python driver for DFRobot GMA gas sensor modules on Raspberry Pi. It uses I2C to detect the module, poll its automatic stability-checked warm-up, read ppm concentration data, and control sleep or wake-up mode.

* Supports GMA2021B modules for CO and NO2 measurement
* Supports GMA3021B modules for CO, NO2 and NH3 measurement
* Communicates through I2C addresses `0x75` to `0x78`
* Repeats 1-minute warm-up periods until the sensor voltage is stable
* Preserves an existing R0, or automatically saves the final stable window when R0 is absent
* Clears an old R0 so the next clean-air warm-up can generate a replacement
* Reads single-gas ppm data or all gas ppm registers at once
* Reads hardware model, status bits, warm-up remaining time, command status, ADC voltage and baseline voltage
* Supports sleep mode, wake-up mode and soft reset
* Uses `NO_SUPPORT_GAS` and `VALUE_NOT_READY` to report unsupported gas types or invalid data states

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

## I2C Address

| ADD_A1 | ADD_A0 | Address |
| --- | --- | --- |
| 0 | 0 | `0x75` |
| 0 | 1 | `0x76` |
| 1 | 0 | `0x77` |
| 1 | 1 | `0x78` |

The default address used by the Python constructor and all examples is `ADDRESS_3` (`0x78`), which corresponds to `ADD_A1=1` and `ADD_A0=1`. If the address switches are changed, pass the matching address constant to `DFRobot_GMA()`.

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
    @param i2c_addr int Sensor I2C address, ADDRESS_0 ~ ADDRESS_3. Default is ADDRESS_3.
  '''

def begin(self):
  '''!
    @brief Check whether the GMA sensor is connected.
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
    @brief Configure and poll the onboard warm-up/stability process.
    @param minute int Initial warm-up duration in minutes; values below 1 are treated as 1.
    @return bool Warm-up state.
    @retval True Warm-up and stability checking are complete; if R0 was absent, the final stable window has been used to create it.
    @retval False Warm-up is running or the I2C command failed.
    @note Firmware starts a 3-minute warm-up automatically at power-on. If the host restarts while the sensor stays powered and reports COMPLETE, the first warm_up_time() call starts a new warm-up. A running firmware warm-up is adopted without resetting its countdown.
    @note Existing R0 is preserved; when R0 is absent, the final stable window is saved automatically.
    @warning When R0 is absent, run this process only in verified clean air.
  '''

def set_warm_up_stable_threshold_mv(self, value):
  '''!
    @brief Set the voltage-deviation threshold used by the warm-up stability check.
    @param value int Threshold in mV, from 1 to 50. A firmware reset restores the 50 mV default.
    @return bool Set result.
    @retval True The firmware accepted the value and readback matches it.
    @retval False The value is out of range, the command failed or readback did not match.
    @note If warm-up is already complete, the next warm_up_time() call starts a new warm-up so the new threshold is verified. A running warm-up continues and uses the new threshold at its next check.
  '''

def get_warm_up_stable_threshold_mv(self):
  '''!
    @brief Read the voltage-deviation threshold used by the warm-up stability check.
    @return int Threshold in mV, or 0 when the register read fails.
  '''

def clear_baseline(self):
  '''!
    @brief Clear the saved R0 and enter the uncalibrated state.
    @return bool Clear result.
    @retval True The Flash record and active R0 were cleared.
    @retval False The I2C command or Flash erase failed.
    @note This method only clears R0. Run warm_up_time() in verified clean air afterward; because no baseline exists, its final stable window is saved automatically as the new R0.
  '''

def has_valid_baseline(self):
  '''!
    @brief Check whether a clean-air R0 baseline is available.
    @return bool Baseline state.
    @retval True A valid R0 exists and can be used for concentration conversion.
    @retval False The module is uncalibrated.
  '''

def get_gas_data(self, gas_type):
  '''!
    @brief Read gas concentration in ppm.
    @param gas_type int GAS_CO, GAS_NO2 or GAS_NH3.
    @return float Gas concentration in ppm, NO_SUPPORT_GAS or VALUE_NOT_READY.
  '''

def get_all_gas_data(self):
  '''!
    @brief Read all gas concentration registers.
    @return tuple (co, no2, nh3), unit ppm. GMA2021B returns NO_SUPPORT_GAS for nh3.
  '''

def sleep_mode(self):
  '''!
    @brief Let the onboard MCU enter sleep mode.
    @note This function waits briefly for the power-state register to update.
  '''

def wake_up_mode(self):
  '''!
    @brief Wake up the onboard MCU and resume gas sampling.
    @note This function waits briefly for the power-state register to update.
  '''

def get_power_state(self):
  '''!
    @brief Read current power state.
    @return int SLEEP_MODE, WAKE_UP_MODE or 0xFF when read failed.
    @note This reads the latest power-state register snapshot from the module.
  '''

def soft_reset(self):
  '''!
    @brief Reset the onboard MCU without clearing user calibration data.
    @return bool Command write result.
  '''

def get_status(self):
  '''!
    @brief Read onboard MCU status bits.
    @return int STATUS_xxx bit mask.
  '''

def get_warm_up_remain_seconds(self):
  '''!
    @brief Read the remaining seconds in the current warm-up phase.
    @return int Remaining seconds in the initial or retry phase.
    @note The firmware reports 0 at an unstable-window boundary before a 1-minute retry. Complete and idle states also return 0; use warm_up_time() to distinguish completion.
  '''

def get_command_status(self):
  '''!
    @brief Read recent command status.
    @return int 0 idle, 1 busy, 2 done, 0x80 error.
  '''

def get_adc_voltage_mv(self, gas_type):
  '''!
    @brief Read current ADC voltage.
    @param gas_type int GAS_CO reads AIN0, GAS_NO2 reads AIN1, GAS_NH3 reads AIN2 on GMA3021B.
    @return int ADC voltage in mV.
    @note Raw ADC voltage is mainly used for diagnostics. Gas concentration is calculated by the onboard MCU from the Rs/R0 ratio.
  '''

def get_baseline_voltage_mv(self, gas_type):
  '''!
    @brief Read clean-air baseline voltage.
    @param gas_type int GAS_CO reads AIN0 baseline, GAS_NO2 reads AIN1 baseline, GAS_NH3 reads AIN2 baseline on GMA3021B.
    @return int Baseline voltage in mV.
    @note Read this value only after has_valid_baseline() returns True. When R0 was absent, a successful warm-up normally creates it, but a Flash failure can leave it invalid.
  '''

```

## Examples

All examples set and read back a 10mV stability threshold before warm-up. Change `WARM_UP_STABLE_THRESHOLD_MV` for debugging; the valid range is 1–50mV. The normal measurement examples adopt automatic warm-up, print raw channel voltages and report retries. A transient zero remaining-time read is ignored when identifying a new 1-minute retry. Without R0 the final stable window is saved automatically as the first clean-air baseline.

The Python driver retries each failed or incomplete I2C transfer up to three times with a short delay. If the host restarts while the powered sensor still exposes an old `COMPLETE` state, a fresh warm-up is requested and that old snapshot is not accepted as the result of the new request.

* `get_gas_data.py`: Wait for stable data, then read CO, NO2 and NH3 ppm values one by one.
* `get_all_gas_data.py`: Wait for stable data, then read all gas ppm registers in one I2C transaction.
* `sleep_wake.py`: Wait for stable data, then enter sleep mode, check power state, and wake up the module.
* `calibrate_baseline.py`: Run only in verified clean air. It clears an existing R0, then calls `warm_up_time()`; the firmware automatically saves the final stable window as the replacement R0.

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

* Date 2026-07-23
* Version V1.3.0

## Credits

Written by thdyyl<yuanlong.yu@dfrobot.com>, 2026. (Welcome to our website)
