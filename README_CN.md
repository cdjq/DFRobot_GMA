# DFRobot_GMA

* [English Version](./README.md)

这款 Gravity MEMS 气体传感器模组（SKU：SEN0757 / SEN0758）在板载 MCU 上完成气体采样与浓度换算，主机通过 I2C 读取结果。模组内置 MEMS 气敏元件与控制器：控制器负责 ADC 采样、洁净空气零点（R0）校准、基于 Rs/R0 比例的 ppm 换算以及 Flash 数据保存；主控端不直接计算气体曲线，而是读取状态位、原始 ADC 电压、基准电压和气体浓度寄存器。硬件型号分为 GMA2021B（`0x21`，支持 CO、NO2）与 GMA3021B（`0x31`，支持 CO、NO2、NH3）。I2C 地址由拨码开关 ADD_A1 / ADD_A0 选择，范围为 `0x75`～`0x78`，默认 `GMA_ADDRESS_3`（`0x78`，ADD_A1=1、ADD_A0=1）。本库用于在 Arduino 等主控上驱动该模组：读取 CO、NO2、NH3 浓度，并完成预热与稳定性检查、基线清除、休眠/唤醒和软复位。示例 `getGasData`、`getAllGasData` 读取浓度，`sleepWake` 演示休眠与唤醒，`calibrateBaseline` 在确认的洁净空气中重新生成 R0。

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

本库用于读取 CO、NO2 和 NH3 浓度，并完成预热、基线清除、休眠与唤醒。先调用 `begin()`，再用 `warmUpTime()` 等待预热完成，然后读取气体数据。日常读数用示例 `getGasData` / `getAllGasData`（自带预热，一般不必先校准）；`calibrateBaseline` 仅在需要强制重做洁净空气 R0 时使用；`sleepWake` 演示板载 MCU 停止/恢复采集（对比休眠前后电源状态与 ppm；SEN0757/SEN0758 加热与 LDO 常开，整机功耗不会明显下降）。

## 库安装

这里有 2 种安装方法：

1. 在 Arduino 库管理器中搜索并安装 DFRobot_GMA。
2. 下载库文件，解压到 Arduino 的 `libraries` 目录，然后打开 `examples` 目录运行示例。

本库仅依赖 Arduino 核心与 `Wire`，无需其它第三方库。

树莓派 / Python 版本位于 [`python/raspberrypi`](./python/raspberrypi/README_CN.md)。

## 方法

固件上电后自动开始 3 分钟预热。到达检查点后，各启用通道取最近 30 个一秒样本，去掉两个最高值和两个最低值，要求其余样本与修剪均值的偏差不超过当前阈值（默认 50mV）。窗口不稳定时自动再预热 1 分钟并复检，直到稳定。`warmUpTime(3)` 负责配置并轮询这一完整过程；固件 V1.3 及以上可通过 `setWarmUpStableThresholdMv()` 临时调整阈值。

确认稳定后，有 R0 就保留并使用已有 R0 输出浓度；没有 R0 时，固件自动把最后 30 秒稳定窗口的修剪均值保存为洁净空气基线。因此没有 R0 的传感器必须在确认的洁净空气中预热。`clearBaseline()` 本身只清除旧 R0，随后调用 `warmUpTime()`，稳定后就会自动生成替代基线。稳定性检查只能判断电压是否稳定，不能判断环境中是否没有目标气体。

气体浓度数据只有在预热和基线准备完成后才有效。未校准、预热中、休眠中或 I2C 读取失败时，ppm 读取值可能返回 `GMA_VALUE_NOT_READY`（`-8888.0`）。当前硬件不支持某种气体时，库会返回 `GMA_NO_SUPPORT_GAS`（`-9999.0`）。例如 GMA2021B 不支持 NH3，调用 `gma.getGasData(gma.eGasNH3)` 会返回 `GMA_NO_SUPPORT_GAS`，调用 `getAllGasData(co, no2, nh3)` 时 `nh3` 会被赋值为 `GMA_NO_SUPPORT_GAS`。

气体类型定义为 `DFRobot_GMA` 类内的公开枚举。在 sketch 和示例中，建议使用 `gma.eGasCO`、`gma.eGasNO2` 或 `gma.eGasNH3`，便于 Arduino IDE 自动补全。

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
 * @param pWire I2C 总线对象。
 * @param i2cAddr 传感器 I2C 地址。
 * @n pWire 默认 &Wire。
 * @n i2cAddr 为 GMA_ADDRESS_0 ~ GMA_ADDRESS_3。默认 GMA_ADDRESS_3（0x78）。
 */
DFRobot_GMA(TwoWire *pWire = &Wire, uint8_t i2cAddr = GMA_ADDRESS_3);

