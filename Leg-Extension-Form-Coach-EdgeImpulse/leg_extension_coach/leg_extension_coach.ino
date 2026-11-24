/*************************************************************
  Final Project - Leg Extension Form Coach (Edge Impulse)

  The Wio Terminal is fixed to the moving arm (leg pad) of a
  leg-extension / quadriceps machine. While the person trains:
    - the built-in accelerometer is sampled continuously
    - an Edge Impulse model classifies the last window of motion
      as: idle / slow / correct / fast
    - the arm angle is used to detect each rep:
        rest -> going up -> stop at the top -> going down -> rest
    - at the end of every rep the model's votes decide the result:
        correct  -> GREEN screen, support words, coin "bling",
                    correct-rep counter + 1
        slow/fast-> RED screen, "TOO SLOW"/"TOO FAST", ugly buzz,
                    rep NOT counted

  Files in this sketch
    config.h       all tunable numbers (thresholds, timings, sounds)
    rep_counter.h  rep detection state machine + rep evaluation
    feedback.h     screen drawing + non-blocking buzzer melodies

  Hardware (all built into the Wio Terminal, no wiring needed)
    LIS3DHTR accelerometer -> internal I2C (Wire1, address 0x18)
    LCD 320x240            -> internal SPI (TFT_eSPI library)
    Buzzer                 -> WIO_BUZZER
    Buttons                -> WIO_KEY_A reset counters
                              WIO_KEY_B recalibrate rest position
                              WIO_KEY_C mute / unmute

  Libraries
    Seeed_Arduino_LIS3DHTR  (Library Manager: "Grove 3-Axis Digital Accelerometer LIS3DHTR")
    TFT_eSPI                (comes with the Seeed SAMD board package)
    Leg_Extension_Coach_inferencing (exported from Edge Impulse, see EDGE_IMPULSE_SETUP.md)
 *************************************************************/

// If your Edge Impulse project has another name, the exported header
// has another name too: change this line to match it.
#include <Leg_Extension_Coach_inferencing.h>

#include <LIS3DHTR.h>
#include "config.h"
#include "rep_counter.h"
#include "feedback.h"

// The model must be trained on 3 values per sample: accX, accY, accZ
#if EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3
#error "The Edge Impulse model must use 3 axes (accX, accY, accZ)"
#endif

LIS3DHTR<TwoWire> lis;

// ---------- Sampling ----------
// The sampling period comes from the model itself, so it always
// matches the frequency used in Edge Impulse.
const unsigned long SAMPLE_INTERVAL_US = (unsigned long)(EI_CLASSIFIER_INTERVAL_MS * 1000);
unsigned long nextSampleUs = 0;

// Ring buffer that always holds the most recent window of samples
// (x0,y0,z0, x1,y1,z1, ...). The model reads it every INFERENCE_EVERY_MS.
const int WINDOW_SIZE = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;   // samples * 3 axes
float ringBuffer[WINDOW_SIZE];
float features[WINDOW_SIZE];        // ring buffer "unrolled" in time order for the model
int   ringHead      = 0;            // where the next sample will be written
int   valuesStored  = 0;            // grows until the window is full

// ---------- Arm angle ----------
float filtered[3]   = {0, 0, 0};    // low-passed acceleration = direction of gravity
float restVector[3] = {0, 0, 1};    // gravity direction with the arm at rest (calibration)
float armAngle      = 0;            // degrees from the rest position
float armSpeed      = 0;            // degrees per second (filtered)
unsigned long lastAngleUs = 0;

// ---------- Machine learning ----------
unsigned long lastInferenceMs = 0;
MotionClass   liveClass = CLASS_UNKNOWN;   // latest prediction (for the screen)
float         liveScore = 0;

// ---------- Counters & screen ----------
int  correctReps = 0;
int  totalReps   = 0;
bool showingFeedback = false;       // green/red screen is on
unsigned long feedbackStartMs = 0;
unsigned long lastLiveRefreshMs = 0;

// ---------- Buttons ----------
// (declared up here, before any function: the Arduino IDE generates
//  function prototypes above the first function and needs this type)
struct Button
{
  int pin;
  bool lastState;                   // true = released (pull-up)
  unsigned long lastChangeMs;
};

Button btnReset = {WIO_KEY_A, true, 0};
Button btnCalib = {WIO_KEY_B, true, 0};
Button btnMute  = {WIO_KEY_C, true, 0};

// =============================================================
//  Accelerometer + angle
// =============================================================

// Read one sample in m/s^2 (same unit as the training data)
void readAccel(float &x, float &y, float &z)
{
  x = lis.getAccelerationX() * GRAVITY_MS2;
  y = lis.getAccelerationY() * GRAVITY_MS2;
  z = lis.getAccelerationZ() * GRAVITY_MS2;
}

