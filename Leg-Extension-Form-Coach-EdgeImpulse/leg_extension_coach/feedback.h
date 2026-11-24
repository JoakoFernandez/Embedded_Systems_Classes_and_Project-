/*************************************************************
  feedback.h - what the user sees and hears

  SCREEN (320 x 240, rotation 3)
    Live screen   : dark, big number of correct reps + live info
    Correct rep   : GREEN, "REP N" + a random support phrase
    Wrong rep     : RED, "TOO SLOW" / "TOO FAST" + a hint

  SOUND (built-in buzzer)
    Correct rep   : short coin-pickup style "bling"
    Wrong rep     : low, dissonant, uncomfortable buzz

  The sound player is NON-BLOCKING: it never uses delay().
  soundUpdate() is called from loop() and switches notes when
  their time is over, so the accelerometer keeps being sampled
  at 100 Hz while a sound is playing.
 *************************************************************/

#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "rep_counter.h"

TFT_eSPI tft;

// =============================================================
//  SOUND
// =============================================================

struct Note
{
  uint16_t freq;         // Hz, 0 = silence
  uint16_t ms;           // duration
};

#define MAX_NOTES 16

static Note          melody[MAX_NOTES];
static int           melodyLength = 0;
static int           melodyIndex  = -1;     // -1 = nothing playing
static unsigned long noteStartMs  = 0;
static bool          muted        = false;

// Start playing the note at melodyIndex
static void startCurrentNote()
{
  noteStartMs = millis();
  if (melody[melodyIndex].freq == 0) noTone(WIO_BUZZER);
  else                               tone(WIO_BUZZER, melody[melodyIndex].freq);
}

// Replace whatever is playing with a new melody
static void playMelody(const Note *notes, int count)
{
  if (muted) return;
  melodyLength = min(count, MAX_NOTES);
  for (int i = 0; i < melodyLength; i++) melody[i] = notes[i];
  melodyIndex = 0;
  startCurrentNote();
}

// Call on every loop(): moves to the next note when the current one ends
void soundUpdate()
{
  if (melodyIndex < 0) return;
  if (millis() - noteStartMs < melody[melodyIndex].ms) return;

  melodyIndex++;
  if (melodyIndex >= melodyLength)
  {
    noTone(WIO_BUZZER);
    melodyIndex = -1;
  }
  else
  {
    startCurrentNote();
  }
}

void soundCorrect()
{
  // Quick note followed by a higher, longer one: a bright "bling"
  const Note coin[] = {
    {COIN_NOTE_1_HZ, COIN_NOTE_1_MS},
    {COIN_NOTE_2_HZ, COIN_NOTE_2_MS},
  };
  playMelody(coin, 2);
}

void soundWrong()
{
  // Two low pitches very close to each other alternate fast:
  // the "beating" between them is what makes it unpleasant.
  Note buzz[BAD_NOTE_REPEATS];
  for (int i = 0; i < BAD_NOTE_REPEATS; i++)
  {
    buzz[i].freq = (i % 2 == 0) ? BAD_NOTE_A_HZ : BAD_NOTE_B_HZ;
    buzz[i].ms   = BAD_NOTE_MS;
  }
  playMelody(buzz, BAD_NOTE_REPEATS);
}

void soundClick()                    // short beep for button presses
{
  const Note click[] = {{2000, 30}};
  playMelody(click, 1);
}

bool toggleMute()
{
  muted = !muted;
  if (muted) { noTone(WIO_BUZZER); melodyIndex = -1; }
  return muted;
}

// =============================================================
//  SCREEN
// =============================================================

// Support phrases shown on a correct rep (one is picked at random)
const char *SUPPORT_WORDS[] = {
  "Great job!",
  "Perfect form!",
  "Keep it up!",
  "Strong!",
  "Nice and controlled!",
  "You got this!",
  "Excellent rep!",
};
const int SUPPORT_COUNT = sizeof(SUPPORT_WORDS) / sizeof(SUPPORT_WORDS[0]);