/**
 * @fn begin
 * @brief 初始化 I2C 通信并读取设备 ID 和硬件型号。
 * @n 在 setup() 中调用一次，且位于其它方法之前；本函数会自行启动 I2C 总线，
 * @n sketch 不需要再调用 Wire.begin()。调用前模块必须已上电，且 ADD_A1 / ADD_A0
 * @n 拨码与 i2cAddr 一致。
 * @n 设备 ID 不是 0xA5，或硬件型号不是 GMA_HW_GMA2021B / GMA_HW_GMA3021B 时返回 false。
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
 * @brief 配置并轮询板载预热与稳定性检查。
 * @param minute 初始预热时间，单位分钟。
 * @n 小于 1 的值按 1 分钟处理。默认 GMA_WARMUP_DEFAULT_MINUTE（3）。
 * @n 固件上电自动开始 3 分钟预热。主控复位而传感器保持供电时，如果传感器仍报告 COMPLETE，首次调用 warmUpTime() 会重新启动预热；仍在进行的固件预热则继续原倒计时。
 * @n 已有 R0 时保留原值，没有 R0 时自动保存最后稳定窗口为新的洁净空气基线。
 * @n 没有 R0 时，必须仅在确认的洁净空气中执行本流程。固件无法判断周围空气是否洁净。
 * @return 预热状态。
 * @retval true 预热和稳定性检查已完成；原来没有 R0 时，最后稳定窗口已用于生成 R0。
 * @retval false 正在预热，或命令写入失败。
 */
bool warmUpTime(uint8_t minute = GMA_WARMUP_DEFAULT_MINUTE);

/**
 * @fn setWarmUpStableThresholdMv
 * @brief 设置预热稳定性检查使用的电压最大偏差阈值。
 * @param value 阈值，单位 mV。
 * @n 有效范围 1～50。该设置为易失，固件复位后恢复默认 50 mV。
 * @n 若预热已经完成，下一次 warmUpTime() 会重新启动预热以验证新阈值；正在进行的预热不会重新计时，新阈值用于下一次检查。
 * @return 设置结果。
 * @retval true 固件已接受且回读值一致。
 * @retval false 参数越界、命令失败或回读不一致。
 */
bool setWarmUpStableThresholdMv(uint8_t value);

/**
 * @fn getWarmUpStableThresholdMv
 * @brief 读取预热稳定性检查使用的电压最大偏差阈值。
 * @n 返回阈值（单位 mV）；读取失败返回 0。
 * @return 阈值，单位 mV。
 */
uint8_t getWarmUpStableThresholdMv(void);

/**
 * @fn clearBaseline
 * @brief 清除已保存的 R0，使模块进入未校准状态。
 * @n 本函数只清除 R0。清除后应在确认的洁净空气中调用 warmUpTime()；由于此时没有基线，其最后稳定窗口会自动保存为新 R0。
 * @return 清除结果。
 * @retval true Flash 记录和当前 R0 已清除。
 * @retval false I2C 命令或 Flash 擦除失败。
 */
bool clearBaseline(void);

/**
 * @fn hasValidBaseline
 * @brief 检查是否存在有效的洁净空气 R0。
 * @n 每次调用都会读一次状态寄存器，占用一次 I2C 传输。
 * @n 寄存器读取失败按 false 处理；此时应视为未校准，直到后续调用返回 true 再读 getBaselineVoltageMv()。
 * @return 基线状态。
 * @retval true 存在可用于浓度换算的有效 R0。
 * @retval false 模块尚未校准。
 */
bool hasValidBaseline(void);

/**
 * @fn getGasData
 * @brief 读取指定气体 ppm 浓度。
 * @param gasType 气体类型，eGasCO / eGasNO2 / eGasNH3。
 * @n GMA2021B 不支持 eGasNH3，返回 GMA_NO_SUPPORT_GAS（-9999.0f）。
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
 * @param nh3 NH3 浓度输出，单位 ppm。
 * @n GMA2021B 会将 nh3 赋值为 GMA_NO_SUPPORT_GAS（-9999.0f）。
 * @n I2C 失败时，所有输出都会被置为 GMA_VALUE_NOT_READY（-8888.0f）。
 * @return 读取结果。
 * @retval true I2C 读取成功。
 * @retval false I2C 通信失败，输出值会被置为 GMA_VALUE_NOT_READY（-8888.0f）。
 */
bool getAllGasData(float &co, float &no2, float &nh3);

/**
 * @fn sleepMode
 * @brief 停止板载 MCU 气体采集（休眠）。
 * @n SEN0757/SEN0758 上加热与 LDO 仍保持供电，整机电流不会明显下降；休眠只暂停 MCU 采集，不是整机断电式低功耗。
 * @n 该函数会短暂等待电源状态寄存器完成更新。
 */
void sleepMode(void);

/**
 * @fn wakeUpMode
 * @brief 休眠后恢复板载 MCU 气体采集。
 * @n 加热与 LDO 本就保持供电；本函数重新启动 MCU 侧采样。
 * @n 该函数会短暂等待电源状态寄存器完成更新。
 */
void wakeUpMode(void);

/**
 * @fn getPowerState
 * @brief 读取板载 MCU 当前是在采样还是已停止采集。
 * @n 该函数读取的是模块对外电源状态寄存器的最新镜像值。
 * @return 电源状态。
 * @retval GMA_SLEEP_MODE MCU 已停止采集。
 * @retval GMA_WAKE_UP_MODE MCU 正常采样。
 * @retval 0xFF I2C 通信失败。
 */
