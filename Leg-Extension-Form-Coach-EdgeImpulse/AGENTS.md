# AGENTS.md – Leg Extension Form Coach

Guide for AI coding agents (and humans) working on this folder.

## What this project is
Embedded Systems (NTUST) final project. A **Seeed Wio Terminal** fixed to the arm of a leg-extension machine
uses its built-in **LIS3DHTR accelerometer** and an **Edge Impulse** model to classify the quadriceps
extension as `idle / slow / correct / fast`, counts only correct reps (lenient), and gives screen + buzzer
feedback after each rep. Full description: `PLAN.md`.

## File map
| Path | Purpose |
|---|---|
| `PLAN.md` | Goal, architecture, rep state machine, phases, test plan |
| `PINOUT.md` | Built-in resources used, buttons, mounting on the machine |
| `DATA_COLLECTION.md` | How to record the training takes (tempo per class, amount, saving CSVs) |
| `EDGE_IMPULSE_SETUP.md` | Web Studio steps: upload CSV, impulse, training, Arduino library export |
| `data_collection/data_collection.ino` | Records labelled 10 s takes at 100 Hz, prints them as CSV |
| `tools/serial_to_csv.py` | Optional: saves each printed take to its own CSV file (pyserial) |
| `leg_extension_coach/leg_extension_coach.ino` | Main firmware: sampling, inference, buttons, glue |
| `leg_extension_coach/config.h` | **All** tunable constants |
| `leg_extension_coach/rep_counter.h` | Angle-based rep state machine + vote-based rep evaluation |
| `leg_extension_coach/feedback.h` | TFT screens + non-blocking buzzer melodies |

## Build
- Arduino IDE, board **Seeeduino Wio Terminal** (Seeed SAMD core). Serial at 115200.
- Libraries: `Grove-3-Axis-Digital-Accelerometer-2g-to-16g-LIS3DHTR`, `TFT_eSPI` (bundled with the core),
  and the Edge Impulse export `Leg_Extension_Coach_inferencing` (only for the coach sketch).
- Command line equivalent:
  ```
  arduino-cli compile --fqbn Seeeduino:samd:seeed_wio_terminal data_collection
  arduino-cli compile --fqbn Seeeduino:samd:seeed_wio_terminal leg_extension_coach
  arduino-cli upload  --fqbn Seeeduino:samd:seeed_wio_terminal -p /dev/ttyACM0 leg_extension_coach
  ```
- The coach sketch does not compile until the Edge Impulse library is installed. The user uses the
  **Edge Impulse web Studio only** – do not introduce the Edge Impulse CLI / data forwarder.

## Conventions
- Comments and on-screen text in **English**; explain *why*, at the density of the existing files.
- Each sketch starts with a header block describing purpose, hardware and libraries (repo style).
- Sketch folder name == `.ino` name. `tft.setRotation(3)`. Buzzer via `tone(WIO_BUZZER, …)`.
- **No `delay()` in the coach's `loop()` path**: sampling must stay at the model's rate. Sounds use the
  non-blocking player in `feedback.h` (`playMelody` + `soundUpdate`). Blocking is only allowed in
  `calibrateRest()` and in `data_collection.ino`.
- New tunable numbers go in `config.h`, not inline.
- Types used in function signatures in the `.ino` must be declared **before the first function** (the Arduino
  IDE auto-generates prototypes there; see the `Button` struct).

## Invariants – keep these consistent or the model breaks
- Labels: exactly `idle`, `slow`, `correct`, `fast` (data collection `LABELS[]`, Edge Impulse, `labelToClass()`).
- Units **m/s²** (g × 9.80665), **100 Hz**, **±4 g** range, axis order **accX, accY, accZ** in both sketches.
- The coach takes its sampling period and window size from the model macros
  (`EI_CLASSIFIER_INTERVAL_MS`, `EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE`); do not hard-code them.
- The arm angle is the angle between the current filtered gravity vector and the calibrated rest vector, so
  it is independent of how the Wio is oriented on the arm.

## Updating the model
1. Retrain / redeploy in Studio (Arduino library, quantized int8, EON).
2. Remove the old `Leg_Extension_Coach_inferencing` folder from the Arduino libraries folder, add the new zip.
3. If the project name changed, update the `#include` at the top of `leg_extension_coach.ino`.

## Do / Don't
- Do test changes to `rep_counter.h` by reading the Serial log (`angle … state … scores`, `Rep votes …`).
- Do keep the counting lenient (the user asked for "not so strict").
- Don't commit generated Edge Impulse libraries or large datasets unless asked.
- Don't commit or push unless the user asks.
