'''!
  @file sleep_wake.py
  @brief Control sleep and wake-up modes of the GMA sensor.
  @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
  @license     The MIT License (MIT)
  @author      thdyyl(yuanlong.yu@dfrobot.com)
  @version  V1.0.0
  @date  2026-07-07
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


def print_baseline_voltage_info(gma):
  print("CO baseline voltage: %d mV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_CO))
  print("NO2 baseline voltage: %d mV" % gma.get_baseline_voltage_mv(DFRobot_GMA.GAS_NO2))


def print_power_state(gma, label):
  power_state = gma.get_power_state()

  if power_state == DFRobot_GMA.SLEEP_MODE:
    state_text = "sleep"
  elif power_state == DFRobot_GMA.WAKE_UP_MODE:
    state_text = "wake-up"
  else:
    state_text = "read failed, raw=0x%02X" % power_state

  print("%s: %s" % (label, state_text))


def main():
  gma = DFRobot_GMA(bus=1, i2c_addr=I2C_ADDR)

  while gma.begin() is False:
    print("GMA sensor not found. Check ADD_A1/ADD_A0 address switches.")
    time.sleep(1)

  print_hardware_model_info(gma)

  print("Keep the sensor in clean air.")
  while gma.warm_up_time(3) is False:
    print("Warm-up remain: %d s" % gma.get_warm_up_remain_seconds())
    time.sleep(1)

  print("Warm-up calibration done.")
  print_baseline_voltage_info(gma)

  while True:
    gma.sleep_mode()
    print_power_state(gma, "Power state after sleep")

    time.sleep(5)

    gma.wake_up_mode()
    print_power_state(gma, "Power state after wake-up")

    time.sleep(5)


if __name__ == "__main__":
  main()
