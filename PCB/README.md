# PCB

**There is no custom PCB in this project.**

The build uses an Arduino Uno Rev3 with an MPU-6050 breakout module on a
solderless breadboard, so there are no board source files to publish. This
folder holds the wiring diagram instead, which serves the same purpose — it
shows exactly how the circuit is connected so someone can reproduce it.

`wiring-diagram.svg` — MPU-6050 to Arduino Uno.

| MPU-6050 | Uno | Note |
|---|---|---|
| VCC | 5V | via the breadboard's + rail |
| GND | GND | via the breadboard's − rail |
| SDA | A4 | I²C, fixed on the Uno |
| SCL | A5 | I²C, fixed on the Uno |

The only soldering in the build is the 8-way pin header onto the MPU-6050
module. The long side of the header goes into the breadboard.

A custom PCB would be a reasonable next step — it would remove the breadboard
as a source of intermittent contacts and let the sensor be mounted rigidly,
which is the main weakness of the current build.
