# FlowSensorTest

PlatformIO project for a flow-sensor test rig running on the
**LilyGo T-Display S3** (ESP32-S3, ST7789 170x320 TFT).

Hardware and display library configuration mirrors the
`LundaLoggern` project.

## Toolchain

- PlatformIO / Arduino framework
- Board: `lilygo-t-display-s3`
- Display library: `bodmer/TFT_eSPI @ 2.5.43`

## One-time TFT_eSPI setup

The TFT_eSPI library needs to know which display to use. After the
first build (which downloads the library into `.pio/libdeps/...`),
open:

```
.pio/libdeps/lilygo-t-display-s3/TFT_eSPI/User_Setup_Select.h
```

Comment out the default `#include <User_Setup.h>` line and enable:

```cpp
#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
```

This is the same manual step used by the LundaLoggern project.

## Wiring

Connect the flow sensor's pulse output to `FLOW_SENSOR_PIN`
(default GPIO1, configurable in `include/main.hpp`). Sensor GND to
board GND, sensor VCC to 3V3 or 5V per its datasheet.

## Build / upload

```
pio run
pio run -t upload
pio device monitor
```
