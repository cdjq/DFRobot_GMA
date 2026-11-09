'''!
  @file get_gas_data.py
  @brief Example: warm up the GMA module, then read CO / NO2 / NH3 ppm one gas at a time.
  @n Wiring (I2C, bus=1, address ADDRESS_3 / 0x78):
  @n VCC → 3.3V or 5V, per the module rating
  @n GND → GND
  @n SCL → SCL
  @n SDA → SDA
  @details Normal daily read demo. You do NOT need calibrate_baseline.py first.
  @n     Existing R0 is kept; if none exists, warm-up in clean air creates one.
  @n     Flow: begin() -> threshold -> hardware model -> warm_up_time(3) ->
  @n     print R0 -> loop get_gas_data() every 1 s.
  @n     remain 0 s alone is not finished; wait for "Warm-up complete...".
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
WARM_UP_STABLE_THRESHOLD_MV = 10


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
  # Poll until warm_up_time() is True. Remain 0 s alone is not finished.
  # Long unpowered time may extend warm-up. If no R0 yet, use clean air.
  is_baseline = gma.has_valid_baseline()
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

  baseline_valid = gma.has_valid_baseline()
  if baseline_valid is False:
    print("Warm-up complete, but the clean-air baseline is not available.")
  elif is_baseline:
    print("Warm-up complete. Output gas data.")
  else:
    print("Warm-up complete. The final stable voltage window has been saved as the clean-air baseline.")
  return baseline_valid


def format_gas_ppm(value):
  if value == DFRobot_GMA.NO_SUPPORT_GAS:
    return "not supported"
  if value == DFRobot_GMA.VALUE_NOT_READY:
    return "not ready"
  return "%.2f ppm" % value


def main():
  gma = DFRobot_GMA(bus=1, i2c_addr=I2C_ADDR)

  while gma.begin() is False:
    print("GMA sensor not found. Check ADD_A1/ADD_A0 address switches.")
    time.sleep(1)

  configure_warm_up_threshold(gma)
  hardware_model = print_hardware_model_info(gma)
  supports_nh3 = hardware_model == DFRobot_GMA.HW_GMA3021B

  if wait_for_warm_up(gma, supports_nh3) is False:
    return
  print_baseline_voltage_info(gma, supports_nh3)

  while True:
    co = gma.get_gas_data(DFRobot_GMA.GAS_CO)
    no2 = gma.get_gas_data(DFRobot_GMA.GAS_NO2)
    nh3 = gma.get_gas_data(DFRobot_GMA.GAS_NH3)
    print("CO: %s, NO2: %s, NH3: %s" %
          (format_gas_ppm(co), format_gas_ppm(no2), format_gas_ppm(nh3)))
    time.sleep(1)


if __name__ == "__main__":
  main()
