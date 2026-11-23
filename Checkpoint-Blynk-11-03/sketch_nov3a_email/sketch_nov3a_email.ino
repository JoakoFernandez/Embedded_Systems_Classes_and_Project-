/*************************************************************
  Checkpoint 11-03: random number (V5) + DHT20 (V6 temp, V7 humidity)
  + email when temperature goes above TEMP_ALERT_C

  Same as sketch_nov3a, with two changes:
   1. Credentials moved to arduino_secrets.h (keep it out of GitHub)
   2. Blynk.logEvent("high_temp", ...) sends the email
 *************************************************************/

#include "arduino_secrets.h"   // BLYNK_TEMPLATE_ID / NAME / AUTH_TOKEN, SECRET_SSID / PASS

#define BLYNK_PRINT Serial

#include <rpcWiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleWioTerminal.h>

#include <Wire.h>
#include <DHT.h>

#define DHTTYPE DHT20
DHT dht(DHTTYPE);

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

#define TEMP_ALERT_C 28.0      // email when temperature rises above this
bool alertSent = false;        // one email per crossing, not one every 2 s

BlynkTimer timer;

// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0)
{
  int value = param.asInt();
  Blynk.virtualWrite(V1, value);
}

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
  Blynk.setProperty(V3, "offImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations.png");
  Blynk.setProperty(V3, "onImageUrl",  "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations_pressed.png");
  Blynk.setProperty(V3, "url", "https://docs.blynk.io/en/getting-started/what-do-i-need-to-blynk/how-quickstart-device-was-made");
}

void myTimerEvent()
{
  // Random number demonstration
  int randomNumber = random(0, 1024);
  Blynk.virtualWrite(V5, randomNumber);

  // Read the DHT20 sensor
  float values[2];

  if (dht.readTempAndHumidity(values) == 0)
  {
    float humidity = values[0];
    float temperature = values[1];

    Blynk.virtualWrite(V6, temperature);
    Blynk.virtualWrite(V7, humidity);

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // ---- NEW: send the email event when the condition is met ----
    if (temperature > TEMP_ALERT_C && !alertSent)
    {
      Blynk.logEvent("high_temp", String("Temperature is ") + temperature + " C");
      alertSent = true;
      Serial.println("Event high_temp sent -> check your email");
    }
    else if (temperature < TEMP_ALERT_C - 1)   // re-arm after it cools down
    {
      alertSent = false;
    }
  }
  else
  {
    Serial.println("Failed to read DHT20!");
  }
}

void setup()
{
  Serial.begin(115200);

  Wire.begin();
  dht.begin();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  randomSeed(micros());

  timer.setInterval(2000L, myTimerEvent);
}

void loop()
{
  Blynk.run();
  timer.run();
}
