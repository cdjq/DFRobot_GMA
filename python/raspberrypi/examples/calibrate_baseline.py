'''!
  @file calibrate_baseline.py
  @brief Example: force a new clean-air R0 by clearing the old baseline, then running warm-up.
  @n Wiring (I2C, bus=1, address ADDRESS_3 / 0x78):
  @n VCC → 3.3V or 5V, per the module rating
  @n GND → GND
  @n SCL → SCL
  @n SDA → SDA
  @details When to use this demo:
  @n     - Force a new clean-air R0 only (wrong old baseline, new environment, etc.).
  @n     - Daily ppm reading does NOT need this first. Use get_gas_data / get_all_gas_data.
  @n     Run only in verified clean air. Stability does not prove the air is clean.
  @n     Flow: begin() -> optional threshold -> clear_baseline() if R0 exists ->
  @n     poll warm_up_time(3) -> firmware saves new R0 -> print Saved R0 voltage.
  @n     remain 0 s alone is not finished. Success prints calibration complete.
  @n     If clear fails after a host-only reset, power-cycle the module and retry.
  @n     Parameter ranges are in the method comments of DFRobot_GMA.py.
  @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
  @license     The MIT License (MIT)
  @author      [thdyyl](yuanlong.yu@dfrobot.com)
  @version  V1.0.0
  @date  2026-07-23
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
I2C_ADDR = DFRobot_GMA.ADDRESS_3
# Demo threshold 10 mV (valid 1-50). Firmware default after reset is 50 mV.
WARM_UP_STABLE_THRESHOLD_MV = 10


def configure_warm_up_threshold(gma):
  # Optional. Failure is non-fatal; firmware keeps default 50 mV.
  # If it keeps failing after a host-only reset, power-cycle the GMA module.
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
  # Exit only when warm_up_time() returns True; remain 0 s is not completion.
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

  # Force recalibration in verified clean air only.
  # Clear existing R0 so the next warm-up must create a new one.
  # If clear fails, power-cycle the module and run again. If no R0 yet, skip clear.
  if gma.has_valid_baseline():
    if gma.clear_baseline() is False:
      print("Failed to clear the old R0. Calibration stopped.")
      return
    print("Old R0 cleared. The sensor is now uncalibrated.")

  wait_for_warm_up(gma, supports_nh3)

  # Success: has_valid_baseline() and Saved R0 voltages. Then use get_gas_data / get_all_gas_data.
  if gma.has_valid_baseline():
    print("Clean-air R0 calibration complete. The final stable window was saved automatically.")
    print_baseline_voltage_info(gma, supports_nh3)
  else:
    print("Warm-up completed, but the clean-air R0 could not be saved.")


if __name__ == "__main__":
  main()
