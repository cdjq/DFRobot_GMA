'''!
  @file calibrate_baseline.py
  @brief Clear an old R0 when necessary, then let warm_up_time() create a new clean-air baseline.
  @copyright Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
  @license The MIT License (MIT)
  @version V1.3.0
  @date 2026-07-23
'''

import os
import sys
import time

sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from DFRobot_GMA import DFRobot_GMA


I2C_ADDR = DFRobot_GMA.ADDRESS_3
WARM_UP_STABLE_THRESHOLD_MV = 10


def configure_warm_up_threshold(gma):
  if gma.set_warm_up_stable_threshold_mv(WARM_UP_STABLE_THRESHOLD_MV):
    print("Warm-up stable threshold: %d mV" %
          gma.get_warm_up_stable_threshold_mv())
    return

  current_threshold = gma.get_warm_up_stable_threshold_mv()
  if current_threshold > 0:
    print("Failed to set the warm-up stable threshold. Current threshold: %d mV." %
          current_threshold)
  else:
    print("Failed to set or read the warm-up stable threshold.")


def print_raw_voltage_info(gma, supports_nh3):
  values = [
    "CO=%dmV" % gma.get_adc_voltage_mv(DFRobot_GMA.GAS_CO),
    "NO2=%dmV" % gma.get_adc_voltage_mv(DFRobot_GMA.GAS_NO2),
  ]
  if supports_nh3:
    values.append("NH3=%dmV" % gma.get_adc_voltage_mv(DFRobot_GMA.GAS_NH3))
  print("Raw voltage: " + ", ".join(values))


def print_baseline_voltage_info(gma, supports_nh3):
  values = [
    "CO=%dmV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_CO),
    "NO2=%dmV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_NO2),
  ]
  if supports_nh3:
    values.append("NH3=%dmV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_NH3))
  print("Saved R0 voltage: " + ", ".join(values))


def wait_for_warm_up(gma, supports_nh3):
  """Warm up, retry until stable, then automatically save R0 because no baseline exists."""
  last_valid_s = None
  while gma.warm_up_time(3) is False:
    remain_s = gma.get_warm_up_remain_seconds()
    if remain_s > 0:
      if (last_valid_s is not None and last_valid_s <= 5 and
          remain_s >= 55):
        print("Warm-up incomplete. Restart 1-minute warm-up.")
      last_valid_s = remain_s
    print_raw_voltage_info(gma, supports_nh3)
    print("Warm-up remain: %d s" % remain_s)
    time.sleep(1)


def main():
  gma = DFRobot_GMA(bus=1, i2c_addr=I2C_ADDR)
  while gma.begin() is False:
    print("GMA sensor not found. Check the I2C address switches.")
    time.sleep(1)

  configure_warm_up_threshold(gma)
  supports_nh3 = gma.get_hardware_model() == DFRobot_GMA.HW_GMA3021B
  print("Clean-air calibration requested.")
  print("Remove all target gases and keep airflow, temperature and humidity stable.")

  # Running this dedicated example is the user's explicit request to recalibrate.
  if gma.has_valid_baseline():
    if gma.clear_baseline() is False:
      print("Failed to clear the old R0. Calibration stopped.")
      return
    print("Old R0 cleared. The sensor is now uncalibrated.")

  wait_for_warm_up(gma, supports_nh3)

  if gma.has_valid_baseline():
    print("Clean-air R0 calibration complete. The final stable window was saved automatically.")
    print_baseline_voltage_info(gma, supports_nh3)
  else:
    print("Warm-up completed, but the clean-air R0 could not be saved.")


if __name__ == "__main__":
  main()
