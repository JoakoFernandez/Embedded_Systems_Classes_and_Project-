# Edge Impulse Setup (web Studio only)

Everything here is done in the browser at <https://studio.edgeimpulse.com> plus the Arduino IDE.
**No Edge Impulse CLI is needed.** Data goes from the Wio Terminal to CSV files, the CSV files are uploaded
in Studio, and the trained model comes back as an Arduino library (.zip).

```
 Wio + data_collection.ino ──Serial──► CSV files ──upload──► Edge Impulse Studio
                                                               │ train
 Wio + leg_extension_coach.ino ◄── Arduino library (.zip) ◄────┘ deploy
```

---

## 1. Prepare the Arduino IDE

1. Board package: **Seeed SAMD Boards** (Board Manager) → board **Seeeduino Wio Terminal**.
2. Library Manager → search **LIS3DHTR** → install **Grove 3-Axis Digital Accelerometer ±2g to 16g (LIS3DHTR)**
   by Seeed Studio.
3. `TFT_eSPI` already comes with the Seeed board package.

## 2. Create the Edge Impulse project

1. Log in → **Create new project** → name it **`Leg Extension Coach`**.
   The exported library will then be called `Leg_Extension_Coach_inferencing` and the sketch already includes
   `<Leg_Extension_Coach_inferencing.h>`. If you choose another name, change that `#include` line in
   `leg_extension_coach.ino`.
2. In the project dashboard, set the target device (top right, *Target*) to **Seeed Wio Terminal**
   (or *Cortex-M4F 120 MHz* if it is not listed). This only affects the time/memory estimates.

## 3. Record the data

Follow **`DATA_COLLECTION.md`**: mounting, how to record takes with `data_collection.ino`, the tempo of each
class, how much data, and how to save the CSV files (Serial Monitor or `tools/serial_to_csv.py`).

Summary of what matters for Edge Impulse:
- File names start with the label (`correct.3fa2.07.csv`): Studio reads the label from the text before the first dot.
- Labels are exactly `idle`, `slow`, `correct`, `fast` (lowercase): the firmware compares these strings.
- CSV columns `timestamp,accX,accY,accZ` (ms, m/s²), 100 Hz, ±4 g.
- At least ~5 minutes per class, balanced.

## 4. Upload the CSV files to Studio

1. The first time, teach Studio the CSV format: **Data acquisition → CSV Wizard**.
   - Upload one of your files (e.g. `correct.xxxx.01.csv`).
   - *Is this time-series data?* → **Yes**, *timestamp* column = `timestamp`, unit **milliseconds**
     (frequency detected ≈ 100 Hz).
   - Label: **no label column** (the label comes from the file name).
   - Value columns: `accX`, `accY`, `accZ`.
   - Do not split the samples (keep each 10 s take as one sample).
   - Finish the wizard.
2. **Data acquisition → Upload data** (the upload icon):
   - Select all your CSV files.
   - *Upload into category*: **Automatically split between training and testing** (80/20).
   - *Label*: **Infer from filename** (the text before the first `.` → `correct`, `slow`, …).
   - Click **Upload data**.
3. Check the dataset: the 4 labels appear, the waveforms look right, and the train/test split is ~80/20.
   Delete bad takes (started late, device moved, etc.).

## 5. Design the impulse

**Impulse design → Create impulse**

| Block | Setting |
|---|---|
| Time series data | Window size **2000 ms** · Window increase **250 ms** · Frequency **100 Hz** · Zero-pad data ✔ |
| Processing block | **Spectral Analysis** (inputs accX, accY, accZ) |
| Learning block | **Classification** |

Click **Save Impulse**.

> Window 2000 ms / increase 250 ms match the firmware: it keeps the last 2 s of data and classifies it every
> 250 ms (`INFERENCE_EVERY_MS` in `config.h`). The window length is read automatically from the exported model.

## 6. Generate features

**Spectral features** → keep defaults (Filter: low pass, FFT length 16–64) → **Save parameters** →
**Generate features**. In the *Feature explorer* the four classes should form visible groups; if `slow` and
`correct` overlap a lot, record clearer takes of each.

## 7. Train the classifier

**Classifier** →
- Training cycles: **100** (raise to 200 if accuracy is still improving)
- Learning rate: **0.0005**
- Architecture: default (Dense 20 → Dense 10) is enough
- **Save & train**

Look at the confusion matrix. Aim for > 85 % accuracy. Typical confusion is `slow` ↔ `correct`; the firmware is
lenient (a rep is correct with only 40 % correct votes), so moderate confusion is acceptable.

## 8. Test the model

**Model testing → Classify all** to check accuracy on the 20 % test data the model never saw.
Optional: **Live classification** is not needed (it requires a connected device), the test set is enough.

## 9. Export as an Arduino library

1. **Deployment** → search **Arduino library**.
2. Optimization: **Quantized (int8)** with **EON Compiler** enabled (smaller and faster; float32 also fits on the
   Wio if you prefer).
3. **Build** → a file like `ei-leg-extension-coach-arduino-1.0.1.zip` is downloaded.
4. Arduino IDE → **Sketch → Include Library → Add .ZIP Library…** → choose that zip.
5. Open `leg_extension_coach/leg_extension_coach.ino`, board *Seeeduino Wio Terminal*, **Upload**.
   The first compile of an Edge Impulse library takes several minutes; that is normal.

When you retrain the model later: remove the old library folder from `~/Arduino/libraries/` (Linux/Mac) or
`Documents\Arduino\libraries\` (Windows) and add the new zip again.

## 10. Check it works

Open the Serial Monitor (115200). Every 250 ms you should see a line like
```
angle  34.2  speed   41.0  UP   | correct 0.86 fast 0.03 idle 0.01 slow 0.10  (dsp 2 ms, nn 1 ms)
```
and after each rep:
```
Rep votes  slow 1  correct 9  fast 0  (5120 ms)
REP correct  (5120 ms)  correct 3 / total 4
```

## Troubleshooting

| Problem | Fix |
|---|---|
| `Leg_Extension_Coach_inferencing.h: No such file` | Library not installed, or project has another name → change the `#include`. |
| `The Edge Impulse model must use 3 axes` | The impulse uses other inputs; make sure accX, accY, accZ are all selected. |
| Always predicts one class | Unbalanced dataset or the Wio was mounted differently than during recording. |
| Reps not detected | Look at the angle on screen during a rep; adjust `TOP_DELTA_DEG` / `START_DELTA_DEG` in `config.h`, recalibrate with button B. |
| Good reps marked wrong | Lower `CORRECT_VOTE_RATIO` or `MIN_CONFIDENCE`, or add more `correct` data. |
