'''!
@file sleep_wake.py
@brief Example: stop and resume onboard MCU gas sampling with sleep_mode / wake_up_mode.
@n Wiring (I2C, bus=1, address ADDRESS_3 / 0x78):
@n VCC → 3.3V or 5V, per the module rating
@n GND → GND
@n SCL → SCL
@n SDA → SDA
@details Purpose: show that sleep stops MCU acquisition (ppm becomes not ready),
@n     and wake-up resumes sampling. On SEN0757/SEN0758 the heater and LDO stay
@n     powered, so overall module current does not drop much; this is not a
@n     system-level low-power demo.
@n     Flow: begin() -> warm-up -> loop:
@n     print power state + CO ppm -> sleep_mode() -> print again ->
@n     wake_up_mode() -> short wait -> print again.
@n     Parameter ranges are in the method comments of DFRobot_GMA.py.
@copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
@license     The MIT License (MIT)
@author      [thdyyl](yuanlong.yu@dfrobot.com)
@version  V1.0.0
@date  2026-11-23
@url https://github.com/DFRobot/DFRobot_GMA
'''

import os
import sys
import time

sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from DFRobot_GMA import DFRobot_GMA


# I2C address switch table:
# ADD_A1 ADD_A0 -> Address
# 0      0      -> ADDRESS_0 (0x75)
# 0      1      -> ADDRESS_1 (0x76)
# 1      0      -> ADDRESS_2 (0x77)
# 1      1      -> ADDRESS_3 (0x78)
# Demo default: ADDRESS_3 (ADD_A1=1, ADD_A0=1, 0x78)
i2c_addr = DFRobot_GMA.ADDRESS_3
# Demo threshold 10 mV (valid 1-50). Firmware default after reset is 50 mV.
warm_up_stable_threshold_mv = 10


# Hardware model return value:
# HW_GMA2021B (0x21) -> GMA2021B, supports CO and NO2.
# HW_GMA3021B (0x31) -> GMA3021B, supports CO, NO2 and NH3.
# 0 or other values   -> Read failed or unknown hardware model.
def print_hardware_model_info(gma):
  hardware_model = gma.get_hardware_model()

  if hardware_model == DFRobot_GMA.HW_GMA2021B:
    model_info = "GMA2021B, CO/NO2"
  elif hardware_model == DFRobot_GMA.HW_GMA3021B:
    model_info = "GMA3021B, CO/NO2/NH3"
  else:
    model_info = "Unknown"

  print("Hardware model: 0x%02X (%s)" % (hardware_model, model_info))
  return hardware_model


def configure_warm_up_threshold(gma):
  if gma.set_warm_up_stable_threshold_mv(warm_up_stable_threshold_mv):
    print("Warm-up stable threshold: %d mV" % gma.get_warm_up_stable_threshold_mv())
    return

  current_threshold = gma.get_warm_up_stable_threshold_mv()
  if current_threshold > 0:
    print("Failed to set the warm-up stable threshold. Current threshold: %d mV." % current_threshold)
  else:
    print("Failed to set or read the warm-up stable threshold.")


def print_baseline_voltage_info(gma, supports_nh3):
  print("CO baseline voltage: %d mV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_CO))
  print("NO2 baseline voltage: %d mV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_NO2))
  if supports_nh3:
    print("NH3 baseline voltage: %d mV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_NH3))


def print_raw_voltage_info(gma, supports_nh3):
  values = [
    "CO=%dmV" % gma.get_adc_voltage_mv(DFRobot_GMA.GAS_CO),
    "NO2=%dmV" % gma.get_adc_voltage_mv(DFRobot_GMA.GAS_NO2),
  ]
  if supports_nh3:
    values.append("NH3=%dmV" % gma.get_adc_voltage_mv(DFRobot_GMA.GAS_NH3))
  print("Raw voltage: " + ", ".join(values))


def wait_for_warm_up(gma, supports_nh3):
  # Warm-up before sleep/wake. Remain 0 s alone is not finished.
  is_baseline = gma.has_valid_baseline()
  last_valid_s = None
  while gma.warm_up_time(3) is False:
    remain_s = gma.get_warm_up_remain_seconds()
    if remain_s > 0:
      if last_valid_s is not None and last_valid_s <= 5 and remain_s >= 55:
        print("Warm-up incomplete. Restart 1-minute warm-up.")
      last_valid_s = remain_s
    print_raw_voltage_info(gma, supports_nh3)
    print("Warm-up remain: %d s" % remain_s)
    time.sleep(1)

  baseline_valid = gma.has_valid_baseline()
  if baseline_valid is False:
    print("Warm-up complete, but the clean-air baseline is not available.")
  elif is_baseline:
    print("Warm-up complete.")
  else:
    print("Warm-up complete. The final stable voltage window has been saved as the clean-air baseline.")
  return baseline_valid


def format_power_state(gma):
  power_state = gma.get_power_state()
  if power_state == DFRobot_GMA.SLEEP_MODE:
    return "sleep"
  if power_state == DFRobot_GMA.WAKE_UP_MODE:
    return "wake-up"
  return "read failed, raw=0x%02X" % power_state


def format_gas_ppm(value):
  if value == DFRobot_GMA.NO_SUPPORT_GAS:
    return "not supported"
  if value == DFRobot_GMA.VALUE_NOT_READY:
    return "not ready"
  return "%.2f ppm" % value


def print_sampling_snapshot(gma, phase):
  # While sleeping, get_gas_data() is expected to report not ready.
  co = gma.get_gas_data(DFRobot_GMA.GAS_CO)  # pylint: disable=invalid-name
  print("%s | Power state: %s | CO: %s" % (phase, format_power_state(gma), format_gas_ppm(co)))


def main():
  gma = DFRobot_GMA(bus=1, i2c_addr=i2c_addr)

  while gma.begin() is False:
    print("GMA sensor not found. Check ADD_A1/ADD_A0 address switches.")
    time.sleep(1)

  configure_warm_up_threshold(gma)
  hardware_model = print_hardware_model_info(gma)
  supports_nh3 = hardware_model == DFRobot_GMA.HW_GMA3021B

  if wait_for_warm_up(gma, supports_nh3):
    print_baseline_voltage_info(gma, supports_nh3)
  # Heater/LDO stay on in sleep; this demo shows sampling stop/resume, not current savings.

  while True:
    print_sampling_snapshot(gma, "Before sleep")

    gma.sleep_mode()
    print_sampling_snapshot(gma, "After sleep")
    time.sleep(3)

    gma.wake_up_mode()
    # Give the MCU a short time to resume acquisition before reading ppm.
    time.sleep(2)
    print_sampling_snapshot(gma, "After wake-up")

    time.sleep(5)


if __name__ == "__main__":
  main()
