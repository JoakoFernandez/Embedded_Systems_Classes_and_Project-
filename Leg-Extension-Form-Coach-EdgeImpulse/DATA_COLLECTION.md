# Data Collection Instructions

How to record the training data for the four classes (`idle`, `slow`, `correct`, `fast`) with the Wio Terminal
on the leg-extension machine. The result is a set of CSV files that you upload to Edge Impulse Studio
(see `EDGE_IMPULSE_SETUP.md`, step 4).

## What you need
- Wio Terminal + USB-C cable + laptop with the Arduino IDE (LIS3DHTR library installed).
- A way to fix the Wio to the machine arm (velcro strap, bands or a clip).
- A metronome app on your phone (to keep the tempo of each class consistent).
- Ideally a helper: one person exercises, the other handles the laptop and presses the button.
- Optional: Python 3 with `pip install pyserial` for the automatic saver script.

## 1. Set up

1. Upload `data_collection/data_collection.ino` to the Wio Terminal.
2. Fix the Wio to the **moving arm, close to the leg pad, screen facing the user** (see `PINOUT.md`).
   Take a photo of how it is mounted: you must use **the same position** later with the coach sketch.
3. Check it does not wobble when you shake the arm by hand.
4. Choose how to save the takes:
   - **Script (recommended):** close the Serial Monitor and run, from the project folder:
     ```
     python tools/serial_to_csv.py --port /dev/ttyACM0 --out dataset    # Linux
     python tools/serial_to_csv.py --port COM5 --out dataset            # Windows
     ```
     (In the Arduino IDE, *Tools → Port* shows the port name.) Each take is saved as its own file in `dataset/`.
   - **Serial Monitor (no Python):** open it at **115200 baud**. After each take, copy the lines between
     `--- BEGIN ... ---` and `--- END ---` into a new text file named as shown in the BEGIN line
     (e.g. `correct.3fa2.07.csv`). Copy the header `timestamp,accX,accY,accZ` too, **not** the marker lines.

## 2. Record one take

| Step | On the Wio |
|---|---|
| 1. Choose the label | 5-way switch **LEFT / RIGHT** → `idle` / `slow` / `correct` / `fast`. The screen shows the tempo hint and how many takes you have for that label. |
| 2. Start | Press **KEY C** (top left button). |
| 3. Get ready | 3 short beeps, one per second (blue screen with countdown). |
| 4. Exercise | High beep + RED screen = recording for **10 s**. A progress bar fills at the bottom. Do the movement at the tempo of the selected class during the whole take. |
| 5. Done | Long low beep, back to the menu. The take is printed on Serial (and saved if the script is running). |

Rules for every take:
- Start moving when the screen turns red, keep going until the long beep.
- Only do the movement of the selected class during the whole take (no mixing).
- If something goes wrong (wrong tempo, device moved, someone bumped the machine), **note the file name and
  delete that file** later. Bad data hurts more than less data.

## 3. What each class looks like

Set the metronome to **60 BPM (1 beat = 1 s)** and count beats.

### `correct` – controlled rep with a stop at the top
- Up in **~2 beats**, **hold ~1 beat** at the top, down in **2–3 beats**, brief rest at the bottom.
- ≈ 2 reps per 10 s take.

### `slow` – dragging, too slow
- Up in **4+ beats**, hold, down in **4+ beats**.
- ≈ 1 rep per take; it is fine if a take ends mid-rep.

### `fast` – explosive / swinging
- Up in **less than 1 beat**, no stop at the top, let the weight drop down in less than 1 beat.
- ≈ 4–6 reps per take. Use a light weight and be careful with your knees.

### `idle` – no exercise
- Sitting on the machine with the arm at rest: still, small fidgets, shifting position, breathing,
  touching the Wio, someone walking next to the machine.
- Also include takes with the arm resting still in different positions (e.g. holding the top a long time),
  so the model learns that "not moving" is idle wherever the arm is.

## 4. How much data

| Label | Minimum | Better |
|---|---|---|
| `idle` | 30 takes (5 min) | 45 takes |
| `slow` | 30 takes (5 min) | 45 takes |
| `correct` | 30 takes (5 min) | 45 takes |
| `fast` | 30 takes (5 min) | 45 takes |

- Keep the classes **balanced** (similar number of takes each).
- Record with **at least 2–3 different people** and **2–3 different weights** if possible.
- Vary the speed a little *inside* each class (e.g. `correct` at 2 s and at 3 s down) so the model is not too
  strict.
- Split the recording over several short sessions instead of one long one (fatigue changes the movement,
  which is actually good variety, but take breaks).

A suggested session (about 20 min): 8 takes of each class, in rotating order
(`idle → correct → slow → fast → idle → …`) so all classes get the same conditions.

## 5. Check the files before uploading

- File names start with the label: `idle.*.csv`, `slow.*.csv`, `correct.*.csv`, `fast.*.csv`
  (Edge Impulse takes the label from the text before the first dot).
- Each file starts with `timestamp,accX,accY,accZ` and has ~1000 data lines (10 s × 100 Hz).
- No `--- BEGIN` / `--- END` lines inside the files.
- Delete the takes you noted as bad.

Then follow `EDGE_IMPULSE_SETUP.md` from step 4 (CSV Wizard + Upload data).

## Safety
- Use a weight you can control easily, especially for the `fast` class.
- Stop if anything hurts. The data does not need maximum effort, only a clear tempo.
- Make sure the strap holding the Wio cannot get caught in the machine.
