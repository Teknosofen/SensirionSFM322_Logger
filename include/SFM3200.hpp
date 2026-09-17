#ifndef SFM3200_HPP
#define SFM3200_HPP

// -----------------------------------------------------------------
// SFM3200 - Sensirion proximal flow sensor (I2C)
//
// Datasheet-defined protocol values:
//   - I2C 7-bit address        : 0x40
//   - Start measurement command: 0x1000
//   - Soft reset command       : 0x2000
//   - Read serial number cmd   : 0x31AE
//   - Frame format             : [flow_MSB][flow_LSB][CRC-8]
//   - CRC-8 polynomial         : 0x131 (init 0x00)
//   - Flow [slm] = (raw - offset) / scaleFactor
//
// Calibration used here: offset = 10000, scaleFactor = 180
// (i.e. 1/180 slm per count). Verify against your datasheet and
// override via setCalibration() if needed.
// -----------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>

class SFM3200 {
public:
    static constexpr uint8_t  I2C_ADDR              = 0x40;
    static constexpr uint16_t CMD_START_MEASUREMENT = 0x1000;
    static constexpr uint16_t CMD_SOFT_RESET        = 0x2000;
    static constexpr uint16_t CMD_READ_SERIAL       = 0x31AE;

    static constexpr float DEFAULT_OFFSET = 10000.0f;
    static constexpr float DEFAULT_SCALE  = 180.0f;

    // Uses the default SDA/SCL pins for the platform if sda/scl are left as -1.
    explicit SFM3200(TwoWire& wire = Wire, int sdaPin = -1, int sclPin = -1,
                     uint32_t i2cClockHz = 100000);

    bool begin();
    bool softReset();
    bool startContinuousMeasurement();
    bool readSerialNumber(uint32_t& serial);

    // Reads one measurement frame and returns flow in slm (standard liters/min).
    bool readFlow(float& flowSlm);

    // Also returns the raw 16-bit sensor value alongside flow.
    bool readFlow(float& flowSlm, uint16_t& raw);

    void setCalibration(float offset, float scaleFactor);

    float offset() const { return offset_; }
    float scaleFactor() const { return scale_; }

private:
    TwoWire& wire_;
    int      sdaPin_;
    int      sclPin_;
    uint32_t clockHz_;
    float    offset_ = DEFAULT_OFFSET;
    float    scale_  = DEFAULT_SCALE;
    bool     measurementStarted_ = false;

    bool writeCommand(uint16_t cmd);
    static uint8_t crc8(const uint8_t* data, size_t len);
};

#endif // SFM3200_HPP
