/*************************************************************
  rep_counter.h - detects reps from the arm angle and decides
                  if each rep was correct, too slow or too fast

  One rep of the leg extension, as the arm angle sees it:

      angle
        ^          TOP (short stop)
        |         _______
        |        /       \
        |       /         \          MOVING_UP   : leg going up
        |      /           \         TOP         : stop at the top
        |_____/             \______  MOVING_DOWN : leg going down
        +----------------------------> time
         REST  UP    TOP   DOWN REST

  Counting is done with the angle (simple and reliable).
  The QUALITY of the rep is decided by the Edge Impulse model:
  every confident prediction made while the rep is going on is a
  "vote" (slow / correct / fast). When the rep ends, the votes
  decide the result. It is lenient: 40 % correct votes is enough.
 *************************************************************/

#pragma once
#include <Arduino.h>
#include "config.h"

// Classes the model can output (names must match the Edge Impulse labels)
enum MotionClass
{
  CLASS_IDLE,
  CLASS_SLOW,
  CLASS_CORRECT,
  CLASS_FAST,
  CLASS_UNKNOWN          // prediction below MIN_CONFIDENCE or unknown label
};

// Steps of one rep
enum RepState
{
  STATE_REST,
  STATE_MOVING_UP,
  STATE_TOP,
  STATE_MOVING_DOWN
};

// What happened in the last call to repUpdate()
enum RepEvent
{
  REP_NONE,              // nothing finished
  REP_FINISHED           // a full rep just ended, result in repResult()
};

// ---------- Internal state of the counter ----------
static RepState      repState       = STATE_REST;
static unsigned long repStartMs     = 0;      // when the arm left the rest position
static unsigned long holdStartMs    = 0;      // when the arm started being still near the top
static bool          holding        = false;
static bool          reachedTop     = false;  // full range of motion reached in this rep
static float         peakAngle      = 0;      // highest angle in this rep
static int           votes[3]       = {0, 0, 0};   // slow, correct, fast
static MotionClass   lastResult     = CLASS_UNKNOWN;
static unsigned long lastDurationMs = 0;

// Human readable names (for the screen and the Serial monitor)
const char *stateName(RepState s)
{
  switch (s)
  {
    case STATE_REST:        return "REST";
    case STATE_MOVING_UP:   return "UP";
    case STATE_TOP:         return "TOP";
    case STATE_MOVING_DOWN: return "DOWN";
  }
  return "?";
}

const char *className(MotionClass c)
{
  switch (c)
  {
    case CLASS_IDLE:    return "idle";
    case CLASS_SLOW:    return "slow";
    case CLASS_CORRECT: return "correct";
    case CLASS_FAST:    return "fast";
    default:            return "-";
  }
}

RepState    repCurrentState() { return repState; }
MotionClass repResult()       { return lastResult; }
unsigned long repDurationMs() { return lastDurationMs; }

// Forget the rep in progress (used on reset / recalibration / timeout)
void repReset()
{
  repState   = STATE_REST;
  holding    = false;
  reachedTop = false;
  peakAngle  = 0;
  votes[0] = votes[1] = votes[2] = 0;
}

// Called by the main sketch after each inference.
// Votes only count while a rep is going on; "idle" is not a vote.
void repAddVote(MotionClass c)
{
  if (repState == STATE_REST) return;
  if (c == CLASS_SLOW)    votes[0]++;
  if (c == CLASS_CORRECT) votes[1]++;
  if (c == CLASS_FAST)    votes[2]++;
}

// Decide the result of the rep that just ended
static MotionClass evaluateRep(unsigned long durationMs)
{
  int slow = votes[0], correct = votes[1], fast = votes[2];
  int total = slow + correct + fast;

  Serial.printf("Rep votes  slow %d  correct %d  fast %d  (%lu ms)\n",
                slow, correct, fast, durationMs);

  if (total > 0)
  {
    // Lenient rule: enough "correct" votes wins, even if not the majority
    if ((float)correct / total >= CORRECT_VOTE_RATIO) return CLASS_CORRECT;
    return (slow >= fast) ? CLASS_SLOW : CLASS_FAST;
  }

  // No confident vote: fall back to the duration of the rep
  if (durationMs < FAST_REP_MS) return CLASS_FAST;
  if (durationMs > SLOW_REP_MS) return CLASS_SLOW;
  return CLASS_CORRECT;
}

// Feed the latest arm angle (deg, 0 = rest) and angular speed (deg/s).
// Call it on every new sample. Returns REP_FINISHED once per full rep.
RepEvent repUpdate(float angle, float speed, unsigned long now)
{
  // Safety: a rep that never ends is dropped (e.g. user left the arm up)
  if (repState != STATE_REST && now - repStartMs > REP_TIMEOUT_MS)
  {
    Serial.println("Rep timeout, ignored");
    repReset();
    return REP_NONE;
  }

  if (angle > peakAngle) peakAngle = angle;

  switch (repState)
  {
    case STATE_REST:
      if (angle > START_DELTA_DEG)
      {
        repReset();                      // clean votes for the new rep
        repState   = STATE_MOVING_UP;
        repStartMs = now;
        peakAngle  = angle;
      }
      break;

    case STATE_MOVING_UP:
      if (angle > TOP_DELTA_DEG) reachedTop = true;

      if (reachedTop)
      {
        // Is the arm (almost) still? Then it is the stop at the top.
        if (fabsf(speed) < HOLD_SPEED_DEG_S)
        {
          if (!holding) { holding = true; holdStartMs = now; }
          if (now - holdStartMs >= HOLD_MIN_MS) repState = STATE_TOP;
        }
        else
        {
          holding = false;
        }

        // Lenient: if the user goes down without a clear stop,
        // the rep still counts (the model judges the tempo anyway).
        if (angle < peakAngle - DOWN_DELTA_DEG) repState = STATE_MOVING_DOWN;
      }
      else if (angle < END_DELTA_DEG)
      {
        // Came back down without reaching the top: half rep, not counted
        Serial.println("Partial rep (top not reached), ignored");
        repReset();
      }
      break;

    case STATE_TOP:
      if (angle < peakAngle - DOWN_DELTA_DEG) repState = STATE_MOVING_DOWN;
      break;

    case STATE_MOVING_DOWN:
      if (angle < END_DELTA_DEG)
      {
        // Back at rest: the rep is complete
        lastDurationMs = now - repStartMs;
        lastResult     = evaluateRep(lastDurationMs);
        repReset();
        return REP_FINISHED;
      }
      break;
  }
  return REP_NONE;
}
