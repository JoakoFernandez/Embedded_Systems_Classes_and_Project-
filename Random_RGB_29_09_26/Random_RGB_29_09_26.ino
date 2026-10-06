/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://docs.arduino.cc/hardware/

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  https://docs.arduino.cc/built-in-examples/basics/Blink/
*/
const int PIN_R = D8;   // physical 37, PWM4
const int PIN_G = D1;   // physical 15, PWM1
const int PIN_B = D6;   // physical 33, PWM3

void setColor(int r, int g, int b) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  analogWrite(PIN_R, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_B, b);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  randomSeed(analogRead(A0));   // floating pin gives a different seed each boot
}

void loop() {
  int r, g, b;
    r = random(0, 256);
    g = random(0, 256);
    b = random(0, 256);
  
  
  setColor(r, g, b);

  Serial.print("RGB: ");
  Serial.print(r); Serial.print(", ");
  Serial.print(g); Serial.print(", ");
  Serial.println(b);

  delay(1000);
}