// Angle between the current gravity direction and the rest one.
// Using the angle between two vectors means it works no matter
// how the Wio is rotated on the machine arm: only the calibration
// at rest matters.
float angleFromRest(const float v[3])
{
  float dot   = v[0] * restVector[0] + v[1] * restVector[1] + v[2] * restVector[2];
  float normV = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  float normR = sqrtf(restVector[0] * restVector[0] + restVector[1] * restVector[1] +
                      restVector[2] * restVector[2]);
  if (normV < 0.001f || normR < 0.001f) return 0;

  float c = dot / (normV * normR);
  c = constrain(c, -1.0f, 1.0f);    // rounding errors could make acos() fail
  return acosf(c) * 180.0f / PI;
}

// Record the rest position: average gravity for CALIBRATION_MS
// while the arm is down and still. (Blocking is fine here: nothing
// else has to run while calibrating.)
void calibrateRest()
{
  screenCalibrating();
  Serial.println("Calibrating rest position...");
  delay(500);                       // time to take the hand off the button

  float sum[3] = {0, 0, 0};
  int count = 0;
  unsigned long start = millis();
  while (millis() - start < CALIBRATION_MS)
  {
    float x, y, z;
    readAccel(x, y, z);
    sum[0] += x; sum[1] += y; sum[2] += z;
    count++;
    delay(10);
  }

  for (int i = 0; i < 3; i++)
  {
    restVector[i] = sum[i] / count;
    filtered[i]   = restVector[i];  // start the filter already at rest
  }
  armAngle = 0;
  armSpeed = 0;
  lastAngleUs = micros();
  repReset();

  Serial.printf("Rest vector: %.2f %.2f %.2f m/s^2\n",
                restVector[0], restVector[1], restVector[2]);
}

// =============================================================
//  Sampling: one sample -> ring buffer, angle, rep detection
// =============================================================

void handleRepFinished();

void takeSample()
{
  float x, y, z;
  readAccel(x, y, z);

  // 1) Save raw sample for the model
  ringBuffer[ringHead]     = x;
  ringBuffer[ringHead + 1] = y;
  ringBuffer[ringHead + 2] = z;
  ringHead = (ringHead + 3) % WINDOW_SIZE;
  if (valuesStored < WINDOW_SIZE) valuesStored += 3;

  // 2) Update the arm angle (filtered gravity direction)
  float raw[3] = {x, y, z};
  for (int i = 0; i < 3; i++)
    filtered[i] += ANGLE_FILTER_ALPHA * (raw[i] - filtered[i]);

  unsigned long nowUs = micros();
  float dt = (nowUs - lastAngleUs) / 1000000.0f;
  lastAngleUs = nowUs;

  float newAngle = angleFromRest(filtered);
  if (dt > 0)
  {
    float instantSpeed = (newAngle - armAngle) / dt;
    armSpeed += SPEED_FILTER_ALPHA * (instantSpeed - armSpeed);
  }
  armAngle = newAngle;

  // 3) Rep detection
  if (repUpdate(armAngle, armSpeed, millis()) == REP_FINISHED)
    handleRepFinished();
}

// =============================================================
//  Machine learning
// =============================================================

// Convert an Edge Impulse label to our enum
MotionClass labelToClass(const char *label)
{
  if (strcmp(label, "idle") == 0)    return CLASS_IDLE;
  if (strcmp(label, "slow") == 0)    return CLASS_SLOW;
  if (strcmp(label, "correct") == 0) return CLASS_CORRECT;
  if (strcmp(label, "fast") == 0)    return CLASS_FAST;
  return CLASS_UNKNOWN;
}

void runInference()
{
  // Copy the ring buffer in time order (oldest sample first).
  // When the buffer is full, the oldest sample is at ringHead.
  for (int i = 0; i < WINDOW_SIZE; i++)
    features[i] = ringBuffer[(ringHead + i) % WINDOW_SIZE];

  signal_t signal;
  if (numpy::signal_from_buffer(features, WINDOW_SIZE, &signal) != 0)
  {
    Serial.println("signal_from_buffer failed");
    return;
  }

  ei_impulse_result_t result;
  memset(&result, 0, sizeof(result));
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK)
  {
    Serial.printf("run_classifier error %d\n", err);
    return;
  }

  // Pick the class with the highest score
  int best = 0;
  for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    if (result.classification[i].value > result.classification[best].value) best = i;

  liveScore = result.classification[best].value;
  liveClass = (liveScore >= MIN_CONFIDENCE)
                ? labelToClass(result.classification[best].label)
                : CLASS_UNKNOWN;

  // Every confident prediction during a rep is a vote for its quality
  repAddVote(liveClass);

  // Debug line: angle, rep state and all scores
  Serial.printf("angle %5.1f  speed %6.1f  %-4s |", armAngle, armSpeed, stateName(repCurrentState()));
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    Serial.printf(" %s %.2f", result.classification[i].label, result.classification[i].value);
  Serial.printf("  (dsp %d ms, nn %d ms)\n", result.timing.dsp, result.timing.classification);
}

