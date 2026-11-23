# DFRobot_GMA（Python，Raspberry Pi）

* [English Version](./README.md)

这款 Gravity MEMS 气体传感器模组（SKU：SEN0757 / SEN0758）在板载 MCU 上完成气体采样与浓度换算，主机通过 I2C 读取结果。模组内置 MEMS 气敏元件与控制器：控制器负责 ADC 采样、洁净空气零点（R0）校准、基于 Rs/R0 比例的 ppm 换算以及 Flash 数据保存；主控端不直接计算气体曲线，而是读取状态位、原始 ADC 电压、基准电压和气体浓度寄存器。硬件型号分为 GMA2021B（`0x21`，支持 CO、NO2）与 GMA3021B（`0x31`，支持 CO、NO2、NH3）。I2C 地址由拨码开关 ADD_A1 / ADD_A0 选择，范围为 `0x75`～`0x78`，默认 `ADDRESS_3`（`0x78`，ADD_A1=1、ADD_A0=1）。本库是树莓派上的 Python 驱动，只与模块的 I2C 寄存器接口通信：读取 CO、NO2、NH3 浓度，并完成预热与稳定性检查、基线清除、休眠/唤醒和软复位。示例 `get_gas_data.py`、`get_all_gas_data.py` 读取浓度，`sleep_wake.py` 演示休眠与唤醒，`calibrate_baseline.py` 在确认的洁净空气中重新生成 R0。

| ADD_A1 | ADD_A0 | 地址 |
| --- | --- | --- |
| 0 | 0 | `0x75` |
| 0 | 1 | `0x76` |
| 1 | 0 | `0x77` |
| 1 | 1 | `0x78` |

## 产品链接 ([https://www.dfrobot.com.cn](https://www.dfrobot.com.cn))

SKU：SEN0757 / SEN0758

## 目录

