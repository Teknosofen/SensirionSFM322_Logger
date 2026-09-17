#include <Arduino.h>
#include "main.hpp"

// ---------------------------------------------------------------
// FlowSensorTest - LilyGo T-Display S3 + Sensirion SFM3200-MMT-280
//
// In the TFT_eSPI User_Setup_Select.h ensure that this line is
// enabled (same as required by the LundaLogger project):
//   #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
// ---------------------------------------------------------------

TFT_eSPI     tft = TFT_eSPI();
SFM3200      flowSensor(Wire, SFM_SDA_PIN, SFM_SCL_PIN, SFM_I2C_HZ);
FlowDisplay  display(tft);

unsigned long lastUpdateMs = 0;
uint16_t      sampleSeq    = 0;   // 0..1023, wraps
uint32_t      sampleIntervalMs = UPDATE_INTERVAL_MS;   // runtime, host can override

namespace {
constexpr uint32_t MIN_INTERVAL_MS = 5;
constexpr uint32_t MAX_INTERVAL_MS = 60000;
constexpr size_t   CMD_BUF_SIZE    = 32;
char   cmdBuf[CMD_BUF_SIZE];
size_t cmdLen = 0;

void handleCommand(const char* cmd) {
    if (cmd[0] == '?' && cmd[1] == '\0') {
        hostCom.printf("INFO R=%lu\n", (unsigned long)sampleIntervalMs);
        return;
    }
    if ((cmd[0] == 'R' || cmd[0] == 'r') && cmd[1] != '\0') {
        char* end = nullptr;
        long ms = strtol(cmd + 1, &end, 10);
        if (end == cmd + 1 || ms < (long)MIN_INTERVAL_MS || ms > (long)MAX_INTERVAL_MS) {
            hostCom.printf("ERR bad rate: %s\n", cmd);
            return;
        }
        sampleIntervalMs = (uint32_t)ms;
        hostCom.printf("OK R=%lu\n", (unsigned long)sampleIntervalMs);
        return;
    }
    hostCom.printf("ERR unknown: %s\n", cmd);
}

void pollHostCommands() {
    while (hostCom.available()) {
        int c = hostCom.read();
        if (c < 0) break;
        if (c == '\r') continue;
        if (c == '\n') {
            cmdBuf[cmdLen] = '\0';
            if (cmdLen > 0) handleCommand(cmdBuf);
            cmdLen = 0;
            continue;
        }
        if (cmdLen + 1 < CMD_BUF_SIZE) {
            cmdBuf[cmdLen++] = (char)c;
        } else {
            cmdLen = 0;   // overflow, drop line
        }
    }
}
}   // namespace

void setup() {
  hostCom.begin(115200);
  delay(200);
  hostCom.println("FlowSensorTest boot");

  tft.init();
  tft.setRotation(1); // landscape 320x170
  display.begin();

  if (!flowSensor.begin()) {
    hostCom.println("SFM3200 begin() failed - check wiring/power");
    display.showError("SFM3200 FAIL");
  } else {
    uint32_t serial = 0;
    if (flowSensor.readSerialNumber(serial)) {
      hostCom.printf("SFM3200 serial: 0x%08lX\n", (unsigned long)serial);
    }
  }

  lastUpdateMs = millis();
}

void loop() {
  pollHostCommands();

  const unsigned long now = millis();
  if (now - lastUpdateMs < sampleIntervalMs) return;
  lastUpdateMs = now;

  float    flow = 0.0f;
  uint16_t raw  = 0;
  if (flowSensor.readFlow(flow, raw)) {
    display.showFlow(flow, raw, "slm Air", 3);
    hostCom.printf("#%u\t%.3f\n", sampleSeq, flow);
    sampleSeq = (sampleSeq + 1) & 0x03FF;   // roll after 1024
  } else {
    display.showError("read FAIL");
    hostCom.println("flow read FAIL");
  }
}
