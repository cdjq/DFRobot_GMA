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

Python driver for DFRobot GMA gas sensor modules on Raspberry Pi. It uses I2C to detect the module, start and poll clean-air warm-up calibration, read ppm concentration data, and control sleep or wake-up mode.

* Supports GMA2021B modules for CO and NO2 measurement
* Supports GMA3021B modules for CO, NO2 and NH3 measurement
* Communicates through I2C addresses `0x75` to `0x78`
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

Gas concentration data is valid only after clean-air baseline calibration is complete. Before calibration, during warm-up, during sleep mode or after an I2C read failure, ppm reads may return `VALUE_NOT_READY` (`-8888.0`). When a gas type is not supported by the current hardware, the library returns `NO_SUPPORT_GAS` (`-9999.0`).

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
    @brief Start and poll clean-air warm-up calibration.
    @param minute int Warm-up time in minutes, minimum 1 minute.
    @return bool Calibration state.
    @retval True Calibration is done.
    @retval False Calibration is still running or command write failed.
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
    @return tuple (co, no2, nh3), unit ppm.
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
    @brief Read warm-up remaining seconds.
    @return int Remaining seconds.
  '''

def get_command_status(self):
  '''!
    @brief Read recent command status.
    @return int 0 idle, 1 busy, 2 done, 0x80 error.
  '''

def get_adc_voltage_mv(self, gas_type):
  '''!
    @brief Read current ADC voltage.
    @param gas_type int GAS_NO2 reads AIN0, GAS_CO reads AIN1.
    @return int ADC voltage in mV.
    @note Raw ADC voltage is mainly used for diagnostics. Gas concentration is calculated by the onboard MCU from the Rs/R0 ratio.
  '''

def get_baseline_voltage_mv(self, gas_type):
  '''!
    @brief Read clean-air baseline voltage.
    @param gas_type int GAS_NO2 reads AIN0 baseline, GAS_CO reads AIN1 baseline.
    @return int Baseline voltage in mV.
    @note Read this value after warm-up calibration is complete. The exposed baseline registers are AIN0 and AIN1.
  '''
```

## Examples

All examples keep the ADD_A1/ADD_A0 address switch table as code comments, print the detected hardware model with its meaning, then run clean-air baseline calibration. Keep the sensor in clean air until warm-up calibration is complete. After calibration, the examples print the CO and NO2 clean-air baseline voltages in mV.

* `get_gas_data.py`: Calibrate the module, then read CO, NO2 and NH3 ppm values one by one.
* `get_all_gas_data.py`: Calibrate the module, then read all gas ppm registers in one I2C transaction.
* `sleep_wake.py`: Calibrate the module, then enter sleep mode, check power state, and wake up the module.

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

* Date 2026-07-07
* Version V1.0.0

## Credits

Written by thdyyl<yuanlong.yu@dfrobot.com>, 2026. (Welcome to our website)
