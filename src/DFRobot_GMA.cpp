/*!
 * @file DFRobot_GMA.cpp
 * @brief I2C driver for DFRobot GMA gas sensor modules.
 * @details Register access, warm-up polling and ppm reads for the onboard GMA controller.
 * @copyright   Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @License     The MIT License (MIT)
 * @author [thdyyl](yuanlong.yu@dfrobot.com)
 * @version  V1.0.0
 * @date  2026-11-23
 * @url https://github.com/DFRobot/DFRobot_GMA
 */

#include "DFRobot_GMA.h"

#define GMA_REG_DEVICE_ID                  ((uint8_t)0x00)
#define GMA_REG_HW_MODEL                   ((uint8_t)0x01)
#define GMA_REG_POWER_STATE                ((uint8_t)0x04)
#define GMA_REG_STATUS                     ((uint8_t)0x05)
#define GMA_REG_WARMUP_MINUTE              ((uint8_t)0x06)
#define GMA_REG_WARMUP_REMAIN_L            ((uint8_t)0x07)
#define GMA_REG_COMMAND                    ((uint8_t)0x09)
#define GMA_REG_COMMAND_PARAM              ((uint8_t)0x0A)
#define GMA_REG_COMMAND_STATUS             ((uint8_t)0x0B)
#define GMA_REG_CO_PPM                     ((uint8_t)0x10)
#define GMA_REG_NO2_PPM                    ((uint8_t)0x14)
#define GMA_REG_NH3_PPM                    ((uint8_t)0x18)
#define GMA_REG_AIN0_MV_L                  ((uint8_t)0x1C)
#define GMA_REG_AIN1_MV_L                  ((uint8_t)0x1E)
#define GMA_REG_BASE_AIN0_MV_L             ((uint8_t)0x20)
#define GMA_REG_BASE_AIN1_MV_L             ((uint8_t)0x22)
#define GMA_REG_AIN2_MV_L                  ((uint8_t)0x28)
#define GMA_REG_BASE_AIN2_MV_L             ((uint8_t)0x2A)
#define GMA_REG_WARMUP_STATE               ((uint8_t)0x2C)
#define GMA_REG_WARMUP_STABLE_THRESHOLD_MV ((uint8_t)0x2D)

#define GMA_DEVICE_ID ((uint8_t)0xA5)

#define GMA_CMD_START_WARMUP                ((uint8_t)0x01)
#define GMA_CMD_SLEEP                       ((uint8_t)0x02)
#define GMA_CMD_WAKEUP                      ((uint8_t)0x03)
#define GMA_CMD_SOFT_RESET                  ((uint8_t)0x04)
#define GMA_CMD_CLEAR_BASELINE              ((uint8_t)0x05)
#define GMA_CMD_SET_WARMUP_STABLE_THRESHOLD ((uint8_t)0x06)

#define GMA_CMD_STATUS_DONE  ((uint8_t)0x02)
#define GMA_CMD_STATUS_ERROR ((uint8_t)0x80)

#define GMA_WARMUP_STATE_IDLE     ((uint8_t)0x00)
#define GMA_WARMUP_STATE_RUNNING  ((uint8_t)0x01)
#define GMA_WARMUP_STATE_COMPLETE ((uint8_t)0x02)

#define GMA_WARMUP_MIN_MINUTE              ((uint8_t)1)
#define GMA_WARMUP_STABLE_THRESHOLD_MIN_MV ((uint8_t)1)
#define GMA_WARMUP_STABLE_THRESHOLD_MAX_MV ((uint8_t)50)
#define GMA_POWER_STATE_TIMEOUT_MS         ((uint16_t)300)
#define GMA_POWER_STATE_POLL_MS            ((uint8_t)20)
#define GMA_COMMAND_TIMEOUT_MS             ((uint16_t)1000)

#ifdef ENABLE_DBG
#define DBG(...)                 \
  do {                           \
    Serial.print(F("["));        \
    Serial.print(__func__);      \
    Serial.print(F("():"));      \
    Serial.print(__LINE__);      \
    Serial.print(F("] "));       \
    Serial.println(__VA_ARGS__); \
  } while (0)
#define DBGLN(...) DBG(__VA_ARGS__)
#else
#define DBG(...) \
  do {           \
  } while (0)
#define DBGLN(...) \
  do {             \
  } while (0)
#endif

static bool waitPowerState(DFRobot_GMA *pGma, uint8_t expectedState, uint16_t timeoutMs);

