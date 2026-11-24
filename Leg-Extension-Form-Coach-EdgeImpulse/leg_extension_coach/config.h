/*************************************************************
  config.h - every tunable number of the Leg Extension Coach

  Change values here instead of hunting through the code.
  Angles are in degrees, measured from the REST position that is
  captured during calibration (0 deg = machine arm down, at rest).
 *************************************************************/

#pragma once

// ---------- Sampling ----------
// The sampling rate is NOT set here: it is taken from the Edge Impulse
// model (EI_CLASSIFIER_INTERVAL_MS), so it always matches the 100 Hz
// used by data_collection.ino when the training data was recorded.
#define GRAVITY_MS2           9.80665f   // Edge Impulse data is in m/s^2, sensor gives g

// ---------- Machine learning ----------
#define INFERENCE_EVERY_MS    250        // sliding window step (window length comes from the model)
#define MIN_CONFIDENCE        0.60f      // ignore predictions below this score

// ---------- Arm angle filter ----------
// Low-pass filter on the acceleration so the angle follows gravity
// (the arm position) and not the small shakes of the movement.
// 0.0 = never changes, 1.0 = no filtering.
#define ANGLE_FILTER_ALPHA    0.15f
#define SPEED_FILTER_ALPHA    0.20f      // smoothing for the angular speed
#define CALIBRATION_MS        2000       // time the arm must stay at rest while calibrating

// ---------- Rep detection (state machine in rep_counter.h) ----------
#define START_DELTA_DEG       15.0f      // arm above this -> the rep has started
#define TOP_DELTA_DEG         45.0f      // arm above this -> full range of motion reached
#define DOWN_DELTA_DEG        10.0f      // drop from the highest point -> going down
#define END_DELTA_DEG         10.0f      // arm below this -> back at rest, rep finished
#define HOLD_SPEED_DEG_S      25.0f      // slower than this at the top = "stopped at the top"
#define HOLD_MIN_MS           200        // how long the stop at the top must last
#define REP_TIMEOUT_MS        20000      // give up on a rep that takes longer than this

// ---------- Rep evaluation (lenient on purpose) ----------
// A rep is "correct" if at least this share of the ML votes during
// the rep said "correct". 0.40 = 40 %, so a few wrong windows are fine.
#define CORRECT_VOTE_RATIO    0.40f
// Fallback when the model gave no confident vote during the rep:
// judge only by the total duration of the rep.
#define FAST_REP_MS           1500       // shorter than this -> too fast
#define SLOW_REP_MS           9000       // longer than this  -> too slow

// ---------- Feedback ----------
#define FEEDBACK_MS           1500       // how long the green/red screen stays
#define LIVE_REFRESH_MS       200        // how often the live info on screen is updated

// Correct rep: short coin-pickup style "bling" (quick note, then a higher, longer one)
#define COIN_NOTE_1_HZ        1320
#define COIN_NOTE_1_MS        60
#define COIN_NOTE_2_HZ        1760
#define COIN_NOTE_2_MS        300

// Wrong rep: low, dissonant buzz alternating between two close pitches
#define BAD_NOTE_A_HZ         180
#define BAD_NOTE_B_HZ         220
#define BAD_NOTE_MS           100        // length of each piece of the buzz
#define BAD_NOTE_REPEATS      7          // number of pieces (7 x 100 ms = 0.7 s)

// ---------- Buttons ----------
#define DEBOUNCE_MS           50
