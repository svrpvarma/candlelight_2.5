# qtcandlelightfdcanbus – CAN FD candleLight plugin for SavvyCAN (Windows)

Qt 6 SerialBus plugin for gs_usb / candleLight adapters **with CAN FD**, written for the
ElmüSoft *CANable 2.5* Candlelight firmware (e.g. Jhoinrch RH02 Elite with
`STM32G431-Candlelight2.5-Jhoinrch.dfu`). Also works with classic candleLight adapters.

Based on [candleLight_fw-SavvyCAN-Windows-plugin](https://github.com/homewsn/candleLight_fw-SavvyCAN-Windows-plugin)
(Vladimir Alemasov, GPLv2) and the candle Windows API (Hubert Denkmair, LGPLv3).

## Changes from the original plugin

| | Original | This plugin |
|---|---|---|
| Qt version | Qt 5 only | **Qt 5.15 and Qt 6** (one source, CI builds both) |
| CAN FD | no (8-byte frames only) | **yes**: FDF/BRS/ESI, 64-byte payloads, data bitrate (`BT_CONST_EXT`, `DATA_BITTIMING`) |
| Bit timing | fixed table for 16/48/170 MHz only, 87.5 % | **calculated** from the limits the firmware reports, any clock (160 MHz CANable 2.5), common prescaler for both phases |
| Sample points | fixed | configurable in `qtcandlelightfd.ini` |
| Listen-only | ignored | honoured (SavvyCAN "Listen only") |
| Bus errors | dropped | delivered as Qt error frames (bus-off, no ACK, error passive, TEC/REC) |
| Port name | `gs_usb0`, `gs_usb1` (enumeration order) | `gs_usb_<USB serial>` – stable across USB ports |
| Unplug | thread kept spinning | reported as ConnectionError, device closes |
| Timestamps | wrapped after ~71 min | 32-bit wrap handled |

## Build

**GitHub Actions (easiest):** push this folder to a GitHub repository. The workflow in
`.github/workflows/build.yml` builds two DLLs and uploads them as artifacts (Actions tab → latest run → Artifacts):

| Artifact | For |
|---|---|
| `qtcandlelightfdcanbus-qt5.15.2-msvc2019-x64` | SavvyCAN **V213 / V220** releases (Qt 5.15.2) |
| `qtcandlelightfdcanbus-qt6.10-msvc2022-x64` | SavvyCAN **Qt6 development builds** |

**Locally:** install Qt 6.10 for MSVC 2022 64-bit with the *Qt Serial Bus* module and Visual Studio 2022
(Desktop C++ workload). From an *x64 Native Tools Command Prompt*:

```
cmake -B build -S . -DCMAKE_PREFIX_PATH=C:\Qt\6.10.0\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

For Qt 5.15.2 use the *MSVC 2019 64-bit* kit and `-DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64`.
The Qt version must match your SavvyCAN build.

## SavvyCAN V213 / V220 (Qt 5) and CAN FD

These versions pass the nominal bitrate, the *CAN FD* checkbox and listen-only to the plugin, but
**not the CAN FD data rate** - set it in `qtcandlelightfd.ini` (`[bus] DataBitrate=`). They also do not
copy the FD/BRS flags into the frame list, so FD frames show with their full payload (up to 64 bytes)
but without an FD marker. Frames longer than 8 bytes sent from SavvyCAN go out as CAN FD
(with BRS unless `BitrateSwitch=false`).

## Install

1. Copy `qtcandlelightfdcanbus.dll` into the `canbus` folder of SavvyCAN (next to the Qt-supplied
   plugins such as `qtpeakcanbus.dll`).
2. Optional: copy `qtcandlelightfd.ini` next to it and set the sample points to match your bus.
3. In SavvyCAN: *Connection → Add New Device Connection → QT SerialBus Devices*,
   SerialBus device type **candleLightFD**, port **gs_usb_<serial>**. Tick *CAN FD* and set the data rate.

## Notes

* Close HUD ECU Hacker / Cangaroo first – WinUSB allows one program per adapter.
* An adapter that is already open is not listed.
* If connecting fails with "rejected the bit timing", check the bitrate/sample point is reachable
  with the adapter's CAN clock (e.g. 3 Mbit/s cannot be made exactly from 160 MHz).
* Run SavvyCAN with `QT_LOGGING_RULES=qt.canbus*=true;default.debug=true` and a debugger/DebugView
  to see the calculated timing in the debug output.