void screenBegin()
{
  tft.begin();
  tft.setRotation(3);                // same orientation as the other class sketches
  tft.fillScreen(TFT_BLACK);
}

// Big centered text helper
static void centerText(const char *text, int y, int size, uint16_t color, uint16_t bg)
{
  tft.setTextDatum(MC_DATUM);        // x,y is the middle of the text
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  tft.drawString(text, 160, y);
}

// Shown while calibrating the rest position
void screenCalibrating()
{
  tft.fillScreen(TFT_NAVY);
  centerText("CALIBRATING", 70, 3, TFT_WHITE, TFT_NAVY);
  centerText("Leave the machine arm", 130, 2, TFT_WHITE, TFT_NAVY);
  centerText("at REST and keep still", 160, 2, TFT_WHITE, TFT_NAVY);
}

// Full redraw of the live (normal) screen: title, counters, button help
void screenLive(int correctReps, int totalReps)
{
  tft.fillScreen(TFT_BLACK);

  // Title bar
  tft.fillRect(0, 0, 320, 28, TFT_DARKGREEN);
  centerText("LEG EXTENSION COACH", 14, 2, TFT_WHITE, TFT_DARKGREEN);

  // Big counter of correct reps
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", correctReps);
  centerText("CORRECT REPS", 50, 2, TFT_LIGHTGREY, TFT_BLACK);
  centerText(buf, 105, 7, TFT_GREEN, TFT_BLACK);

  snprintf(buf, sizeof(buf), "Attempts: %d", totalReps);
  centerText(buf, 160, 2, TFT_LIGHTGREY, TFT_BLACK);

  // Button help (the 3 top buttons, left to right = C, B, A)
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("C: mute   B: recalibrate   A: reset", 50, 228);
}

// Small update of the live info line (state, model output, angle).
// Only this strip is redrawn so the screen does not flicker.
void screenLiveInfo(RepState state, MotionClass live, float score, float angle)
{
  char buf[48];
  snprintf(buf, sizeof(buf), "%-4s %-7s %3d%% %3ddeg",
           stateName(state), className(live), (int)(score * 100), (int)angle);

  tft.fillRect(0, 185, 320, 30, TFT_BLACK);
  centerText(buf, 200, 2, TFT_YELLOW, TFT_BLACK);
}

// Green screen for a correct rep
void screenCorrect(int correctReps)
{
  tft.fillScreen(TFT_GREEN);

  char buf[16];
  snprintf(buf, sizeof(buf), "REP %d", correctReps);
  centerText(buf, 80, 6, TFT_BLACK, TFT_GREEN);

  // Long phrases use a smaller font so they fit in 320 px (size 3 = 18 px per char)
  const char *word = SUPPORT_WORDS[random(SUPPORT_COUNT)];
  int size = (strlen(word) * 18 > 300) ? 2 : 3;
  centerText(word, 160, size, TFT_BLACK, TFT_GREEN);
}

// Red screen for a wrong rep (too slow or too fast)
void screenWrong(MotionClass result)
{
  tft.fillScreen(TFT_RED);

  if (result == CLASS_SLOW)
  {
    centerText("TOO SLOW", 90, 5, TFT_WHITE, TFT_RED);
    centerText("Speed up a bit", 160, 3, TFT_WHITE, TFT_RED);
  }
  else
  {
    centerText("TOO FAST", 90, 5, TFT_WHITE, TFT_RED);
    centerText("Control the movement", 160, 2, TFT_WHITE, TFT_RED);
  }
  centerText("Rep not counted", 205, 2, TFT_WHITE, TFT_RED);
}

// Short message on the bottom strip (e.g. "Counters reset")
void screenToast(const char *msg)
{
  tft.fillRect(0, 185, 320, 30, TFT_BLACK);
  centerText(msg, 200, 2, TFT_CYAN, TFT_BLACK);
}
