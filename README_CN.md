# DFRobot_GMA

* [English Version](./README.md)

DFRobot_GMA 是 DFRobot Gravity MEMS 气体传感器模组的 Arduino 库，用于通过 I2C 从板载 GMA 控制器读取 CO、NO2 和 NH3 气体浓度数据。

模块上的 MCU 负责 ADC 采样、洁净空气零点校准、基于 Rs/R0 比例的 ppm 换算和 Flash 数据保存。本库通过简单的 I2C 寄存器接口与模块固件通信。

本库不在主控端直接计算气体曲线。板载 MCU 会保存洁净空气基准值并更新 ppm 寄存器；主控端读取模块状态位、原始 ADC 电压、基准电压和气体浓度值。

## 产品链接（[https://www.dfrobot.com.cn](https://www.dfrobot.com.cn)）

    SKU: SEN0757 / SEN0758

## 目录

* [概述](#概述)
* [库安装](#库安装)
* [I2C 地址](#i2c-地址)
* [方法](#方法)
* [示例](#示例)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)

## 概述

* 支持 GMA2021B 模块，可测量 CO 和 NO2
* 支持 GMA3021B 模块，可测量 CO、NO2 和 NH3
* 通过 `0x75` 到 `0x78` 的 I2C 地址与板载控制器通信
* 支持启动并轮询洁净空气预热零点校准
* 支持读取单个气体 ppm 数据，或一次性读取所有气体 ppm 寄存器
* 支持读取硬件型号、状态位、预热剩余时间、命令状态、ADC 电压和基准电压
* 支持休眠、唤醒和软复位
* 使用 `GMA_NO_SUPPORT_GAS` 和 `GMA_VALUE_NOT_READY` 表示气体不支持或数据未就绪

## 库安装

使用此库前，可下载 zip 文件并解压到 Arduino sketchbook 目录下的 `libraries` 文件夹，或在 Arduino Library Manager 可用时直接安装。

1. 打开 Arduino IDE
2. 在库管理器中搜索 `DFRobot_GMA`
3. 点击安装

或手动安装：

```bash
git clone https://github.com/DFRobot/DFRobot_GMA.git
```

该 Arduino 库仅依赖标准 `Wire` 库。

Raspberry Pi 使用方法请参考 `python/raspberrypi/README_CN.md`。

## I2C 地址

| ADD_A1 | ADD_A0 | 地址 |
| --- | --- | --- |
| 0 | 0 | `0x75` |
| 0 | 1 | `0x76` |
| 1 | 0 | `0x77` |
| 1 | 1 | `0x78` |

库构造函数和所有示例默认使用 `GMA_ADDRESS_3`（`0x78`），对应 `ADD_A1=1`、`ADD_A0=1`。如果拨码开关位置不同，需要在构造函数中传入对应的地址常量。

## 方法

气体浓度数据只有在洁净空气零点校准完成后才有效。未校准、预热中、休眠中或 I2C 读取失败时，ppm 读取值可能返回 `GMA_VALUE_NOT_READY`（`-8888.0`）。当前硬件不支持某种气体时，库会返回 `GMA_NO_SUPPORT_GAS`（`-9999.0`）。例如 GMA2021B 不支持 NH3，调用 `getGasData(eGasNH3)` 会返回 `GMA_NO_SUPPORT_GAS`，调用 `getAllGasData(co, no2, nh3)` 时 `nh3` 会被赋值为 `GMA_NO_SUPPORT_GAS`。

气体类型定义为 `DFRobot_GMA` 类内的公开枚举，调用时使用 `DFRobot_GMA::eGasCO`、`DFRobot_GMA::eGasNO2` 或 `DFRobot_GMA::eGasNH3`。

`getHardwareModel()` 返回的硬件型号含义如下：

| 返回值 | 宏 | 模块 | 支持气体 |
| --- | --- | --- | --- |
| `0x21` | `GMA_HW_GMA2021B` | GMA2021B | CO、NO2 |
| `0x31` | `GMA_HW_GMA3021B` | GMA3021B | CO、NO2、NH3 |
| `0x00` | - | 无效或读取失败 | - |

```C++

typedef enum {
  eGasCO = 0x01,
  eGasNO2 = 0x02,
  eGasNH3 = 0x03
} eGasType_t;

/**
 * @fn DFRobot_GMA
 * @brief 构造函数，绑定 I2C 总线和传感器 I2C 地址。
 * @param pWire I2C 总线对象，默认 &Wire。
 * @param i2cAddr 传感器 I2C 地址，GMA_ADDRESS_0 ~ GMA_ADDRESS_3。默认 GMA_ADDRESS_3。
 */
DFRobot_GMA(TwoWire *pWire = &Wire, uint8_t i2cAddr = GMA_ADDRESS_3);

/**
 * @fn begin
 * @brief 初始化 I2C 通信并读取设备 ID 和硬件型号。
 * @return 初始化结果。
 * @retval true 检测到 GMA 设备。
 * @retval false 未检测到设备或 I2C 通信失败。
 */
bool begin(void);

/**
 * @fn getHardwareModel
 * @brief 读取板载 MCU 保存的硬件型号。
 * @return 硬件型号寄存器值。
 * @retval GMA_HW_GMA2021B 0x21，GMA2021B 双气体模块，支持 CO 和 NO2。
 * @retval GMA_HW_GMA3021B 0x31，GMA3021B 三气体模块，支持 CO、NO2 和 NH3。
 * @retval 0 读取失败或硬件型号无效。
 */
uint8_t getHardwareModel(void);

/**
 * @fn warmUpTime
 * @brief 启动并轮询洁净空气零点预热校准。
 * @param minute 预热校准时间，单位分钟；小于 1 的值按 1 分钟处理。
 * @return 校准状态。
 * @retval true 预热校准完成。
 * @retval false 正在预热，或命令写入失败。
 * @note 该函数不会阻塞整个预热时间，需要在 loop() 或 setup() 中反复调用。
 */
bool warmUpTime(uint8_t minute = GMA_WARMUP_DEFAULT_MINUTE);

/**
 * @fn getGasData
 * @brief 读取指定气体 ppm 浓度。
 * @param gasType 气体类型，eGasCO / eGasNO2 / eGasNH3。
 * @return 气体浓度，单位 ppm。
 * @retval GMA_NO_SUPPORT_GAS 当前硬件不支持该气体。例如 GMA2021B 读取 eGasNH3 时返回该值（-9999.0f）。
 * @retval GMA_VALUE_NOT_READY 数据未就绪，或模块未校准、预热中、休眠中。
 */
float getGasData(eGasType_t gasType);

/**
 * @fn getAllGasData
 * @brief 一次性读取全部气体浓度寄存器。
 * @param co CO 浓度输出，单位 ppm。
 * @param no2 NO2 浓度输出，单位 ppm。
 * @param nh3 NH3 浓度输出。GMA2021B 会将该输出赋值为 GMA_NO_SUPPORT_GAS（-9999.0f）。
 * @return 读取结果。
 * @retval true I2C 读取成功。
 * @retval false I2C 通信失败，输出值会被置为 GMA_VALUE_NOT_READY（-8888.0f）。
 */
bool getAllGasData(float &co, float &no2, float &nh3);

/**
 * @fn getAllGasData
 * @brief 一次性读取 CO 和 NO2 浓度寄存器。
 * @param co CO 浓度输出，单位 ppm。
 * @param no2 NO2 浓度输出，单位 ppm。
 * @return 读取结果。
 * @retval true I2C 读取成功。
 * @retval false I2C 通信失败，输出值会被置为 GMA_VALUE_NOT_READY（-8888.0f）。
 * @note 不需要 NH3，或使用 GMA2021B 时，可调用该重载，不传入 nh3 参数。
 */
bool getAllGasData(float &co, float &no2);

/**
 * @fn sleepMode
 * @brief 让板载 MCU 进入低功耗休眠模式。
 * @note 该函数会短暂等待电源状态寄存器完成更新。
 */
void sleepMode(void);

/**
 * @fn wakeUpMode
 * @brief 唤醒板载 MCU 并恢复气体采样。
 * @note 该函数会短暂等待电源状态寄存器完成更新。
 */
void wakeUpMode(void);

/**
 * @fn getPowerState
 * @brief 读取板载 MCU 当前电源状态。
 * @return 电源状态。
 * @retval GMA_SLEEP_MODE 休眠模式。
 * @retval GMA_WAKE_UP_MODE 正常采样模式。
 * @retval 0xFF I2C 通信失败。
 * @note 该函数读取的是模块对外电源状态寄存器的最新镜像值。
 */
uint8_t getPowerState(void);

/**
 * @fn softReset
 * @brief 复位板载 MCU，不清除用户校准数据。
 * @return 命令写入结果。
 * @retval true 命令写入成功。
 * @retval false I2C 通信失败。
 */
bool softReset(void);

/**
 * @fn getStatus
 * @brief 读取板载 MCU 状态位。
 * @return GMA_STATUS_xxx 位掩码。
 */
uint8_t getStatus(void);

/**
 * @fn getWarmUpRemainSeconds
 * @brief 读取预热剩余时间。
 * @return 剩余秒数。未预热时通常为 0。
 */
uint16_t getWarmUpRemainSeconds(void);

/**
 * @fn getCommandStatus
 * @brief 读取最近一次命令执行状态。
 * @return 命令状态：0 空闲，1 忙，2 完成，0x80 错误。
 */
uint8_t getCommandStatus(void);

/**
 * @fn getADCVoltageMv
 * @brief 读取指定气体通道的当前 ADC 电压。
 * @param gasType eGasNO2 读取 AIN0/ADC_NO2，eGasCO 读取 AIN1/ADC_CO。
 * @return 当前电压，单位 mV。不支持的气体类型返回 0。
 * @note 原始 ADC 电压主要用于诊断。气体浓度由板载 MCU 根据 Rs/R0 比例计算。
 */
uint16_t getADCVoltageMv(eGasType_t gasType);

/**
 * @fn getBaselineVoltageMv
 * @brief 读取指定气体通道的洁净空气基准电压。
 * @param gasType eGasNO2 读取 AIN0 基准，eGasCO 读取 AIN1 基准。
 * @return 基准电压，单位 mV。不支持的气体类型返回 0。
 * @note 建议在预热校准完成后读取。当前公开的基准电压寄存器为 AIN0 和 AIN1。
 */
uint16_t getBaselineVoltageMv(eGasType_t gasType);
```

`getStatus()` 返回的状态位：

| 位 | 宏 | 说明 |
| --- | --- | --- |
| `0x01` | `GMA_STATUS_BASELINE_VALID` | 洁净空气零点有效 |
| `0x02` | `GMA_STATUS_WARMING` | 正在预热校准 |
| `0x04` | `GMA_STATUS_DATA_VALID` | 气体 ppm 数据有效 |
| `0x08` | `GMA_STATUS_SLEEP` | 板载 MCU 正在休眠 |
| `0x10` | `GMA_STATUS_ADC_ERROR` | ADC 采样失败 |
| `0x20` | `GMA_STATUS_FLASH_ERROR` | Flash 保存失败 |
| `0x40` | `GMA_STATUS_FORMULA_MISSING` | 固件中未配置气体换算公式 |

## 示例

所有示例都在代码注释中保留 ADD_A1/ADD_A0 拨码地址表，运行时会打印检测到的硬件型号及其含义，然后执行洁净空气零点校准。请将传感器保持在洁净空气中，等待预热校准完成后再进入示例主流程。校准完成后，示例会打印 CO 和 NO2 的洁净空气基准电压，单位 mV。

* `getGasData`：校准模块后，分别读取 CO、NO2 和 NH3 ppm 浓度。
* `getAllGasData`：校准模块后，通过一次 I2C 读取获取气体 ppm 寄存器。不需要 NH3 或当前硬件不支持 NH3 时，可使用 `getAllGasData(co, no2)`。
* `sleepWake`：校准模块后，进入休眠模式，检查电源状态，然后唤醒模块。

## 兼容性

| MCU                | 正常 | 异常 | 未测试 | 备注 |
| ------------------ |:----:|:----:|:------:| ---- |
| Arduino uno        |  √   |      |        |      |
| Mega2560           |  √   |      |        |      |
| Leonardo           |  √   |      |        |      |
| ESP32              |  √   |      |        |      |
| ESP8266            |  √   |      |        |      |
| FireBeetle M0      |  √   |      |        |      |
| micro:bit          |  √   |      |        |      |
| Raspberry Pi 4B    |  √   |      |        | Python 驱动 |

## 历史

* 日期 2026-07-07
* 版本 V1.0.0

## 创作者

Written by thdyyl<yuanlong.yu@dfrobot.com>, 2026. (Welcome to our website)