* [产品链接](#产品链接-httpswwwdfrobotcomcn)
* [概述](#概述)
* [库安装](#库安装)
* [方法](#方法)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)

## 概述

本库在树莓派上读取 CO、NO2 和 NH3 浓度，并完成预热、基线清除、休眠与唤醒。先调用 `begin()`，再用 `warm_up_time()` 等待预热完成，然后读取气体数据。日常读数用 `get_gas_data.py` / `get_all_gas_data.py`（自带预热，一般不必先校准）；`calibrate_baseline.py` 仅在需要强制重做洁净空气 R0 时使用；`sleep_wake.py` 演示板载 MCU 停止/恢复采集（对比休眠前后电源状态与 ppm；SEN0757/SEN0758 加热与 LDO 常开，整机功耗不会明显下降）。

## 库安装

在 Raspberry Pi 上安装依赖：

```bash
sudo apt update
sudo apt install -y i2c-tools
pip install smbus2
```

无法安装 `smbus2` 时可使用的兜底依赖：

```bash
sudo apt install -y python3-smbus
```

运行示例前，请先打开 Raspberry Pi 的 I2C：

```bash
sudo raspi-config
# Interface Options -> I2C -> Enable
```

运行示例：

```bash
cd python/raspberrypi/examples
python get_gas_data.py
```

示例在预热前会设置并回读 10mV 稳定阈值。调试时可修改 `WARM_UP_STABLE_THRESHOLD_MV`，有效范围为 1～50mV。驱动会对失败或长度不完整的 I2C 传输最多重试三次。主控复位但传感器仍保持供电、并暴露旧的 `COMPLETE` 状态时，驱动会请求新一轮预热，不会把该旧快照当成本轮结果。

## 方法

固件上电后自动开始 3 分钟预热。到达检查点后，各启用通道取最近 30 个一秒样本，去掉两个最高值和两个最低值，要求其余样本与修剪均值的偏差不超过当前阈值（默认 50mV）。窗口不稳定时自动再预热 1 分钟并复检，直到稳定。`warm_up_time(3)` 负责配置并轮询这一完整过程；固件 V1.3 及以上可通过 `set_warm_up_stable_threshold_mv()` 临时调整阈值。

确认稳定后，有 R0 就保留并使用已有 R0 输出浓度；没有 R0 时，固件自动把最后 30 秒稳定窗口的修剪均值保存为洁净空气基线。因此没有 R0 的传感器必须在确认的洁净空气中预热。`clear_baseline()` 本身只清除旧 R0，随后调用 `warm_up_time()`，稳定后就会自动生成替代基线。稳定性检查只能判断电压是否稳定，不能判断环境中是否没有目标气体。

气体浓度数据只有在预热和基线准备完成后才有效。未校准、预热中、休眠中或 I2C 读取失败时，ppm 读取值可能返回 `VALUE_NOT_READY`（`-8888.0`）。当前硬件不支持某种气体时，库会返回 `NO_SUPPORT_GAS`（`-9999.0`）。

`get_hardware_model()` 返回的硬件型号含义如下：

| 返回值 | 常量 | 模块 | 支持气体 |
| --- | --- | --- | --- |
| `0x21` | `HW_GMA2021B` | GMA2021B | CO、NO2 |
| `0x31` | `HW_GMA3021B` | GMA3021B | CO、NO2、NH3 |
| `0x00` | - | 无效或读取失败 | - |

```python

def __init__(self, bus=1, i2c_addr=ADDRESS_3):
  '''!
    @brief 模块初始化。
    @param bus int Raspberry Pi I2C 总线编号。
    @param i2c_addr int 传感器 I2C 地址。
    @n i2c_addr 为 ADDRESS_0 ~ ADDRESS_3。默认 ADDRESS_3（0x78）。bus 默认 1。
  '''

def begin(self):
  '''!
    @brief 检查 GMA 传感器是否连接。
    @n 在其它方法之前调用一次。调用前模块必须已上电，且 ADD_A1 / ADD_A0
    @n 拨码与 i2c_addr 一致。
    @n 设备 ID 不是 0xA5，或硬件型号不是 HW_GMA2021B / HW_GMA3021B 时返回 False。
    @return bool 初始化结果。
    @retval True 检测到设备。
    @retval False I2C 通信失败或设备 ID 不匹配。
  '''

def get_hardware_model(self):
  '''!
    @brief 读取板载 MCU 保存的硬件型号。
    @return int 硬件型号寄存器值。
    @retval HW_GMA2021B 0x21，GMA2021B 双气体模块，支持 CO 和 NO2。
    @retval HW_GMA3021B 0x31，GMA3021B 三气体模块，支持 CO、NO2 和 NH3。
    @retval 0 读取失败或硬件型号无效。
  '''

def warm_up_time(self, minute=3):
  '''!
    @brief 配置并轮询板载预热与稳定性检查。
    @param minute int 首次预热时长，单位分钟。
    @n 小于 1 时按 1 分钟处理。默认 3 分钟。
    @n 固件上电自动开始 3 分钟预热。主控复位而传感器保持供电时，如果传感器仍报告 COMPLETE，首次调用 warm_up_time() 会重新启动预热；仍在进行的固件预热则继续原倒计时。
    @n 已有 R0 时保留原值，没有 R0 时自动保存最后稳定窗口。
    @n 没有 R0 时，必须仅在确认的洁净空气中执行本流程。稳定性不能证明空气洁净。
    @return bool 预热状态。
    @retval True 预热及稳定性检查完成；原来没有 R0 时，最后稳定窗口已用于生成 R0。
    @retval False 正在预热，或 I2C 命令失败。
  '''

def set_warm_up_stable_threshold_mv(self, value):
  '''!
    @brief 设置预热稳定性检查使用的电压最大偏差阈值。
    @param value int 阈值，单位 mV。
    @n 有效范围 1～50。该设置为易失，固件复位后恢复默认 50 mV。
    @n 若预热已经完成，下一次 warm_up_time() 会重新启动预热以验证新阈值；正在进行的预热不会重新计时，新阈值用于下一次检查。
    @return bool 设置结果。
    @retval True 固件已接受且回读值一致。
    @retval False 参数越界、命令失败或回读不一致。
  '''

def get_warm_up_stable_threshold_mv(self):
  '''!
    @brief 读取预热稳定性检查使用的电压最大偏差阈值。
    @n 返回阈值（单位 mV）；读取失败返回 0。
    @return int 阈值，单位 mV。
  '''

def clear_baseline(self):
  '''!
    @brief 清除已保存的 R0，使模块进入未校准状态。
    @n 本方法只清除 R0。清除后应在确认的洁净空气中调用 warm_up_time()；由于此时没有基线，其最后稳定窗口会自动保存为新 R0。
    @return bool 清除结果。
    @retval True Flash 记录和当前 R0 已清除。
    @retval False I2C 命令或 Flash 擦除失败。
  '''

def has_valid_baseline(self):
  '''!
    @brief 检查是否存在有效的洁净空气 R0。
    @n 每次调用都会读一次状态寄存器，占用一次 I2C 传输。
    @n 寄存器读取失败按 False 处理；此时应视为未校准，直到后续调用返回 True 再读 get_baseline_voltage_mv()。
    @return bool 基线状态。
    @retval True 存在可用于浓度换算的有效 R0。
    @retval False 模块尚未校准。
  '''

def get_gas_data(self, gas_type):
  '''!
    @brief 读取气体 ppm 浓度。
    @param gas_type int GAS_CO、GAS_NO2 或 GAS_NH3。
    @n GMA2021B 不支持 GAS_NH3，返回 NO_SUPPORT_GAS。
    @return float 气体浓度，单位 ppm。
    @retval NO_SUPPORT_GAS 当前硬件不支持该气体。
    @retval VALUE_NOT_READY 数据未就绪，或模块未校准、预热中、休眠中。
  '''

def get_all_gas_data(self):
  '''!
    @brief 读取全部气体浓度寄存器。
    @return tuple (co, no2, nh3)，单位 ppm。
    @n GMA2021B 的 nh3 为 NO_SUPPORT_GAS。
    @n I2C 失败时三个值均为 VALUE_NOT_READY。
  '''

def sleep_mode(self):
  '''!
    @brief 停止板载 MCU 气体采集（休眠）。
    @n SEN0757/SEN0758 上加热与 LDO 仍保持供电，整机电流不会明显下降；休眠只暂停 MCU 采集，不是整机断电式低功耗。
    @n 该函数会短暂等待电源状态寄存器完成更新。
  '''

def wake_up_mode(self):
  '''!
    @brief 休眠后恢复板载 MCU 气体采集。
    @n 加热与 LDO 本就保持供电；本方法重新启动 MCU 侧采样。
    @n 该函数会短暂等待电源状态寄存器完成更新。
  '''

def get_power_state(self):
  '''!
    @brief 读取板载 MCU 当前是在采样还是已停止采集。
    @n 该函数读取的是模块对外电源状态寄存器的最新镜像值。
    @return int SLEEP_MODE（已停止采集）、WAKE_UP_MODE（正常采样），读取失败时返回 0xFF。
  '''

def soft_reset(self):
  '''!
    @brief 复位板载 MCU，不清除用户校准数据。
    @n 已保存的 R0 保留，但板载预热会从头开始；复位后需重新调用 warm_up_time() 并等到返回 True 再读 ppm。
    @n 命令发出后方法会等待 80 ms，让 MCU 完成重启。
    @return bool 命令写入结果。
    @retval True 命令写入成功。
    @retval False I2C 通信失败。
  '''

def get_status(self):
  '''!
    @brief 读取板载 MCU 状态位。
    @n 状态位依次为 STATUS_BASELINE_VALID、STATUS_WARMING、STATUS_DATA_VALID、
    @n STATUS_SLEEP、STATUS_ADC_ERROR、STATUS_FLASH_ERROR 和 STATUS_FORMULA_MISSING。
    @n 寄存器读取失败同样返回 0，与「没有任何状态位」无法区分；可用 get_power_state()（失败返回 0xFF）判断总线是否断开。
    @return int STATUS_xxx 位掩码；读取失败返回 0。
  '''

def get_warm_up_remain_seconds(self):
  '''!
    @brief 读取当前预热阶段的剩余秒数。
    @n 返回首次预热或重试阶段的剩余秒数。
    @n 稳定窗口检查失败时，固件会在下一轮 1 分钟重试前返回 0。完成和空闲状态也返回 0，应使用 warm_up_time() 区分。
    @n 寄存器读取失败也返回 0。
    @return int 剩余秒数；预热空闲、已完成或读取失败时为 0。
  '''

def get_command_status(self):
  '''!
    @brief 读取最近一次命令状态。
    @n 0 空闲，1 忙，2 完成，0x80 错误。
    @n 寄存器读取失败返回 0x80，与命令执行失败相同。
    @return int 命令状态；读取失败返回 0x80。
  '''

def get_adc_voltage_mv(self, gas_type):
  '''!
    @brief 读取当前 ADC 电压。
    @param gas_type int 气体通道。
    @n GAS_CO 读取 AIN0，GAS_NO2 读取 AIN1，GAS_NH3 在 GMA3021B 上读取 AIN2。
    @n 不支持的气体类型返回 0。
    @n 寄存器读取失败也返回 0。
    @n 原始 ADC 电压主要用于诊断。气体浓度由板载 MCU 根据 Rs/R0 比例计算。
    @return int ADC 电压，单位 mV；气体类型不支持或读取失败时为 0。
  '''

def get_baseline_voltage_mv(self, gas_type):
  '''!
    @brief 读取洁净空气基准电压。
    @param gas_type int 气体通道。
    @n GAS_CO 读取 AIN0 基准，GAS_NO2 读取 AIN1 基准，GAS_NH3 在 GMA3021B 上读取 AIN2 基准。
    @n 不支持的气体类型返回 0。
    @n 寄存器读取失败也返回 0。
    @n 仅应在 has_valid_baseline() 返回 True 后读取；原来没有 R0 时，成功预热通常会生成基线，但 Flash 失败仍可能导致基线无效。
    @return int 基准电压，单位 mV；气体类型不支持、无 R0 或读取失败时为 0。
  '''

```

## 兼容性

| 主板         | 通过 | 未通过 | 未测试 | 备注 |
| ------------ | :--: | :--: | :----: | ---- |
| Raspberry Pi |  √   |      |        |      |

* Python 版本

| Python  | 通过 | 未通过 | 未测试 | 备注 |
| ------- | :--: | :--: | :----: | ---- |
| Python2 |      |      |   √    |      |
| Python3 |  √   |      |        |      |

## 历史

- 2026/11/23 - V1.0.0 版本

## 创作者

Written by thdyyl(yuanlong.yu@dfrobot.com), 2026. (Welcome to our [website](https://www.dfrobot.com/))
