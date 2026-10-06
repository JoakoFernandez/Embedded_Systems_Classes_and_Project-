#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  pinMode(WIO_KEY_C, INPUT_PULLUP);

  tft.drawString("Light:", 160, 10);
}

void loop() {
  //Light lecture
  int sensorValue = analogRead(WIO_LIGHT);

  //Button sensor display
  if (digitalRead(WIO_KEY_C) == LOW) {
    tft.drawString("Button C pressed", 10, 10);
  }
  else {
    tft.fillRect(0, 0, 150, 20, TFT_BLACK);

  }
  // Erase only the previous number
  tft.fillRect(200, 10, 50, 20, TFT_BLACK);

  // Draw only the new number
  tft.drawString(String(sensorValue), 200, 10);

  //Circle Coloring
   int color;
  if (sensorValue < 150) {
    color = TFT_BLUE;       // very low
  } 
  else if (sensorValue < 300) {
    color = TFT_GREEN;      // low
  } 
  else if (sensorValue < 450) {
    color = TFT_YELLOW;     // low medium
  } 
  else if (sensorValue < 600) {
    color = TFT_ORANGE;     // medium
  } 
  else if (sensorValue < 750) {
    color = TFT_PINK;       // high
  } 
  else {
    color = TFT_RED;        // very high
  }

  //Draw circle
  tft.fillCircle(160, 120, 70, color);
  
  //Delay for smoothness
  delay(150);
}