// =============================================================
//  Rep result -> counters, screen and sound
// =============================================================

void handleRepFinished()
{
  MotionClass res = repResult();
  totalReps++;

  if (res == CLASS_CORRECT)
  {
    correctReps++;
    screenCorrect(correctReps);
    soundCorrect();
  }
  else
  {
    screenWrong(res);
    soundWrong();
  }

  Serial.printf("REP %s  (%lu ms)  correct %d / total %d\n",
                className(res), repDurationMs(), correctReps, totalReps);

  showingFeedback = true;
  feedbackStartMs = millis();
}

// =============================================================
//  Buttons (active LOW, with simple debounce)
// =============================================================

// Returns true only once, at the moment the button is pressed
bool wasPressed(Button &b)
{
  bool state = digitalRead(b.pin);
  if (state != b.lastState && millis() - b.lastChangeMs > DEBOUNCE_MS)
  {
    b.lastState = state;
    b.lastChangeMs = millis();
    return state == LOW;
  }
  return false;
}

void handleButtons()
{
  if (wasPressed(btnReset))
  {
    correctReps = 0;
    totalReps   = 0;
    repReset();
    soundClick();
    showingFeedback = false;
    screenLive(correctReps, totalReps);
    screenToast("Counters reset");
    lastLiveRefreshMs = millis() + 1000;   // keep the message visible ~1 s
    Serial.println("Counters reset");
  }

  if (wasPressed(btnCalib))
  {
    soundClick();
    calibrateRest();
    showingFeedback = false;
    screenLive(correctReps, totalReps);
    nextSampleUs = micros();
  }

  if (wasPressed(btnMute))
  {
    bool m = toggleMute();
    if (!m) soundClick();
    if (!showingFeedback)
    {
      screenToast(m ? "Sound OFF" : "Sound ON");
      lastLiveRefreshMs = millis() + 1000;
    }
    Serial.println(m ? "Muted" : "Sound on");
  }
}

// =============================================================
//  Setup & loop
// =============================================================

void setup()
{
  Serial.begin(115200);
  pinMode(WIO_BUZZER, OUTPUT);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  screenBegin();

  // Built-in accelerometer is on the internal I2C bus (Wire1)
  lis.begin(Wire1);
  if (!lis.isConnection())
  {
    Serial.println("LIS3DHTR not found!");
    tft.fillScreen(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("Accelerometer not found", 160, 120);
    while (true) delay(1000);
  }
  lis.setOutputDataRate(LIS3DHTR_DATARATE_100HZ);
  lis.setFullScaleRange(LIS3DHTR_RANGE_4G);   // same range as data_collection.ino

  randomSeed(analogRead(WIO_LIGHT) + micros());   // different support words every session

  Serial.printf("Model: window %d samples, %.1f ms interval, %d labels\n",
                EI_CLASSIFIER_RAW_SAMPLE_COUNT, (float)EI_CLASSIFIER_INTERVAL_MS,
                EI_CLASSIFIER_LABEL_COUNT);

  calibrateRest();
  screenLive(correctReps, totalReps);
  nextSampleUs = micros();
}

void loop()
{
  unsigned long nowUs = micros();
  unsigned long nowMs = millis();

  soundUpdate();
  handleButtons();

  // 1) Fixed-rate sampling. If something slow (e.g. a full screen
  //    redraw) made us late by more than one period, resync instead
  //    of taking a burst of samples at the same instant.
  if ((long)(nowUs - nextSampleUs) >= 0)
  {
    takeSample();
    nextSampleUs += SAMPLE_INTERVAL_US;
    if ((long)(micros() - nextSampleUs) > (long)SAMPLE_INTERVAL_US)
      nextSampleUs = micros() + SAMPLE_INTERVAL_US;
  }

  // 2) Classify the last window every INFERENCE_EVERY_MS (once it is full)
  if (valuesStored >= WINDOW_SIZE && nowMs - lastInferenceMs >= INFERENCE_EVERY_MS)
  {
    lastInferenceMs = nowMs;
    runInference();
  }

  // 3) After the green/red screen, go back to the live screen
  if (showingFeedback && nowMs - feedbackStartMs >= FEEDBACK_MS)
  {
    showingFeedback = false;
    screenLive(correctReps, totalReps);
  }

  // 4) Refresh the live info strip
  if (!showingFeedback && (long)(nowMs - lastLiveRefreshMs) >= LIVE_REFRESH_MS)
  {
    lastLiveRefreshMs = nowMs;
    screenLiveInfo(repCurrentState(), liveClass, liveScore, armAngle);
  }
}