DFRobot_GMA::DFRobot_GMA(TwoWire *pWire, uint8_t i2cAddr) : _pWire(pWire), _i2cAddr(i2cAddr), _hwModel(0), _warmUpMinute(0), _warmUpActive(false), _warmUpDone(false), _warmUpRestartPending(false) {}

bool DFRobot_GMA::begin(void)
{
  uint8_t deviceId = 0;

  if (_pWire == NULL) {
    return false;
  }

  _pWire->begin();
  delay(10);

  if (readReg(GMA_REG_DEVICE_ID, &deviceId, 1) == false) {
    return false;
  }

  if (deviceId != GMA_DEVICE_ID) {
    return false;
  }

  _hwModel = getHardwareModel();
  if ((_hwModel != GMA_HW_GMA2021B) && (_hwModel != GMA_HW_GMA3021B)) {
    return false;
  }

  _warmUpMinute         = 0;
  _warmUpActive         = false;
  _warmUpDone           = false;
  _warmUpRestartPending = (getWarmUpState() != GMA_WARMUP_STATE_RUNNING);
  return true;
}

uint8_t DFRobot_GMA::getHardwareModel(void)
{
  _hwModel = readUInt8(GMA_REG_HW_MODEL, 0);
  return _hwModel;
}

bool DFRobot_GMA::warmUpTime(uint8_t minute)
{
  uint8_t commandStatus  = getCommandStatus();
  uint8_t status         = getStatus();
  uint8_t firmwareMinute = getWarmUpMinute();
  uint8_t warmUpState    = getWarmUpState();

  if (minute < GMA_WARMUP_MIN_MINUTE) {
    minute = GMA_WARMUP_MIN_MINUTE;
  }

  if (_warmUpRestartPending == true) {
    if (writeCommand(GMA_CMD_START_WARMUP, minute) == false) {
      return false;
    }
    _warmUpRestartPending = false;
    _warmUpMinute         = minute;
    _warmUpActive         = true;
    _warmUpDone           = false;
    /*
     * The firmware executes commands asynchronously. Do not accept the old
     * COMPLETE register snapshot in the same call that starts a new warm-up.
     */
    return false;
  }

  if ((_warmUpDone == true) && (_warmUpActive == false) && (_warmUpMinute == minute) && ((status & GMA_STATUS_WARMING) == 0) && (warmUpState == GMA_WARMUP_STATE_COMPLETE)) {
    return true;
  }

  if (((status & GMA_STATUS_WARMING) != 0) && (warmUpState == GMA_WARMUP_STATE_RUNNING) && (firmwareMinute == minute)) {
    _warmUpMinute = minute;
    _warmUpActive = true;
    _warmUpDone   = false;
  } else if ((_warmUpActive == false) || (_warmUpMinute != minute)) {
    if (writeCommand(GMA_CMD_START_WARMUP, minute) == false) {
      return false;
    }
    _warmUpMinute = minute;
    _warmUpActive = true;
    _warmUpDone   = false;
    /* Wait for a later poll to observe RUNNING before accepting COMPLETE. */
    return false;
  }

  if ((_warmUpActive == true) && (commandStatus == GMA_CMD_STATUS_DONE) && ((status & GMA_STATUS_WARMING) == 0) && (warmUpState == GMA_WARMUP_STATE_COMPLETE)) {
    _warmUpActive = false;
    _warmUpDone   = true;
    return true;
  }

  if ((commandStatus == GMA_CMD_STATUS_ERROR) && (((status & GMA_STATUS_WARMING) == 0) || (warmUpState != GMA_WARMUP_STATE_RUNNING))) {
    _warmUpActive = false;
    _warmUpDone   = false;
  }

  return false;
}

bool DFRobot_GMA::clearBaseline(void)
{
  if (writeCommand(GMA_CMD_CLEAR_BASELINE, 0) == false) {
    return false;
  }

  if (waitCommandDone(GMA_COMMAND_TIMEOUT_MS) == false) {
    return false;
  }

  _warmUpMinute = 0;
  _warmUpActive = false;
  _warmUpDone   = false;
  return (hasValidBaseline() == false);
}

bool DFRobot_GMA::setWarmUpStableThresholdMv(uint8_t value)
{
  if ((value < GMA_WARMUP_STABLE_THRESHOLD_MIN_MV) || (value > GMA_WARMUP_STABLE_THRESHOLD_MAX_MV)) {
    return false;
  }

  if (writeCommand(GMA_CMD_SET_WARMUP_STABLE_THRESHOLD, value) == false) {
    return false;
  }

  if (waitCommandDone(GMA_COMMAND_TIMEOUT_MS) == false) {
    return false;
  }

  if (getWarmUpStableThresholdMv() != value) {
    return false;
  }

  _warmUpRestartPending = (getWarmUpState() != GMA_WARMUP_STATE_RUNNING);
  if (_warmUpRestartPending == true) {
    _warmUpMinute = 0;
    _warmUpActive = false;
    _warmUpDone   = false;
  }
  return true;
}

