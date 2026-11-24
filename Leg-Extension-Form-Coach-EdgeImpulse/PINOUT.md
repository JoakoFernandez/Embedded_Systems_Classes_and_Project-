# Pinout & Mounting – Leg Extension Form Coach

The project uses **only hardware built into the Wio Terminal**, so there is no external wiring.

## Resources used

| Function | Wio Terminal resource | Name in code | Notes |
|---|---|---|---|
| Accelerometer | LIS3DHTR, internal I2C bus | `Wire1`, address `0x18` | 100 Hz output data rate, ±4 g range (same in both sketches) |
| Display | 2.4" LCD 320×240, internal SPI | `TFT_eSPI tft` | `setRotation(3)` like the other class sketches |
| Buzzer | Built-in passive buzzer | `WIO_BUZZER` | driven with `tone()` / `noTone()` |
| Button A (top, right) | User key | `WIO_KEY_A` | Coach: **reset counters** |
| Button B (top, middle) | User key | `WIO_KEY_B` | Coach: **recalibrate rest position** |
| Button C (top, left) | User key | `WIO_KEY_C` | Coach: **mute / unmute** · Data collection: **start recording** |
| 5-way switch left/right | Joystick | `WIO_5S_LEFT`, `WIO_5S_RIGHT` | Data collection: choose label |
| Light sensor | Built-in | `WIO_LIGHT` | only read once to seed `random()` |
| USB-C | Serial 115200 baud | `Serial` | debug output / CSV takes |

All buttons are active LOW and use `INPUT_PULLUP`.

## Power
- During data collection: USB-C cable to the laptop (the CSV takes travel through Serial).
- During training: USB cable to a laptop or a USB power bank. If you use a power bank, the coach still
  works normally; you only lose the Serial debug output.

## Mounting on the machine

```
            side view of the leg-extension machine

     REST (arm down)                    TOP (leg extended)

        seat                               seat
     ┌───────┐                          ┌───────┐
     │       │ ● pivot (knee)           │       │ ● pivot ───────[WIO]══ pad
     │       │ │                        │       │
     └───────┘ │                        └───────┘
               │                                     angle ≈ 60–90°
             [WIO]  ← screen facing
               ║      the user
              pad
```

- Fix the Wio Terminal **to the moving arm, close to the leg pad** (velcro strap, rubber bands or a 3D-printed
  clip). The farther from the pivot, the cleaner the signal.
- Screen facing the user so they can read the feedback; keep the USB-C port accessible.
- It must not wobble: a loose device adds vibrations that confuse both the angle and the model.
- **Use exactly the same position and orientation for data collection and for training with the coach.**
  The model learns the signal as the sensor sees it; if the Wio is rotated, the axes change and the model
  gets worse.

### Why no axis configuration is needed
The arm angle is computed as the **angle between the current gravity direction and the gravity direction
recorded at rest** (calibration at power-on, or button B). This works for any orientation of the Wio on the
arm, so there is no "which axis" setting. Just make sure the arm is down and still while it says
*CALIBRATING*. If the thresholds feel wrong for your machine, change the angles in
`leg_extension_coach/config.h` (`START_DELTA_DEG`, `TOP_DELTA_DEG`, `END_DELTA_DEG`); the live screen shows the
current angle to help you choose them.
