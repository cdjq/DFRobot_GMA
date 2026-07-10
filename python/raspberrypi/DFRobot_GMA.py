'''!
  @file DFRobot_GMA.py
  @brief Raspberry Pi driver for DFRobot GMA gas sensor modules.
  @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
  @license     The MIT License (MIT)
  @author      thdyyl(yuanlong.yu@dfrobot.com)
  @version  V1.0.0
  @date  2026-07-07
  @url https://github.com/DFRobot/DFRobot_GMA
'''

import struct
import time

try:
  import smbus
except ImportError:
  import smbus2 as smbus


class DFRobot_GMA:
  '''!
    @brief DFRobot GMA gas sensor driver.
    @details The onboard MCU performs ADC sampling, clean-air calibration,
    @n ppm conversion and Flash storage. This class only communicates with it
    @n through I2C registers.
  '''

  ADDRESS_0 = 0x75
  ADDRESS_1 = 0x76
  ADDRESS_2 = 0x77
  ADDRESS_3 = 0x78

  # Hardware model values returned by get_hardware_model().
  # HW_GMA2021B (0x21): GMA2021B, supports CO and NO2.
  # HW_GMA3021B (0x31): GMA3021B, supports CO, NO2 and NH3.
  HW_GMA2021B = 0x21
  HW_GMA3021B = 0x31

  GAS_CO = 0x01
  GAS_NO2 = 0x02
  GAS_NH3 = 0x03

  SLEEP_MODE = 0x00
  WAKE_UP_MODE = 0x01

  NO_SUPPORT_GAS = -9999.0
  VALUE_NOT_READY = -8888.0

  STATUS_BASELINE_VALID = 0x01
  STATUS_WARMING = 0x02
  STATUS_DATA_VALID = 0x04
  STATUS_SLEEP = 0x08
  STATUS_ADC_ERROR = 0x10
  STATUS_FLASH_ERROR = 0x20
  STATUS_FORMULA_MISSING = 0x40

  _REG_DEVICE_ID = 0x00
  _REG_HW_MODEL = 0x01
  _REG_POWER_STATE = 0x04
  _REG_STATUS = 0x05
  _REG_WARMUP_REMAIN_L = 0x07
  _REG_COMMAND = 0x09
  _REG_COMMAND_PARAM = 0x0A
  _REG_COMMAND_STATUS = 0x0B
  _REG_CO_PPM = 0x10
  _REG_NO2_PPM = 0x14
  _REG_NH3_PPM = 0x18
  _REG_AIN0_MV_L = 0x1C
  _REG_AIN1_MV_L = 0x1E
  _REG_BASE_AIN0_MV_L = 0x20
  _REG_BASE_AIN1_MV_L = 0x22

  _DEVICE_ID = 0xA5

  _CMD_START_WARMUP = 0x01
  _CMD_SLEEP = 0x02
  _CMD_WAKEUP = 0x03
  _CMD_SOFT_RESET = 0x04

  _CMD_STATUS_DONE = 0x02
  _CMD_STATUS_ERROR = 0x80
  _POWER_STATE_TIMEOUT_S = 0.3
  _POWER_STATE_POLL_S = 0.02

  def __init__(self, bus=1, i2c_addr=ADDRESS_3):
    '''!
      @brief Module init.
      @param bus int Raspberry Pi I2C bus number.
      @param i2c_addr int Sensor I2C address, ADDRESS_0 ~ ADDRESS_3. Default is ADDRESS_3.
    '''
    self._bus = smbus.SMBus(bus)
    self._i2c_addr = i2c_addr
    self._hw_model = 0
    self._warm_up_minute = 0
    self._warm_up_active = False
    self._warm_up_done = False

  def begin(self):
    '''!
      @brief Check whether the GMA sensor is connected.
      @return bool Init result.
      @retval True Device is detected.
      @retval False I2C communication failed or device ID mismatch.
    '''
    device_id = self._read_u8(self._REG_DEVICE_ID, 0)
    if device_id != self._DEVICE_ID:
      return False
    self._hw_model = self.get_hardware_model()
    return self._hw_model in (self.HW_GMA2021B, self.HW_GMA3021B)

  def get_hardware_model(self):
    '''!
      @brief Read hardware model stored by the onboard MCU.
      @return int Hardware model register value.
      @retval HW_GMA2021B 0x21, GMA2021B dual-gas module, supports CO and NO2.
      @retval HW_GMA3021B 0x31, GMA3021B triple-gas module, supports CO, NO2 and NH3.
      @retval 0 Read failed or the hardware model is invalid.
    '''
    self._hw_model = self._read_u8(self._REG_HW_MODEL, 0)
    return self._hw_model

  def warm_up_time(self, minute=3):
    '''!
      @brief Start and poll clean-air warm-up calibration.
      @param minute int Warm-up time in minutes, minimum 1 minute.
      @return bool Calibration state.
      @retval True Calibration is done.
      @retval False Calibration is still running or command write failed.
    '''
    if minute < 1:
      minute = 1

    if self._warm_up_done is True and self._warm_up_active is False and self._warm_up_minute == minute:
      return True

    if self._warm_up_active is False or self._warm_up_minute != minute:
      if self._write_command(self._CMD_START_WARMUP, minute) is False:
        return False
      self._warm_up_minute = minute
      self._warm_up_active = True
      self._warm_up_done = False
      time.sleep(0.02)

    command_status = self.get_command_status()
    status = self.get_status()

    if command_status == self._CMD_STATUS_DONE and (status & self.STATUS_WARMING) == 0:
      self._warm_up_active = False
      self._warm_up_done = True
      return True

    if command_status == self._CMD_STATUS_ERROR:
      self._warm_up_active = False
      self._warm_up_done = False

    return False

  def get_gas_data(self, gas_type):
    '''!
      @brief Read gas concentration in ppm.
      @param gas_type int GAS_CO, GAS_NO2 or GAS_NH3.
      @return float Gas concentration in ppm, NO_SUPPORT_GAS or VALUE_NOT_READY.
    '''
    if gas_type == self.GAS_CO:
      return self._read_float(self._REG_CO_PPM, self.VALUE_NOT_READY)
    if gas_type == self.GAS_NO2:
      return self._read_float(self._REG_NO2_PPM, self.VALUE_NOT_READY)
    if gas_type == self.GAS_NH3:
      if self._hw_model == 0 or self._hw_model == self.HW_GMA2021B:
        self.get_hardware_model()
      if self._hw_model == self.HW_GMA2021B:
        return self.NO_SUPPORT_GAS
      return self._read_float(self._REG_NH3_PPM, self.VALUE_NOT_READY)
    return self.NO_SUPPORT_GAS

  def get_all_gas_data(self):
    '''!
      @brief Read all gas concentration registers.
      @return tuple (co, no2, nh3), unit ppm.
    '''
    data = self._read_reg(self._REG_CO_PPM, 12)
    if data is None:
      return (self.VALUE_NOT_READY, self.VALUE_NOT_READY, self.VALUE_NOT_READY)

    co = self._bytes_to_float(data[0:4])
    no2 = self._bytes_to_float(data[4:8])

    if self._hw_model == 0 or self._hw_model == self.HW_GMA2021B:
      self.get_hardware_model()
    if self._hw_model == self.HW_GMA2021B:
      nh3 = self.NO_SUPPORT_GAS
    else:
      nh3 = self._bytes_to_float(data[8:12])

    return (co, no2, nh3)

  def sleep_mode(self):
    '''!
      @brief Let the onboard MCU enter sleep mode.
      @note This function waits briefly for the power-state register to update.
    '''
    if self._write_command(self._CMD_SLEEP, 0) is True:
      self._wait_power_state(self.SLEEP_MODE, self._POWER_STATE_TIMEOUT_S)

  def wake_up_mode(self):
    '''!
      @brief Wake up the onboard MCU and resume gas sampling.
      @note This function waits briefly for the power-state register to update.
    '''
    if self._write_command(self._CMD_WAKEUP, 0) is True:
      self._warm_up_active = False
      self._wait_power_state(self.WAKE_UP_MODE, self._POWER_STATE_TIMEOUT_S)

  def get_power_state(self):
    '''!
      @brief Read current power state.
      @return int SLEEP_MODE, WAKE_UP_MODE or 0xFF when read failed.
      @note This reads the latest power-state register snapshot from the module.
    '''
    return self._read_u8(self._REG_POWER_STATE, 0xFF)

  def soft_reset(self):
    '''!
      @brief Reset the onboard MCU without clearing user calibration data.
      @return bool Command write result.
    '''
    ret = self._write_command(self._CMD_SOFT_RESET, 0)
    if ret is True:
      self._warm_up_active = False
      self._warm_up_done = False
      time.sleep(0.08)
    return ret

  def get_status(self):
    '''!
      @brief Read onboard MCU status bits.
      @return int STATUS_xxx bit mask.
    '''
    return self._read_u8(self._REG_STATUS, 0)

  def get_warm_up_remain_seconds(self):
    '''!
      @brief Read warm-up remaining seconds.
      @return int Remaining seconds.
    '''
    return self._read_u16(self._REG_WARMUP_REMAIN_L, 0)

  def get_command_status(self):
    '''!
      @brief Read recent command status.
      @return int 0 idle, 1 busy, 2 done, 0x80 error.
    '''
    return self._read_u8(self._REG_COMMAND_STATUS, self._CMD_STATUS_ERROR)

  def get_adc_voltage_mv(self, gas_type):
    '''!
      @brief Read current ADC voltage.
      @param gas_type int GAS_NO2 reads AIN0, GAS_CO reads AIN1.
      @return int ADC voltage in mV.
    '''
    if gas_type == self.GAS_NO2:
      return self._read_u16(self._REG_AIN0_MV_L, 0)
    if gas_type == self.GAS_CO:
      return self._read_u16(self._REG_AIN1_MV_L, 0)
    return 0

  def get_baseline_voltage_mv(self, gas_type):
    '''!
      @brief Read clean-air baseline voltage.
      @param gas_type int GAS_NO2 reads AIN0 baseline, GAS_CO reads AIN1 baseline.
      @return int Baseline voltage in mV.
      @note Read this value after warm-up calibration is complete. The exposed baseline registers are AIN0 and AIN1.
    '''
    if gas_type == self.GAS_NO2:
      return self._read_u16(self._REG_BASE_AIN0_MV_L, 0)
    if gas_type == self.GAS_CO:
      return self._read_u16(self._REG_BASE_AIN1_MV_L, 0)
    return 0

  def _read_reg(self, reg, length):
    try:
      return self._bus.read_i2c_block_data(self._i2c_addr, reg, length)
    except IOError:
      return None
    except OSError:
      return None

  def _write_reg(self, reg, data):
    try:
      self._bus.write_i2c_block_data(self._i2c_addr, reg, data)
      return True
    except IOError:
      return False
    except OSError:
      return False

  def _write_command(self, command, param):
    if self._write_reg(self._REG_COMMAND_PARAM, [param & 0xFF]) is False:
      return False
    return self._write_reg(self._REG_COMMAND, [command & 0xFF])

  def _wait_power_state(self, expected_state, timeout_s):
    start_time = time.time()

    while (time.time() - start_time) < timeout_s:
      if self.get_power_state() == expected_state:
        return True
      time.sleep(self._POWER_STATE_POLL_S)

    return False

  def _read_u8(self, reg, default_value):
    data = self._read_reg(reg, 1)
    if data is None:
      return default_value
    return data[0]

  def _read_u16(self, reg, default_value):
    data = self._read_reg(reg, 2)
    if data is None:
      return default_value
    return data[0] | (data[1] << 8)

  def _read_float(self, reg, default_value):
    data = self._read_reg(reg, 4)
    if data is None:
      return default_value
    return self._bytes_to_float(data)

  def _bytes_to_float(self, data):
    return struct.unpack('<f', bytes(data))[0]
