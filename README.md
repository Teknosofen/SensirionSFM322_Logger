# FlowSensorTest

PlatformIO project for a flow-sensor test rig running on the
**LilyGo T-Display S3** (ESP32-S3, ST7789 170x320 TFT), reading a
**Sensirion SFM3200-MMT-280** proximal flow sensor over I2C.

The firmware shows live flow on the TFT and streams samples over USB
serial. Two host-side tools (Python and browser) plot and record that
stream.

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

> **Note:** this edit lives inside `.pio/`, which is not in version
> control. It is lost on a clean build or a fresh clone, and the
> symptom is a garbled or blank display rather than a build error. If
> the screen looks wrong after a rebuild, check this first.

## Wiring

The SFM3200 is an I2C device at 7-bit address `0x40`. It is a 5 V part,
so the bus does **not** connect directly to the T-Display — a
bidirectional level converter sits between them:

```
T-Display S3 (3.3 V)       level converter        SFM3200 (5 V)

           3V3 ─┬──┬─ 3k3           5V ─┬──┬─ 3k3
                │  │                    │  │
  GPIO43  SDA ──┴──┼──►  LV1 ── HV1 ────┴──┼──►  SDA
  GPIO44  SCL ─────┴──►  LV2 ── HV2 ───────┴──►  SCL

          3V3 ─────────►  LV
           5V ─────────────────── HV ────────►  VDD
          GND ─────────►  GND ── GND ────────►  GND
```

The converter is powered from the T-Display itself: its low-voltage
rail from the `3V3` pin, its high-voltage rail from the `5V` pin, with
grounds common. The sensor's VDD comes off the same 5 V rail.

Board-side pins are defined in [include/main.hpp](include/main.hpp):

| Signal | Board pin | Constant      | Cable colour |
|--------|-----------|---------------|--------------|
| SDA    | GPIO43    | `SFM_SDA_PIN` | green        |
| SCL    | GPIO44    | `SFM_SCL_PIN` | blue         |

Bus speed is 400 kHz (`SFM_I2C_HZ`). SDA and SCL carry 3 kΩ pull-ups
on **both** sides of the converter — LV side to 3V3, HV side to 5V.
These are external, added on top of whatever the converter breakout
provides; if the breakout has its own (commonly 10 kΩ), the effective
pull-up is the parallel combination, around 2.3 kΩ. That is well
within spec for 400 kHz and on the strong side by design, which is
what keeps the edges clean over the converter and the cable run.

The T-Display S3 pinout is in
`Documentation/Lilygo-T-display_pinlayout.webp`; sensor electrical
limits are in `Documentation/Sensiorion spec 1.pdf`.

## Calibration

Flow is derived from the raw 16-bit reading as:

```
flow [slm] = (raw - offset) / scaleFactor
```

Defaults are `offset = 10000` and `scaleFactor = 180` (see
[include/SFM3200.hpp](include/SFM3200.hpp)). Verify these against your
datasheet variant and override at runtime with
`flowSensor.setCalibration(offset, scale)` if they differ.

## Build / upload

```
pio run
pio run -t upload
pio device monitor
```

## Serial protocol

115200 baud, 8N1. Samples are emitted as tab-separated lines:

```
#<seq>\t<flow>\n
```

`seq` is a sample counter that wraps at 1024; `flow` is in slm (Air),
three decimals. Default sample interval is 50 ms (20 Hz).

The firmware also accepts newline-terminated commands on the same port:

| Command | Effect                                      | Reply         |
|---------|---------------------------------------------|---------------|
| `R<ms>` | Set sample interval, 5–60000 ms             | `OK R=<ms>`   |
| `?`     | Query current interval                      | `INFO R=<ms>` |
| other   | —                                           | `ERR ...`     |

Errors on the sensor side print `flow read FAIL` and show `read FAIL`
on the TFT.

## Host tools

Both tools plot flow live and record CSV with the columns
`iso_time, epoch_s, seq, flow_slm`. They connect to the same serial
port, so only run one at a time.

### `tools/flow_monitor.py`

Matplotlib plot with a Tk port picker.

```
pip install -r tools/requirements.txt
python tools/flow_monitor.py [--port COM7] [--baud 115200] [--window 30] [--outdir recordings]
```

If `--port` is omitted a port-selection dialog is shown. In the plot
window: `r` / **Record** toggles CSV recording (new file per session,
written to `recordings/`), `c` / **Clear** empties the on-screen
buffer, `q` quits. The **Hz** text box sends an `R<ms>` command to
change the sample rate on the fly.

### `tools/flow_monitor.html`

Standalone page using the Web Serial API — no install needed, but
requires a Chromium-based browser (Chrome, Edge, Opera, Brave).
Firefox and Safari are not supported. Open the file and click
**Connect…**.

Same controls as the Python tool, plus an exponential smoothing filter:

```
filtered[n] = a * filtered[n-1] + (1 - a) * flow[n]
```

where `a` is the **Forgetting factor** (0–1, default 0.9). Higher
values smooth harder and lag more. Both traces are drawn — unfiltered
in green, filtered in white — and the CSV written here carries an
extra `filtered_flow_slm` column.