uint8_t DFRobot_GMA::getWarmUpStableThresholdMv(void)
{
  return readUInt8(GMA_REG_WARMUP_STABLE_THRESHOLD_MV, 0);
}

bool DFRobot_GMA::hasValidBaseline(void)
{
  return ((getStatus() & GMA_STATUS_BASELINE_VALID) != 0);
}

float DFRobot_GMA::getGasData(DFRobot_GMA::eGasType_t gasType)
{
  switch (gasType) {
    case eGasCO:
      return readFloat(GMA_REG_CO_PPM, GMA_VALUE_NOT_READY);

    case eGasNO2:
      return readFloat(GMA_REG_NO2_PPM, GMA_VALUE_NOT_READY);

    case eGasNH3:
      if ((_hwModel == 0) || (_hwModel == GMA_HW_GMA2021B)) {
        getHardwareModel();
      }
      if (_hwModel == GMA_HW_GMA2021B) {
        return GMA_NO_SUPPORT_GAS;
      }
      return readFloat(GMA_REG_NH3_PPM, GMA_VALUE_NOT_READY);

    default:
      return GMA_NO_SUPPORT_GAS;
  }
}

bool DFRobot_GMA::getAllGasData(float &co, float &no2, float &nh3)
{
  uint8_t buf[12];

  if (readReg(GMA_REG_CO_PPM, buf, sizeof(buf)) == false) {
    co  = GMA_VALUE_NOT_READY;
    no2 = GMA_VALUE_NOT_READY;
    nh3 = GMA_VALUE_NOT_READY;
    return false;
  }

  co  = bytesToFloat(&buf[0]);
  no2 = bytesToFloat(&buf[4]);
  nh3 = bytesToFloat(&buf[8]);

  if (_hwModel == 0) {
    getHardwareModel();
  }
  if (_hwModel == GMA_HW_GMA2021B) {
    nh3 = GMA_NO_SUPPORT_GAS;
  }

  return true;
}

void DFRobot_GMA::sleepMode(void)
{
  if (writeCommand(GMA_CMD_SLEEP, 0) == true) {
    _warmUpActive = false;
    _warmUpDone   = false;
    (void)waitPowerState(this, GMA_SLEEP_MODE, GMA_POWER_STATE_TIMEOUT_MS);
  }
}

void DFRobot_GMA::wakeUpMode(void)
{
  if (writeCommand(GMA_CMD_WAKEUP, 0) == true) {
    _warmUpActive = false;
    _warmUpDone   = false;
    (void)waitPowerState(this, GMA_WAKE_UP_MODE, GMA_POWER_STATE_TIMEOUT_MS);
  }
}

uint8_t DFRobot_GMA::getPowerState(void)
{
  return readUInt8(GMA_REG_POWER_STATE, 0xFF);
}

bool DFRobot_GMA::softReset(void)
{
  bool ret = writeCommand(GMA_CMD_SOFT_RESET, 0);

  if (ret == true) {
    _warmUpActive = false;
    _warmUpDone   = false;
    delay(80);
  }

  return ret;
}

uint8_t DFRobot_GMA::getStatus(void)
{
  return readUInt8(GMA_REG_STATUS, 0);
}

uint16_t DFRobot_GMA::getWarmUpRemainSeconds(void)
{
  return readUInt16(GMA_REG_WARMUP_REMAIN_L, 0);
}

uint8_t DFRobot_GMA::getWarmUpMinute(void)
{
  return readUInt8(GMA_REG_WARMUP_MINUTE, 0);
}

uint8_t DFRobot_GMA::getWarmUpState(void)
{
  return readUInt8(GMA_REG_WARMUP_STATE, GMA_WARMUP_STATE_IDLE);
}

uint8_t DFRobot_GMA::getCommandStatus(void)
{
  return readUInt8(GMA_REG_COMMAND_STATUS, GMA_CMD_STATUS_ERROR);
}

