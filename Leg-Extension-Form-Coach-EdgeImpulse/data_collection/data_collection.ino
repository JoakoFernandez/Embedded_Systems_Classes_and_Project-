/*************************************************************
  Final Project - Leg Extension Form Coach
  DATA COLLECTION sketch (for the Edge Impulse web Studio)

  Records labelled "takes" of the built-in accelerometer at 100 Hz
  and prints each one on the Serial port as a CSV file that can be
  uploaded directly in Edge Impulse Studio:
      Data acquisition -> Upload data

  How to use
    1. Fix the Wio Terminal to the machine arm (same place and
       orientation you will use later, see PINOUT.md).
    2. Open the Serial Monitor (115200 baud) or run
       tools/serial_to_csv.py to save the takes automatically.
    3. 5-way switch LEFT / RIGHT -> choose the label
         idle | slow | correct | fast
    4. Press KEY_C (left top button) -> 3 beeps countdown ->
       recording for RECORD_SECONDS, do the reps at that tempo.
    5. A long beep marks the end. The CSV is printed between
         --- BEGIN <label>.<session>.<n>.csv ---
         --- END ---
       Copy it into a file with that name (or let the script do it).
       Edge Impulse reads the label from the text before the first dot.

  CSV format
    timestamp,accX,accY,accZ     (ms, m/s^2, m/s^2, m/s^2)

  Hardware: everything built in (LIS3DHTR on Wire1, LCD, buzzer,
  5-way switch, KEY_C). Library: Seeed_Arduino_LIS3DHTR.
 *************************************************************/

#include <LIS3DHTR.h>
#include <TFT_eSPI.h>

#define SAMPLE_RATE_HZ   100                       // must be the same in the whole project
#define SAMPLE_PERIOD_US (1000000UL / SAMPLE_RATE_HZ)
#define RECORD_SECONDS   10                        // length of one take
#define GRAVITY_MS2      9.80665f                  // sensor gives g, Edge Impulse wants m/s^2

LIS3DHTR<TwoWire> lis;
TFT_eSPI tft;

// Labels must be EXACTLY the same names used in Edge Impulse
// and in leg_extension_coach.ino (labelToClass()).
const char *LABELS[] = {"idle", "slow", "correct", "fast"};

// Tempo reminder shown under each label (see EDGE_IMPULSE_SETUP.md)
const char *HINTS[] = {
  "Sit still / small moves",
  "4s up, hold, 4s down",
  "2s up, 1s hold, 2-3s down",
  "<1s up, no hold, <1s down",
};
const int LABEL_COUNT = 4;

int labelIndex = 2;                 // start on "correct"
int takes[LABEL_COUNT] = {0, 0, 0, 0};
char sessionId[5];                  // random tag so files from different sessions never clash

// ---------- Screen ----------

void drawMenu()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("DATA COLLECTION", 160, 20);

  // Selected label, big, with arrows to show it can be changed
  tft.setTextSize(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(String("< ") + LABELS[labelIndex] + " >", 160, 85);

  tft.setTextSize(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(HINTS[labelIndex], 160, 130);
  tft.drawString(String("Takes: ") + takes[labelIndex], 160, 165);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("5-way LEFT/RIGHT: label    KEY C: record", 160, 225);
}

void drawStatus(const char *text, uint16_t bg)
{
  tft.fillScreen(bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(text, 160, 100);
  tft.setTextSize(2);
  tft.drawString(LABELS[labelIndex], 160, 160);
}

// ---------- Recording ----------

void recordTake()
{
  takes[labelIndex]++;

  // Countdown: 3 short beeps, one per second, so the user can get ready
  for (int i = 3; i > 0; i--)
  {
    drawStatus(String(i).c_str(), TFT_NAVY);
    tone(WIO_BUZZER, 1000, 100);
    delay(1000);
  }
  drawStatus("RECORDING", TFT_RED);
  tone(WIO_BUZZER, 2000, 200);

  // File header. The script / user saves everything between the markers.
  Serial.printf("--- BEGIN %s.%s.%02d.csv ---\n", LABELS[labelIndex], sessionId, takes[labelIndex]);
  Serial.println("timestamp,accX,accY,accZ");

  const int totalSamples = RECORD_SECONDS * SAMPLE_RATE_HZ;
  unsigned long nextUs = micros();

  for (int i = 0; i < totalSamples; i++)
  {
    // Wait for the exact sample time so the rate is a steady 100 Hz
    while ((long)(micros() - nextUs) < 0) { }
    nextUs += SAMPLE_PERIOD_US;

    float x = lis.getAccelerationX() * GRAVITY_MS2;
    float y = lis.getAccelerationY() * GRAVITY_MS2;
    float z = lis.getAccelerationZ() * GRAVITY_MS2;

    // Ideal timestamp (i * 10 ms): the samples are taken at that rate
    Serial.printf("%lu,%.4f,%.4f,%.4f\n", (unsigned long)i * 1000UL / SAMPLE_RATE_HZ, x, y, z);

    // Progress bar once per second (small drawing, does not disturb timing)
    if (i % SAMPLE_RATE_HZ == 0)
    {
      int w = map(i, 0, totalSamples, 0, 280);
      tft.fillRect(20, 200, w, 15, TFT_WHITE);
    }
  }

  Serial.println("--- END ---");
  tone(WIO_BUZZER, 600, 500);       // long low beep = finished
  delay(600);
  drawMenu();
}

// ---------- Setup & loop ----------

void setup()
{
  Serial.begin(115200);
  pinMode(WIO_BUZZER, OUTPUT);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(3);

  // Built-in accelerometer: internal I2C bus Wire1
  lis.begin(Wire1);
  if (!lis.isConnection())
  {
    Serial.println("LIS3DHTR not found!");
    drawStatus("NO SENSOR", TFT_RED);
    while (true) delay(1000);
  }
  lis.setOutputDataRate(LIS3DHTR_DATARATE_100HZ);
  lis.setFullScaleRange(LIS3DHTR_RANGE_4G);   // +-4 g is plenty for this exercise

  randomSeed(analogRead(WIO_LIGHT) + micros());
  snprintf(sessionId, sizeof(sessionId), "%04lx", random(0x10000));

  Serial.println("Data collection ready. Choose a label and press KEY C.");
  drawMenu();
}

void loop()
{
  if (digitalRead(WIO_5S_LEFT) == LOW)
  {
    labelIndex = (labelIndex + LABEL_COUNT - 1) % LABEL_COUNT;
    drawMenu();
    delay(250);                     // simple debounce, nothing else runs here
  }
  if (digitalRead(WIO_5S_RIGHT) == LOW)
  {
    labelIndex = (labelIndex + 1) % LABEL_COUNT;
    drawMenu();
    delay(250);
  }
  if (digitalRead(WIO_KEY_C) == LOW)
  {
    recordTake();
  }
}
