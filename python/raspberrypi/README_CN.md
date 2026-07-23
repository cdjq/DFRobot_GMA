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

这是 DFRobot GMA 气体传感器模组在 Raspberry Pi 上使用的 Python 驱动。它通过 I2C 检测模块、轮询自动预热与稳定性检查、读取 ppm 浓度数据，并控制休眠或唤醒模式。

* 支持 GMA2021B 模块，可测量 CO 和 NO2
* 支持 GMA3021B 模块，可测量 CO、NO2 和 NH3
* 通过 `0x75` 到 `0x78` 的 I2C 地址通信
* 电压不稳定时自动重复 1 分钟预热，直到稳定
* 已有 R0 时保留原值；没有 R0 时自动保存最后稳定窗口
* 支持清除旧 R0，使下一次洁净空气预热生成替代基线
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
    @brief 配置并轮询板载预热与稳定性检查。
    @param minute int 首次预热时长，单位分钟；小于 1 时按 1 分钟处理。
    @return bool 预热状态。
    @retval True 预热及稳定性检查完成；原来没有 R0 时，最后稳定窗口已用于生成 R0。
    @retval False 正在预热，或 I2C 命令失败。
    @note 固件上电自动开始 3 分钟预热。主控复位而传感器保持供电时，如果传感器仍报告 COMPLETE，首次调用 warm_up_time() 会重新启动预热；仍在进行的固件预热则继续原倒计时。
    @note 已有 R0 时保留原值，没有 R0 时自动保存最后稳定窗口。
    @warning 没有 R0 时，必须仅在确认的洁净空气中执行本流程。
  '''

def set_warm_up_stable_threshold_mv(self, value):
  '''!
    @brief 设置预热稳定性检查使用的电压最大偏差阈值。
    @param value int 阈值，单位 mV，有效范围 1～50；固件复位后恢复默认 50mV。
    @return bool 设置结果。
    @retval True 固件已接受且回读值一致。
    @retval False 参数越界、命令失败或回读不一致。
    @note 若预热已经完成，下一次 warm_up_time() 会重新启动预热以验证新阈值；正在进行的预热不会重新计时，新阈值用于下一次检查。
  '''

def get_warm_up_stable_threshold_mv(self):
  '''!
    @brief 读取预热稳定性检查使用的电压最大偏差阈值。
    @return int 阈值，单位 mV；读取失败返回 0。
  '''

def clear_baseline(self):
  '''!
    @brief 清除已保存的 R0，使模块进入未校准状态。
    @return bool 清除结果。
    @retval True Flash 记录和当前 R0 已清除。
    @retval False I2C 命令或 Flash 擦除失败。
    @note 本方法只清除 R0。清除后应在确认的洁净空气中调用 warm_up_time()；由于此时没有基线，其最后稳定窗口会自动保存为新 R0。
  '''

def has_valid_baseline(self):
  '''!
    @brief 检查是否存在有效的洁净空气 R0。
    @return bool 基线状态。
    @retval True 存在可用于浓度换算的有效 R0。
    @retval False 模块尚未校准。
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
    @return tuple (co, no2, nh3)，单位 ppm；GMA2021B 的 nh3 为 NO_SUPPORT_GAS。
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
    @brief 读取当前预热阶段的剩余秒数。
    @return int 首次预热或重试阶段的剩余秒数。
    @note 稳定窗口检查失败时，固件会在下一轮 1 分钟重试前返回 0。完成和空闲状态也返回 0，应使用 warm_up_time() 区分。
  '''

def get_command_status(self):
  '''!
    @brief 读取最近一次命令状态。
    @return int 0 空闲，1 忙，2 完成，0x80 错误。
  '''

def get_adc_voltage_mv(self, gas_type):
  '''!
    @brief 读取当前 ADC 电压。
    @param gas_type int GAS_CO 读取 AIN0，GAS_NO2 读取 AIN1，GAS_NH3 在 GMA3021B 上读取 AIN2。
    @return int ADC 电压，单位 mV。
    @note 原始 ADC 电压主要用于诊断。气体浓度由板载 MCU 根据 Rs/R0 比例计算。
  '''

def get_baseline_voltage_mv(self, gas_type):
  '''!
    @brief 读取洁净空气基准电压。
    @param gas_type int GAS_CO 读取 AIN0 基准，GAS_NO2 读取 AIN1 基准，GAS_NH3 在 GMA3021B 上读取 AIN2 基准。
    @return int 基准电压，单位 mV。
    @note 仅应在 has_valid_baseline() 返回 True 后读取；原来没有 R0 时，成功预热通常会生成基线，但 Flash 失败仍可能导致基线无效。
  '''

```

## 示例

所有示例都会在预热前设置并回读 10mV 稳定阈值。调试时可以修改 `WARM_UP_STABLE_THRESHOLD_MV`，有效范围为 1～50mV。普通测量示例会接管自动预热、打印原始通道电压并报告重试；识别新一轮 1 分钟重试时会忽略偶发的剩余时间 0 秒缺读。没有 R0 时，最后稳定窗口会自动保存为第一份洁净空气基线。

Python 驱动会对失败或长度不完整的 I2C 传输进行最多三次短间隔重试。主控复位但传感器保持供电、仍暴露旧 `COMPLETE` 状态时，驱动会请求新一轮预热，并且不会把该旧快照当成本轮结果。

* `get_gas_data.py`：等待数据稳定后，分别读取 CO、NO2 和 NH3 ppm 浓度。
* `get_all_gas_data.py`：等待数据稳定后，通过一次 I2C 读取获取全部气体 ppm 寄存器。
* `sleep_wake.py`：等待数据稳定后，进入休眠模式，检查电源状态，然后唤醒模块。
* `calibrate_baseline.py`：仅在确认洁净空气后运行；先清除已有 R0，再调用 `warm_up_time()`，固件会自动把最后稳定窗口保存为替代 R0。

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

* 日期 2026-07-23
* 版本 V1.3.0

## 创作者

Written by thdyyl<yuanlong.yu@dfrobot.com>, 2026. (Welcome to our website)