uint16_t DFRobot_GMA::getADCVoltageMv(DFRobot_GMA::eGasType_t gasType)
{
  switch (gasType) {
    case eGasCO:
      return readUInt16(GMA_REG_AIN0_MV_L, 0);

    case eGasNO2:
      return readUInt16(GMA_REG_AIN1_MV_L, 0);

    case eGasNH3:
      if (_hwModel == 0) {
        getHardwareModel();
      }
      if (_hwModel == GMA_HW_GMA3021B) {
        return readUInt16(GMA_REG_AIN2_MV_L, 0);
      }
      return 0;

    default:
      return 0;
  }
}

uint16_t DFRobot_GMA::getBaselineVoltageMv(DFRobot_GMA::eGasType_t gasType)
{
  switch (gasType) {
    case eGasCO:
      return readUInt16(GMA_REG_BASE_AIN0_MV_L, 0);

    case eGasNO2:
      return readUInt16(GMA_REG_BASE_AIN1_MV_L, 0);

    case eGasNH3:
      if (_hwModel == 0) {
        getHardwareModel();
      }
      if (_hwModel == GMA_HW_GMA3021B) {
        return readUInt16(GMA_REG_BASE_AIN2_MV_L, 0);
      }
      return 0;

    default:
      return 0;
  }
}

bool DFRobot_GMA::readReg(uint8_t reg, uint8_t *pBuf, uint8_t len)
{
  uint8_t i;
  uint8_t received;

  if ((pBuf == NULL) || (len == 0)) {
    return false;
  }

  _pWire->beginTransmission(_i2cAddr);
  _pWire->write(reg);
  if (_pWire->endTransmission() != 0) {
    return false;
  }

  received = _pWire->requestFrom(_i2cAddr, len);
  if (received != len) {
    return false;
  }

  for (i = 0; i < len; i++) {
    if (_pWire->available() == 0) {
      return false;
    }
    pBuf[i] = _pWire->read();
  }

  return true;
}

bool DFRobot_GMA::writeReg(uint8_t reg, const uint8_t *pBuf, uint8_t len)
{
  uint8_t i;

  if ((pBuf == NULL) || (len == 0)) {
    return false;
  }

  _pWire->beginTransmission(_i2cAddr);
  _pWire->write(reg);
  for (i = 0; i < len; i++) {
    _pWire->write(pBuf[i]);
  }

  return (_pWire->endTransmission() == 0);
}

bool DFRobot_GMA::writeCommand(uint8_t command, uint8_t param)
{
  if (writeReg(GMA_REG_COMMAND_PARAM, &param, 1) == false) {
    return false;
  }

  return writeReg(GMA_REG_COMMAND, &command, 1);
}

bool DFRobot_GMA::waitCommandDone(uint16_t timeoutMs)
{
  uint32_t startTime = millis();

  while ((uint32_t)(millis() - startTime) < timeoutMs) {
    uint8_t commandStatus;

    delay(GMA_POWER_STATE_POLL_MS);
    commandStatus = getCommandStatus();
    if (commandStatus == GMA_CMD_STATUS_DONE) {
      return true;
    }
    if (commandStatus == GMA_CMD_STATUS_ERROR) {
      return false;
    }
  }

  return false;
}

static bool waitPowerState(DFRobot_GMA *pGma, uint8_t expectedState, uint16_t timeoutMs)
{
  uint32_t startTime = millis();

  if (pGma == NULL) {
    return false;
  }

  while ((uint32_t)(millis() - startTime) < timeoutMs) {
    if (pGma->getPowerState() == expectedState) {
      return true;
    }
    delay(GMA_POWER_STATE_POLL_MS);
  }

  return false;
}

uint8_t DFRobot_GMA::readUInt8(uint8_t reg, uint8_t defaultValue)
{
  uint8_t value = defaultValue;

  if (readReg(reg, &value, 1) == false) {
    return defaultValue;
  }

  return value;
}

uint16_t DFRobot_GMA::readUInt16(uint8_t reg, uint16_t defaultValue)
{
  uint8_t buf[2];

  if (readReg(reg, buf, sizeof(buf)) == false) {
    return defaultValue;
  }

  return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

float DFRobot_GMA::readFloat(uint8_t reg, float defaultValue)
{
  uint8_t buf[4];

  if (readReg(reg, buf, sizeof(buf)) == false) {
    return defaultValue;
  }

  return bytesToFloat(buf);
}

float DFRobot_GMA::bytesToFloat(const uint8_t *pBuf)
{
  uFloatData_t data;

  data.value    = 0;
  data.bytes[0] = pBuf[0];
  data.bytes[1] = pBuf[1];
  data.bytes[2] = pBuf[2];
  data.bytes[3] = pBuf[3];
  return data.floatValue;
}
