# Smart Fall Detection and SOS Alert System

An embedded fall-detection and manual-SOS system built on the Silicon Labs
SiWG917 Wi-Fi SoC (BRD2605A Dev Kit). The device uses the onboard six-axis
IMU to detect a genuine fall in real time, and a physical button as a
secondary manual alert trigger. Cloud alerting (Firebase Realtime Database)
is designed in and partially implemented, but not yet completed — see
[Current Status](#current-status) below.

## Hardware

- Silicon Labs **BRD2605A Dev Kit** (SiWG917M111MGTBA SoC)
- Onboard **ICM40627** 6-axis IMU (accelerometer + gyroscope)
- Onboard push button **BTN0** (manual SOS trigger)
- USB cable for power and programming
- 2.4 GHz Wi-Fi access point for network connectivity

## Software / Toolchain

- Simplicity Studio 6 (project configuration, component management)
- Visual Studio Code + Silicon Labs extension (firmware development)
- WiSeConnect 3 SDK
- FreeRTOS (via `sl_main` / `cmsis_os2`)
- Serial terminal at 115200 baud (Simplicity Studio console, or PuTTY / Tera Term)

## Project Structure

```
fall_detection_sos/
├── app.c / app.h              # Application entry point, Wi-Fi bring-up, main loop
├── icm40627_example.c/.h      # IMU driver + fall-detection state machine
├── gpio_uulp_example.c/.h     # SOS button (BTN0) driver, debounced
├── main.c                     # Generated entry point
├── config/                    # Auto-generated component configuration
├── fall_detection_sos.slcp    # Simplicity Studio project/component definition
└── readme.md
```

## Setup Instructions

Before building, set two things in **`app.h`**:

```c
#define DEFAULT_WIFI_CLIENT_PROFILE_SSID "YOUR_WIFI_SSID"
#define DEFAULT_WIFI_CLIENT_CREDENTIAL   "YOUR_WIFI_PASSWORD"
```

Replace with your own 2.4 GHz network's credentials (the SiWG917 radio is
2.4 GHz only). Do not commit real credentials to source control.

## Build Instructions

1. Open the project in Simplicity Studio (target: BRD2605A).
2. In **Software Components**, confirm the following are installed:
   - Sleep Timer for Si91x
   - GPIO (Peripheral)
   - ICM40627
   - SSI (`ulp_primary` instance)
   - Wi-Fi / Network Manager components (included by default from the
     Station Ping base example)
3. Open in VS Code (GCC), then **Clean** and **Build**.

> **Note:** In this project's history, reinstalling or regenerating a
> component through the Software Components GUI has occasionally reset
> `cmake_gcc/<project>.cmake`, dropping manually-added source files or
> include paths. If a rebuild suddenly reports `undefined reference` or
> `No such file or directory` for `icm40627_example.c`,
> `gpio_uulp_example.c`, or SSI/Sleeptimer symbols, check that file's
> `add_library(slc OBJECT ...)` and `target_include_directories(...)`
> blocks still list them, and re-add if missing.

## Flashing

1. Connect the BRD2605A via USB.
2. Flash from VS Code / Simplicity Studio as normal.
3. Open a serial console at **115200 baud**.

## Testing

On boot, the console should show, in order:
- GPIO driver initialization messages
- ICM40627 initialization messages (including `Sleeptimer start status: 0x0`)
- Wi-Fi connection messages
- `Entering main loop`, followed by periodic `Main loop alive` heartbeats

**SOS button:** press BTN0 — the console should print exactly one
`SOS BUTTON PRESSED` per physical press (debounced, no bounce spam).

**Fall detection:** with `FALL_DEBUG_MODE` set to `1` in
`icm40627_example.c`, the console streams `mag=<value> state=<0-3>` on
every sample. A genuine drop (release the board briefly onto a soft
surface — do not swing or guide it by hand) should show:
- `state=0 → 1`: a free-fall dip (magnitude below ~0.6g)
- `state=1 → 2`: an impact spike (magnitude above ~2.0g)
- `state=2 → 3`: after ~2 seconds of settling, the console prints
  `FALL DETECTED`

**Important:** once a fall is confirmed (`state=3`), the system
deliberately **latches** in that state and will not print another
`FALL DETECTED` for the same event. This is intentional — a reset
(`icm40627_fall_detection_reset()`) is provided as a hook, intended to be
called once an alert has been successfully sent, or manually, but nothing
currently calls it automatically.

## Fall Detection Algorithm

Implemented as a 4-state machine in `icm40627_example.c`, driven by the
combined acceleration magnitude `sqrt(x² + y² + z²)`:

| State | Meaning | Trigger condition |
|---|---|---|
| `NORMAL` (0) | Resting / ordinary movement | — |
| `FREEFALL_DETECTED` (1) | Possible free-fall dip seen | magnitude < 0.6g for ≥3 consecutive samples |
| `IMPACT_DETECTED` (2) | Impact spike seen after a dip | magnitude > 2.0g within 1s of entering state 1 |
| `CONFIRMED` (3) | Fall confirmed | magnitude settles into 0.7–1.3g for ~2s (brief excursions up to 300ms tolerated) |

Thresholds were tuned empirically using controlled drop tests and may need
further adjustment for a specific mounting location or use case (see
[Future Work](#future-work)).

## Current Status

- ✅ Wi-Fi connectivity (non-fatal on failure — sensors and button keep
  working even if Wi-Fi doesn't connect)
- ✅ IMU-based automatic fall detection, tuned and verified against real
  drop tests
- ✅ Manual SOS button (BTN0), debounced
- ⏳ **Cloud alerting (Firebase Realtime Database) — attempted, not yet
  completed.** An HTTP Client component and HTTPS/TLS certificate setup
  were explored; the `send_fall_alert()` integration referenced by the
  `TODO` comments in `icm40627_example.c` and `gpio_uulp_example.c` is not
  yet implemented. This is the primary remaining task.

## Future Work

- Complete the Firebase Realtime Database HTTPS integration
- Automatic or event-driven reset of the fall-detection state after an
  alert is sent
- Further threshold tuning using real-world fall data / more drop tests
- GPS-based location tagging for alerts
- Caregiver-facing dashboard for live alert monitoring

## License

Portions of this project are adapted from Silicon Laboratories' WiSeConnect
3 SDK example projects (Station Ping, ICM40627, GPIO UULP), used under the
Silicon Labs Master Software License Agreement / Zlib license as stated in
each source file's header.
