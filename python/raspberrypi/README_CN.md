# DFRobot_GMA（Python，Raspberry Pi）

* [English Version](./README.md)

DFRobot_GMA 是 DFRobot Gravity MEMS 气体传感器模组在 Raspberry Pi 上使用的 Python 库，用于通过 I2C 从板载 GMA 控制器读取 CO、NO2 和 NH3 气体浓度数据。

模块上的 MCU 负责 ADC 采样、洁净空气零点校准、基于 Rs/R0 比例的 ppm 换算和 Flash 数据保存。本 Python 库只与模块的 I2C 寄存器接口通信。

主控端不直接计算气体曲线。板载 MCU 会保存洁净空气基准值并更新 ppm 寄存器；Python 驱动读取模块状态位、原始 ADC 电压、基准电压和气体浓度值。

## 产品链接（[https://www.dfrobot.com.cn](https://www.dfrobot.com.cn)）

    SKU: SEN0757 / SEN0758

## 目录

* [概述](#概述)
* [安装](#安装)
* [I2C 地址](#i2c-地址)
* [方法](#方法)
* [示例](#示例)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)

## 概述

这是 DFRobot GMA 气体传感器模组在 Raspberry Pi 上使用的 Python 驱动。它通过 I2C 检测模块、启动并轮询洁净空气预热校准、读取 ppm 浓度数据，并控制休眠或唤醒模式。

* 支持 GMA2021B 模块，可测量 CO 和 NO2
* 支持 GMA3021B 模块，可测量 CO、NO2 和 NH3
* 通过 `0x75` 到 `0x78` 的 I2C 地址通信
* 支持读取单个气体 ppm 数据，或一次性读取所有气体 ppm 寄存器
* 支持读取硬件型号、状态位、预热剩余时间、命令状态、ADC 电压和基准电压
* 支持休眠、唤醒和软复位
* 使用 `NO_SUPPORT_GAS` 和 `VALUE_NOT_READY` 表示气体不支持或数据未就绪

## 安装

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

## I2C 地址

| ADD_A1 | ADD_A0 | 地址 |
| --- | --- | --- |
| 0 | 0 | `0x75` |
| 0 | 1 | `0x76` |
| 1 | 0 | `0x77` |
| 1 | 1 | `0x78` |

Python 构造函数和所有示例默认使用 `ADDRESS_3`（`0x78`），对应 `ADD_A1=1`、`ADD_A0=1`。如果拨码开关位置不同，需要在 `DFRobot_GMA()` 中传入对应的地址常量。

## 方法

气体浓度数据只有在洁净空气零点校准完成后才有效。未校准、预热中、休眠中或 I2C 读取失败时，ppm 读取值可能返回 `VALUE_NOT_READY`（`-8888.0`）。当前硬件不支持某种气体时，库会返回 `NO_SUPPORT_GAS`（`-9999.0`）。

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
    @param i2c_addr int 传感器 I2C 地址，ADDRESS_0 ~ ADDRESS_3。默认 ADDRESS_3。
  '''

def begin(self):
  '''!
    @brief 检查 GMA 传感器是否连接。
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
    @brief 启动并轮询洁净空气预热校准。
    @param minute int 预热时间，单位分钟，最小 1 分钟。
    @return bool 校准状态。
    @retval True 校准完成。
    @retval False 正在校准或命令写入失败。
  '''

def get_gas_data(self, gas_type):
  '''!
    @brief 读取气体 ppm 浓度。
    @param gas_type int GAS_CO、GAS_NO2 或 GAS_NH3。
    @return float 气体浓度，单位 ppm，或 NO_SUPPORT_GAS / VALUE_NOT_READY。
  '''

def get_all_gas_data(self):
  '''!
    @brief 读取全部气体浓度寄存器。
    @return tuple (co, no2, nh3)，单位 ppm。
  '''

def sleep_mode(self):
  '''!
    @brief 让板载 MCU 进入休眠模式。
    @note 该函数会短暂等待电源状态寄存器完成更新。
  '''

def wake_up_mode(self):
  '''!
    @brief 唤醒板载 MCU 并恢复气体采样。
    @note 该函数会短暂等待电源状态寄存器完成更新。
  '''

def get_power_state(self):
  '''!
    @brief 读取当前电源状态。
    @return int SLEEP_MODE、WAKE_UP_MODE，读取失败时返回 0xFF。
    @note 该函数读取的是模块对外电源状态寄存器的最新镜像值。
  '''

def soft_reset(self):
  '''!
    @brief 复位板载 MCU，不清除用户校准数据。
    @return bool 命令写入结果。
  '''

def get_status(self):
  '''!
    @brief 读取板载 MCU 状态位。
    @return int STATUS_xxx 位掩码。
  '''

def get_warm_up_remain_seconds(self):
  '''!
    @brief 读取预热剩余秒数。
    @return int 剩余秒数。
  '''

def get_command_status(self):
  '''!
    @brief 读取最近一次命令状态。
    @return int 0 空闲，1 忙，2 完成，0x80 错误。
  '''

def get_adc_voltage_mv(self, gas_type):
  '''!
    @brief 读取当前 ADC 电压。
    @param gas_type int GAS_NO2 读取 AIN0，GAS_CO 读取 AIN1。
    @return int ADC 电压，单位 mV。
    @note 原始 ADC 电压主要用于诊断。气体浓度由板载 MCU 根据 Rs/R0 比例计算。
  '''

def get_baseline_voltage_mv(self, gas_type):
  '''!
    @brief 读取洁净空气基准电压。
    @param gas_type int GAS_NO2 读取 AIN0 基准，GAS_CO 读取 AIN1 基准。
    @return int 基准电压，单位 mV。
    @note 建议在预热校准完成后读取。当前公开的基准电压寄存器为 AIN0 和 AIN1。
  '''
```

## 示例

所有示例都在代码注释中保留 ADD_A1/ADD_A0 拨码地址表，运行时会打印检测到的硬件型号及其含义，然后执行洁净空气零点校准。请将传感器保持在洁净空气中，等待预热校准完成后再进入示例主流程。校准完成后，示例会打印 CO 和 NO2 的洁净空气基准电压，单位 mV。

* `get_gas_data.py`：校准模块后，分别读取 CO、NO2 和 NH3 ppm 浓度。
* `get_all_gas_data.py`：校准模块后，通过一次 I2C 读取获取全部气体 ppm 寄存器。
* `sleep_wake.py`：校准模块后，进入休眠模式，检查电源状态，然后唤醒模块。

## 兼容性

| MCU          | 正常 | 异常 | 未测试 | 备注 |
| ------------ | :--: | :--: | :----: | ---- |
| Raspberry Pi |  √   |      |        |      |

* Python 版本

| Python  | 正常 | 异常 | 未测试 | 备注 |
| ------- | :--: | :--: | :----: | ---- |
| Python2 |      |      |   √    |      |
| Python3 |  √   |      |        |      |

## 历史

* 日期 2026-07-07
* 版本 V1.0.0

## 创作者

Written by thdyyl<yuanlong.yu@dfrobot.com>, 2026. (Welcome to our website)