uint8_t getPowerState(void);

/**
 * @fn softReset
 * @brief 复位板载 MCU，不清除用户校准数据。
 * @n 已保存的 R0 保留，但板载预热会从头开始；复位后需重新调用 warmUpTime() 并等到返回 true 再读 ppm。
 * @n 命令发出后函数会等待 80 ms，让 MCU 完成重启。
 * @return 命令写入结果。
 * @retval true 命令写入成功。
 * @retval false I2C 通信失败。
 */
bool softReset(void);

/**
 * @fn getStatus
 * @brief 读取板载 MCU 状态位。
 * @n 状态位依次为 GMA_STATUS_BASELINE_VALID、GMA_STATUS_WARMING、GMA_STATUS_DATA_VALID、
 * @n GMA_STATUS_SLEEP、GMA_STATUS_ADC_ERROR、GMA_STATUS_FLASH_ERROR 和 GMA_STATUS_FORMULA_MISSING。
 * @n 寄存器读取失败同样返回 0，与「没有任何状态位」无法区分；可用 getPowerState()（失败返回 0xFF）判断总线是否断开。
 * @return GMA_STATUS_xxx 位掩码；读取失败返回 0。
 */
uint8_t getStatus(void);

/**
 * @fn getWarmUpRemainSeconds
 * @brief 读取当前预热阶段的剩余秒数。
 * @n 返回当前首次预热或重试阶段的剩余秒数。
 * @n 稳定窗口检查失败时，固件会在下一轮 1 分钟重试前对外保留一次 0。预热完成或空闲时也返回 0，应使用 warmUpTime() 判断整个流程是否完成。
 * @n 寄存器读取失败也返回 0。
 * @return 剩余秒数；预热空闲、已完成或读取失败时为 0。
 */
uint16_t getWarmUpRemainSeconds(void);

/**
 * @fn getCommandStatus
 * @brief 读取最近一次命令执行状态。
 * @n 0 空闲，1 忙，2 完成，0x80 错误。
 * @n 寄存器读取失败返回 0x80，与命令执行失败相同。
 * @return 命令状态；读取失败返回 0x80。
 */
uint8_t getCommandStatus(void);

/**
 * @fn getADCVoltageMv
 * @brief 读取指定气体通道的当前 ADC 电压。
 * @param gasType 气体通道。
 * @n eGasCO 读取 AIN0/ADC_CO，eGasNO2 读取 AIN1/ADC_NO2，eGasNH3 在 GMA3021B 上读取 AIN2/ADC_NH3。
 * @n 不支持的气体类型返回 0。
 * @n 寄存器读取失败也返回 0。
 * @n 原始 ADC 电压主要用于诊断。气体浓度由板载 MCU 根据 Rs/R0 比例计算。
 * @return 当前电压，单位 mV；气体类型不支持或读取失败时为 0。
 */
uint16_t getADCVoltageMv(eGasType_t gasType);

/**
 * @fn getBaselineVoltageMv
 * @brief 读取指定气体通道的洁净空气基准电压。
 * @param gasType 气体通道。
 * @n eGasCO 读取 AIN0 基准，eGasNO2 读取 AIN1 基准，eGasNH3 在 GMA3021B 上读取 AIN2 基准。
 * @n 不支持的气体类型返回 0。
 * @n 寄存器读取失败也返回 0。
 * @n 仅应在 hasValidBaseline() 返回 true 后读取；原来没有 R0 时，成功预热通常会生成基线，但 Flash 失败仍可能导致基线无效。
 * @return 基准电压，单位 mV；气体类型不支持、无 R0 或读取失败时为 0。
 */
uint16_t getBaselineVoltageMv(eGasType_t gasType);
```

`getStatus()` 返回的状态位：

| 位 | 宏 | 说明 |
| --- | --- | --- |
| `0x01` | `GMA_STATUS_BASELINE_VALID` | 洁净空气零点有效 |
| `0x02` | `GMA_STATUS_WARMING` | 正在预热/检查稳定性，此时不会生成 R0 |
| `0x04` | `GMA_STATUS_DATA_VALID` | 气体 ppm 数据有效 |
| `0x08` | `GMA_STATUS_SLEEP` | 板载 MCU 已停止采集（休眠；加热/LDO 仍供电） |
| `0x10` | `GMA_STATUS_ADC_ERROR` | ADC 采样失败 |
| `0x20` | `GMA_STATUS_FLASH_ERROR` | Flash 保存失败 |
| `0x40` | `GMA_STATUS_FORMULA_MISSING` | 固件中未配置气体换算公式 |

## 兼容性

| 主板               | 通过 | 未通过 | 未测试 | 备注 |
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

- 2026/11/23 - V1.0.0 版本

## 创作者

Written by thdyyl(yuanlong.yu@dfrobot.com), 2026. (Welcome to our [website](https://www.dfrobot.com/))
