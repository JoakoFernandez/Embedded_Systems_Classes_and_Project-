# Project Plan – Leg Extension Form Coach

## Goal
A Wio Terminal fixed to the arm of a leg-extension (quadriceps) machine that, in real time:
1. Classifies the movement with an Edge Impulse model into **idle / slow / correct / fast**.
2. Counts **only correct reps** (with a lenient rule, so it is not frustrating).
3. Gives immediate feedback after every rep:
   - **Correct** → green screen, support words ("Great job!", "Perfect form!", …), coin-pickup style "bling".
   - **Too slow / too fast** → red screen with "TOO SLOW" / "TOO FAST" and a hint, uncomfortable buzz.

## What is one rep
```
 rest ──► moving up ──► stop at the top ──► moving down ──► rest
```

## Architecture

```
                 ┌──────────────── Wio Terminal ────────────────────────────┐
                 │                                                          │
 LIS3DHTR ──100 Hz──► ring buffer (last 2 s) ──every 250 ms──► Edge Impulse │
 (Wire1)         │        │                                   classifier    │
                 │        │                                      │          │
                 │        ▼                                      ▼          │
                 │  low-pass gravity ──► arm angle ──► REP STATE MACHINE    │
                 │                                    (rest/up/top/down)    │
                 │                                        │   ▲ votes       │
                 │                                        ▼   │ (slow/      │
                 │                                rep finished  correct/    │
                 │                                        │      fast)      │
                 │                                        ▼                 │
                 │                         evaluate rep (≥40 % correct)     │
                 │                           │                     │        │
                 │                     GREEN + bling          RED + buzz    │
                 │                     count + 1              not counted   │
                 └──────────────────────────────────────────────────────────┘
```

### Two jobs, two tools
| Job | Tool | Why |
|---|---|---|
| **When** a rep starts/ends | Arm angle state machine (`rep_counter.h`) | Angle from gravity is simple and very reliable for counting. |
| **How** the rep was done | Edge Impulse model (`run_classifier`) | Tempo/smoothness is a pattern in the signal, ideal for ML. |

### Rep state machine (angles from the calibrated rest position)
| From | To | Condition |
|---|---|---|
| REST | UP | angle > 15° |
| UP | TOP | angle > 45° **and** speed < 25°/s for 200 ms |
| UP | DOWN | top reached but no clear stop, angle drops 10° from the peak (lenient) |
| UP | REST | back below 10° without reaching 45° → half rep, ignored |
| TOP | DOWN | angle drops 10° from the peak |
| DOWN | REST | angle < 10° → **rep finished, evaluate** |
| any | REST | rep longer than 20 s → ignored |

Different start (15°) and end (10°) thresholds give hysteresis so a rep is never counted twice.

### Rep evaluation (lenient)
- Every inference with confidence ≥ 60 % made **during** the rep is a vote (`idle` does not vote).
- `correct` if correct votes ≥ **40 %** of the votes; otherwise whichever of `slow`/`fast` has more votes.
- No confident votes at all → judge by duration (< 1.5 s fast, > 9 s slow, else correct).
- All thresholds live in `leg_extension_coach/config.h`.

## Phases

- [x] **1. Setup** – folder structure, docs, libraries (LIS3DHTR, TFT_eSPI, Seeed SAMD core).
- [x] **2. Firmware for data collection** – `data_collection.ino` + `tools/serial_to_csv.py`.
- [ ] **3. Data collection** – ≥ 5 min per class, several people/weights (`DATA_COLLECTION.md`).
- [ ] **4. Model** – Edge Impulse web Studio: CSV upload, Spectral Analysis + NN, > 85 % test accuracy
      (`EDGE_IMPULSE_SETUP.md`).
- [x] **5. Coach firmware** – sampling, inference, rep state machine, screen & sound (`leg_extension_coach/`).
- [ ] **6. Integration** – install the exported library, compile, upload.
- [ ] **7. Tuning on the machine** – adjust angles, `CORRECT_VOTE_RATIO`, `MIN_CONFIDENCE` with real reps.
- [ ] **8. Demo** – video of correct / slow / fast reps and the counter.

## Test plan
1. **Angle**: move the arm by hand; the live screen angle goes ~0° at rest to 60–90° at the top.
2. **Counting**: 5 slow-but-full reps with the model ignored (watch the Serial log): 5 "rep finished" events, no
   doubles; half reps are ignored.
3. **Classification**: 5 correct, 5 slow, 5 fast reps → check screen color, text, sound and that only correct
   reps increase the counter. Target: ≥ 4/5 right per class.
4. **Buttons**: A resets, B recalibrates, C mutes.
5. **Robustness**: long pause at the top, sitting idle for 1 min (no false reps), different users.

## Risks & mitigations
| Risk | Mitigation |
|---|---|
| Device mounted differently than during recording | Photo of the mounting; same position always (see `PINOUT.md`). |
| `slow` and `correct` confused by the model | Clear tempo with a metronome while recording; lenient 40 % vote rule. |
| Different machines have different ranges of motion | Angle thresholds in `config.h`, live angle on screen to tune. |
| Screen redraws block sampling | Only small strips are redrawn live; full redraws only at rep end; sampler resyncs. |
| Not enough data | ≥ 30 takes per class, balanced, several people. |
