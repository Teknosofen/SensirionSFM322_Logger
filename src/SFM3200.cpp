#include "SFM3200.hpp"

SFM3200::SFM3200(TwoWire& wire, int sdaPin, int sclPin, uint32_t i2cClockHz)
    : wire_(wire), sdaPin_(sdaPin), sclPin_(sclPin), clockHz_(i2cClockHz) {}

bool SFM3200::begin() {
    if (sdaPin_ >= 0 && sclPin_ >= 0) {
        wire_.begin(sdaPin_, sclPin_, clockHz_);
    } else {
        wire_.begin();
        wire_.setClock(clockHz_);
    }

    if (!softReset()) return false;
    delay(20);
    return startContinuousMeasurement();
}

bool SFM3200::softReset() {
    const bool ok = writeCommand(CMD_SOFT_RESET);
    measurementStarted_ = false;
    return ok;
}

bool SFM3200::startContinuousMeasurement() {
    if (!writeCommand(CMD_START_MEASUREMENT)) return false;
    measurementStarted_ = true;
    // Datasheet: first measurement is available ~5 ms after the command.
    delay(10);
    return true;
}

bool SFM3200::readSerialNumber(uint32_t& serial) {
    if (!writeCommand(CMD_READ_SERIAL)) return false;

    // Response: 6 bytes = [MSB1 MSB0 CRC] [LSB1 LSB0 CRC]
    const uint8_t n = wire_.requestFrom((int)I2C_ADDR, 6);
    if (n != 6) return false;

    uint8_t buf[6];
    for (uint8_t i = 0; i < 6; ++i) buf[i] = wire_.read();
    if (crc8(&buf[0], 2) != buf[2]) return false;
    if (crc8(&buf[3], 2) != buf[5]) return false;

    serial = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
           | ((uint32_t)buf[3] << 8)  |  (uint32_t)buf[4];

    // readSerial breaks measurement mode; restart it for the caller.
    measurementStarted_ = false;
    startContinuousMeasurement();
    return true;
}

bool SFM3200::readFlow(float& flowSlm) {
    uint16_t raw;
    return readFlow(flowSlm, raw);
}

bool SFM3200::readFlow(float& flowSlm, uint16_t& raw) {
    if (!measurementStarted_ && !startContinuousMeasurement()) return false;

    const uint8_t n = wire_.requestFrom((int)I2C_ADDR, 3);
    if (n != 3) return false;

    uint8_t buf[3];
    buf[0] = wire_.read();
    buf[1] = wire_.read();
    buf[2] = wire_.read();
    if (crc8(buf, 2) != buf[2]) return false;

    raw = ((uint16_t)buf[0] << 8) | buf[1];
    flowSlm = ((float)raw - offset_) / scale_;
    return true;
}

void SFM3200::setCalibration(float offset, float scaleFactor) {
    offset_ = offset;
    scale_  = scaleFactor;
}

bool SFM3200::writeCommand(uint16_t cmd) {
    wire_.beginTransmission(I2C_ADDR);
    wire_.write((uint8_t)(cmd >> 8));
    wire_.write((uint8_t)(cmd & 0xFF));
    return wire_.endTransmission() == 0;
}

uint8_t SFM3200::crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}
