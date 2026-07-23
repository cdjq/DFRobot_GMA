'''!
  @file DFRobot_GMA.py
  @brief Raspberry Pi driver for DFRobot GMA gas sensor modules.
  @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
  @license     The MIT License (MIT)
  @author      thdyyl(yuanlong.yu@dfrobot.com)
  @version  V1.3.0
  @date  2026-07-23
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
  _WARMUP_STATE_IDLE = 0x00
  _WARMUP_STATE_RUNNING = 0x01
  _WARMUP_STATE_COMPLETE = 0x02

  _REG_DEVICE_ID = 0x00
  _REG_HW_MODEL = 0x01
  _REG_POWER_STATE = 0x04
  _REG_STATUS = 0x05
  _REG_WARMUP_MINUTE = 0x06
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
  _REG_AIN2_MV_L = 0x28
  _REG_BASE_AIN2_MV_L = 0x2A
  _REG_WARMUP_STATE = 0x2C
  _REG_WARMUP_STABLE_THRESHOLD_MV = 0x2D

  _DEVICE_ID = 0xA5

  _CMD_START_WARMUP = 0x01
  _CMD_SLEEP = 0x02
  _CMD_WAKEUP = 0x03
  _CMD_SOFT_RESET = 0x04
  _CMD_CLEAR_BASELINE = 0x05
  _CMD_SET_WARMUP_STABLE_THRESHOLD = 0x06

  _CMD_STATUS_DONE = 0x02
  _CMD_STATUS_ERROR = 0x80
  _POWER_STATE_TIMEOUT_S = 0.3
  _COMMAND_TIMEOUT_S = 1.0
  _POWER_STATE_POLL_S = 0.02
  _I2C_RETRY_COUNT = 3
  _I2C_RETRY_DELAY_S = 0.005
  _WARMUP_STABLE_THRESHOLD_MIN_MV = 1
  _WARMUP_STABLE_THRESHOLD_MAX_MV = 50

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
    self._warm_up_restart_pending = False

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
    if self._hw_model not in (self.HW_GMA2021B, self.HW_GMA3021B):
      return False
    self._warm_up_minute = 0
    self._warm_up_active = False
    self._warm_up_done = False
    self._warm_up_restart_pending = self._get_warm_up_state() != self._WARMUP_STATE_RUNNING
    return True

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
      @brief Configure and poll the automatic warm-up/stability process.
      @param minute int Initial warm-up duration in minutes; values below 1 are treated as 1.
      @return bool Warm-up state.
      @retval True Warm-up and stability checking are complete; if R0 was absent, the firmware has attempted to save the final stable window as R0.
      @retval False Warm-up is running or the I2C command failed.
      @note Firmware starts a 3-minute warm-up automatically at power-on. If the host restarts while the sensor stays powered and reports COMPLETE, the first warm_up_time() call starts a new warm-up. A running firmware warm-up is adopted without resetting its countdown.
      @note Existing R0 is preserved; when R0 is absent, the final stable window is saved automatically.
      @warning When R0 is absent, use this method only in verified clean air because stability does not prove that the air is clean.
    '''
    if minute < 1:
      minute = 1

    if self._warm_up_restart_pending is True:
      if self._write_command(self._CMD_START_WARMUP, minute) is False:
        return False
      self._warm_up_restart_pending = False
      self._warm_up_minute = minute
      self._warm_up_active = True
      self._warm_up_done = False
      # Do not accept the old COMPLETE snapshot in the same call. The firmware
      # executes I2C commands asynchronously in its main loop.
      return False

    command_status = self.get_command_status()
    status = self.get_status()
    firmware_minute = self._get_warm_up_minute()
    warm_up_state = self._get_warm_up_state()

    if (self._warm_up_done is True and self._warm_up_active is False and
        self._warm_up_minute == minute and
        (status & self.STATUS_WARMING) == 0 and
        warm_up_state == self._WARMUP_STATE_COMPLETE):
      return True

    if ((status & self.STATUS_WARMING) != 0 and
        warm_up_state == self._WARMUP_STATE_RUNNING and
        firmware_minute == minute):
      self._warm_up_minute = minute
      self._warm_up_active = True
      self._warm_up_done = False
    elif self._warm_up_active is False or self._warm_up_minute != minute:
      if self._write_command(self._CMD_START_WARMUP, minute) is False:
        return False
      self._warm_up_minute = minute
      self._warm_up_active = True
      self._warm_up_done = False
      # Wait for a later poll to observe RUNNING before COMPLETE is accepted.
      return False

    if (self._warm_up_active is True and
        command_status == self._CMD_STATUS_DONE and
        (status & self.STATUS_WARMING) == 0 and
        warm_up_state == self._WARMUP_STATE_COMPLETE):
      self._warm_up_active = False
      self._warm_up_done = True
      return True

    if (command_status == self._CMD_STATUS_ERROR and
        ((status & self.STATUS_WARMING) == 0 or
         warm_up_state != self._WARMUP_STATE_RUNNING)):
      self._warm_up_active = False
      self._warm_up_done = False

    return False

  def clear_baseline(self):
    '''!
      @brief Clear the saved R0 and enter the uncalibrated state.
      @return bool Clear result.
      @retval True The Flash record and active R0 were cleared.
      @retval False The I2C command or Flash erase failed.
      @note This method only clears R0. Run warm_up_time() in verified clean air afterward; because no baseline exists, its final stable window will be saved automatically as the new R0.
    '''
    if self._write_command(self._CMD_CLEAR_BASELINE, 0) is False:
      return False
    if self._wait_command_done(self._COMMAND_TIMEOUT_S) is False:
      return False
    self._warm_up_minute = 0
    self._warm_up_active = False
    self._warm_up_done = False
    return self.has_valid_baseline() is False

  def set_warm_up_stable_threshold_mv(self, value):
    '''!
      @brief Set the voltage-deviation threshold used by the warm-up stability check.
      @param value int Threshold in mV, from 1 to 50. The setting is volatile and resets to 50 mV after a firmware reset.
      @return bool Set result.
      @retval True The firmware accepted the value and readback matches it.
      @retval False The value is out of range, the command failed or readback did not match.
      @note If warm-up is already complete, the next warm_up_time() call starts a new warm-up so the new threshold is verified. A running warm-up continues and uses the new threshold at its next check.
    '''
    if (value < self._WARMUP_STABLE_THRESHOLD_MIN_MV or
        value > self._WARMUP_STABLE_THRESHOLD_MAX_MV):
      return False
    if self._write_command(self._CMD_SET_WARMUP_STABLE_THRESHOLD, value) is False:
      return False
    if self._wait_command_done(self._COMMAND_TIMEOUT_S) is False:
      return False
    if self.get_warm_up_stable_threshold_mv() != value:
      return False
    self._warm_up_restart_pending = self._get_warm_up_state() != self._WARMUP_STATE_RUNNING
    if self._warm_up_restart_pending is True:
      self._warm_up_minute = 0
      self._warm_up_active = False
      self._warm_up_done = False
    return True

  def get_warm_up_stable_threshold_mv(self):
    '''!
      @brief Read the voltage-deviation threshold used by the warm-up stability check.
      @return int Threshold in mV, or 0 when the register read fails.
    '''
    return self._read_u8(self._REG_WARMUP_STABLE_THRESHOLD_MV, 0)

  def has_valid_baseline(self):
    '''!
      @brief Check whether a clean-air R0 baseline is available.
      @return bool Baseline state.
      @retval True A valid R0 exists and can be used for concentration conversion.
      @retval False The module is uncalibrated.
    '''
    return (self.get_status() & self.STATUS_BASELINE_VALID) != 0

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
      @return tuple (co, no2, nh3), unit ppm. GMA2021B returns NO_SUPPORT_GAS for nh3.
    '''
    data = self._read_reg(self._REG_CO_PPM, 12)
    if data is None:
      return (self.VALUE_NOT_READY, self.VALUE_NOT_READY, self.VALUE_NOT_READY)

    co = self._bytes_to_float(data[0:4])
    no2 = self._bytes_to_float(data[4:8])

    if self._hw_model == 0:
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
      self._warm_up_active = False
      self._warm_up_done = False
      self._wait_power_state(self.SLEEP_MODE, self._POWER_STATE_TIMEOUT_S)

  def wake_up_mode(self):
    '''!
      @brief Wake up the onboard MCU and resume gas sampling.
      @note This function waits briefly for the power-state register to update.
    '''
    if self._write_command(self._CMD_WAKEUP, 0) is True:
      self._warm_up_active = False
      self._warm_up_done = False
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
      @brief Read the remaining seconds in the current warm-up phase.
      @return int Remaining seconds in the initial or retry phase.
      @note The firmware reports 0 at an unstable-window boundary before a 1-minute retry. Complete and idle states also return 0; use warm_up_time() to distinguish completion.
    '''
    return self._read_u16(self._REG_WARMUP_REMAIN_L, 0)

  def _get_warm_up_minute(self):
    return self._read_u8(self._REG_WARMUP_MINUTE, 0)

  def _get_warm_up_state(self):
    return self._read_u8(self._REG_WARMUP_STATE, self._WARMUP_STATE_IDLE)

  def get_command_status(self):
    '''!
      @brief Read recent command status.
      @return int 0 idle, 1 busy, 2 done, 0x80 error.
    '''
    return self._read_u8(self._REG_COMMAND_STATUS, self._CMD_STATUS_ERROR)

  def get_adc_voltage_mv(self, gas_type):
    '''!
      @brief Read current ADC voltage.
      @param gas_type int GAS_CO reads AIN0, GAS_NO2 reads AIN1, GAS_NH3 reads AIN2 on GMA3021B.
      @return int ADC voltage in mV.
    '''
    if gas_type == self.GAS_CO:
      return self._read_u16(self._REG_AIN0_MV_L, 0)
    if gas_type == self.GAS_NO2:
      return self._read_u16(self._REG_AIN1_MV_L, 0)
    if gas_type == self.GAS_NH3:
      if self._hw_model == 0:
        self.get_hardware_model()
      if self._hw_model == self.HW_GMA3021B:
        return self._read_u16(self._REG_AIN2_MV_L, 0)
    return 0

  def get_baseline_voltage_mv(self, gas_type):
    '''!
      @brief Read clean-air baseline voltage.
      @param gas_type int GAS_CO reads AIN0 baseline, GAS_NO2 reads AIN1 baseline, GAS_NH3 reads AIN2 baseline on GMA3021B.
      @return int Baseline voltage in mV.
      @note Read this value only after has_valid_baseline() returns True. When R0 was absent, a successful warm-up normally creates it, but a Flash failure can leave it invalid.
    '''
    if gas_type == self.GAS_CO:
      return self._read_u16(self._REG_BASE_AIN0_MV_L, 0)
    if gas_type == self.GAS_NO2:
      return self._read_u16(self._REG_BASE_AIN1_MV_L, 0)
    if gas_type == self.GAS_NH3:
      if self._hw_model == 0:
        self.get_hardware_model()
      if self._hw_model == self.HW_GMA3021B:
        return self._read_u16(self._REG_BASE_AIN2_MV_L, 0)
    return 0

  def _read_reg(self, reg, length):
    for attempt in range(self._I2C_RETRY_COUNT):
      try:
        data = self._bus.read_i2c_block_data(self._i2c_addr, reg, length)
        if data is not None and len(data) == length:
          return data
      except (IOError, OSError):
        pass

      if attempt + 1 < self._I2C_RETRY_COUNT:
        time.sleep(self._I2C_RETRY_DELAY_S)

    return None

  def _write_reg(self, reg, data):
    for attempt in range(self._I2C_RETRY_COUNT):
      try:
        self._bus.write_i2c_block_data(self._i2c_addr, reg, data)
        return True
      except (IOError, OSError):
        pass

      if attempt + 1 < self._I2C_RETRY_COUNT:
        time.sleep(self._I2C_RETRY_DELAY_S)

    return False

  def _write_command(self, command, param):
    if self._write_reg(self._REG_COMMAND_PARAM, [param & 0xFF]) is False:
      return False
    return self._write_reg(self._REG_COMMAND, [command & 0xFF])

  def _wait_command_done(self, timeout_s):
    deadline = time.monotonic() + timeout_s

    while time.monotonic() < deadline:
      time.sleep(self._POWER_STATE_POLL_S)
      command_status = self.get_command_status()
      if command_status == self._CMD_STATUS_DONE:
        return True
      if command_status == self._CMD_STATUS_ERROR:
        return False

    return False

  def _wait_power_state(self, expected_state, timeout_s):
    deadline = time.monotonic() + timeout_s

    while time.monotonic() < deadline:
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
