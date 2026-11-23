/*************************************************************
  HW #3 - Blynk dashboard (builds on sketch_nov3a, same template)

  READ (Wio -> Blynk)
    V6   DHT20 temperature  -> Chart widget          (same pin as the checkpoint)
    V7   DHT20 humidity     -> Gauge or Label widget (same pin as the checkpoint)
    V8   ultrasonic distance in cm -> Label (optional, handy for testing)
    V9   1 when closer than DISTANCE_THRESHOLD_CM, else 0 -> LED widget

  CONTROL (Blynk -> Wio)
    V10  Slider 0-255 -> LED brightness
    V11  Switch 0/1   -> buzzer, new random pitch 500-1500 Hz each time it turns on

  Wiring
    DHT20       -> left Grove port (I2C)
    Ultrasonic  -> right Grove port (D0)
    LED         -> pin LED_PIN (must support analogWrite) + ~220 ohm resistor to GND
    Buzzer      -> built-in (WIO_BUZZER)
 *************************************************************/

#include "arduino_secrets.h"

#define BLYNK_PRINT Serial

#include <rpcWiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleWioTerminal.h>
#include <Wire.h>
#include <DHT.h>
#include "Ultrasonic.h"        // library: Grove Ultrasonic Ranger (Seeed)

#define DHTTYPE DHT20
DHT dht(DHTTYPE);

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

#define ULTRASONIC_PIN         D0
#define LED_PIN                D2     // change to the pin your LED is on
#define DISTANCE_THRESHOLD_CM  20

Ultrasonic ultrasonic(ULTRASONIC_PIN);
BlynkTimer timer;

// ---------- 1. READ ----------
void sendSensorData()
{
  float values[2];
  if (dht.readTempAndHumidity(values) == 0)
  {
    float humidity = values[0];
    float temperature = values[1];
    Blynk.virtualWrite(V6, temperature);
    Blynk.virtualWrite(V7, humidity);
    Serial.printf("Temp %.1f C  Hum %.1f %%\n", temperature, humidity);
  }
  else
  {
    Serial.println("Failed to read DHT20!");
  }

  long distance = ultrasonic.MeasureInCentimeters();
  bool close = distance > 0 && distance < DISTANCE_THRESHOLD_CM;
  Blynk.virtualWrite(V8, distance);
  Blynk.virtualWrite(V9, close ? 1 : 0);
  Serial.printf("Distance %ld cm  alert %d\n", distance, close);
}

// ---------- 2. CONTROL ----------
BLYNK_WRITE(V10)                      // slider
{
  int level = param.asInt();          // 0-255
  analogWrite(LED_PIN, level);
  Serial.printf("LED level %d\n", level);
}

BLYNK_WRITE(V11)                      // switch
{
  if (param.asInt() == 1)
  {
    int pitch = random(500, 1501);    // new random pitch on every trigger
    tone(WIO_BUZZER, pitch);
    Serial.printf("Buzzer ON  %d Hz\n", pitch);
  }
  else
  {
    noTone(WIO_BUZZER);
    Serial.println("Buzzer OFF");
  }
}

BLYNK_CONNECTED()
{
  Blynk.syncVirtual(V10, V11);        // restore slider/switch state after reconnect
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIO_BUZZER, OUTPUT);

  Wire.begin();
  dht.begin();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  randomSeed(micros());

  timer.setInterval(2000L, sendSensorData);
}

void loop()
{
  Blynk.run();
  timer.run();
}
