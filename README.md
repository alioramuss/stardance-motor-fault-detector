# Motor Fault Detector

An edge-AI predictive maintenance system. An accelerometer on a small motor
streams vibration to a laptop, where a machine-learning model decides in real
time whether the motor is running healthy or has developed a fault.

Built for the [Stardance Challenge 2026](https://stardance.hackclub.com)
(NASA × Hack Club × AMD × GitHub).

---

## The problem

When a motor develops a mechanical fault — a bearing wearing out, a rotor going
out of balance — it changes how it vibrates long before it fails. Industry
catches this with condition monitoring: sensors on the machine, software
watching for the signature of a fault. The equipment that does it costs
thousands.

This is the same idea for about £15, using a hobbyist accelerometer and a
laptop.

---

## Hardware

| Part | Role |
|---|---|
| Arduino Uno Rev3 | Samples the sensor and streams over USB |
| MPU-6050 (GY-521) | 3-axis accelerometer, ±2 g range, 16384 counts per g |
| USB desk fan | The motor under test |
| Breadboard + jumpers | |

Wiring — the Uno's I²C lives on A4/A5 and cannot be moved:

| MPU-6050 | Uno |
|---|---|
| VCC | 5V |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

**Fault injection.** A small weight taped to one fan blade creates a rotor
imbalance — the most common mechanical fault in rotating machinery, and one
that produces a clear vibration signature at the rotation frequency.

**Sensor mounting.** The MPU-6050 is fixed to the fan body with double-sided
tape rather than a printed bracket. This is a known weakness: the sensor's
position shifted between the first two recordings, which is why the healthy and
faulty datasets are not perfectly comparable. A printed holder is planned, and
would be the single biggest improvement to data quality.

A full CAD assembly of the build is in [`CAD/`](CAD/), as STEP and STL.

---

## How it works

```
MPU-6050 ──I²C──> Arduino Uno ──USB serial──> browser
  200 Hz            timestamped CSV            ├─ 1 s rolling window
                    115200 baud                ├─ spectral features (FFT)
                                               ├─ neural network
                                               └─ HEALTHY / FAULT
                                                        │
                              LED alarm <──serial───────┘
```

The Uno does no inference. With 2 KB of RAM it cannot hold a neural network, so
it acts purely as a sensor node — a real constraint that shaped the design, not
a shortcut.

### Sampling

`firmware/motor_sensor.ino` samples all three axes at a fixed **200 Hz** and
prints `timestamp,ax,ay,az` at 115200 baud. The loop is timed with `micros()`
rather than `delay()` so the interval stays constant; the FFT downstream is
meaningless if the sample rate wanders.

200 Hz gives 100 Hz of usable bandwidth, comfortably above the fan's ~37.5 Hz
rotation frequency.

### Data collection

`tools/capture.ps1` reads the serial port and writes CSV. No extra software is
required — it uses `System.IO.Ports`, which ships with Windows.

Two 60-second recordings, 12,000 samples each: fan normal, and fan with the
weight attached. Each was split into twelve 5-second files so the training and
test sets could be split at the sample level.

### Features and model

Built in [Edge Impulse](https://edgeimpulse.com):

| Setting | Value |
|---|---|
| Window | 1000 ms |
| Stride | 500 ms (50% overlap) |
| Processing | Spectral Analysis, FFT length 64 |
| Classifier | Dense network, 111 input features |
| Training windows | 170 (90 faulty, 80 healthy) |

Spectral Analysis matters here for a specific reason: it removes the DC
component before the FFT. Without that, the model could learn *which way the
sensor is tilted* — gravity dominates the raw signal at roughly 16,000 counts,
while the vibration is a few thousand. Stripping DC forces it to use the
frequency content, which is what a fault actually changes.

### Deployment

The model is exported as a **WebAssembly** build and runs in the browser. The
dashboard reads the serial port directly through the Web Serial API, so
inference happens on the same machine with no server and no cloud call.

---

## Results

### Fan rotation frequency

The FFT shows a clear peak at **37.5 Hz — about 2,250 rpm.** The fan's speed,
measured with an accelerometer and some maths rather than a tachometer.

### Raw separation, before any ML

Standard deviation of the X axis:

| Condition | sd (counts) |
|---|---|
| Healthy | 2,507 |
| Imbalanced | 3,875 |

A 55% increase. The fault is visible in the raw data before a model touches it,
which is the right thing to check first.

### Classifier

| | Accuracy |
|---|---|
| First attempt, unnormalised features | 64.7% |
| With StandardScaler + 100 training cycles | 100% |
| Held-out test set | 5/5 samples, 44/44 windows |

The first attempt failed in an informative way: it classified almost everything
as faulty. The features range from log-spectral values near 6 to RMS values in
the thousands, and a network fed inputs on wildly different scales struggles to
converge. Normalising fixed it completely.

### Live performance

| Metric | Value |
|---|---|
| Sample rate sustained | 200/sec |
| Inference time | 0.3 ms |
| Decision latency | ~1 s (one window) |

---

## Honest limitations

**The 100% is not as good as it looks.** Both classes came from a single
continuous recording each. The training and test windows are therefore
near-neighbours from the same session — same fan, same position, same
afternoon. The test set measures whether the pipeline works, not whether the
model generalises.

In live use it detects the fault reliably but not every single time. That gap
between 100% on paper and imperfect in practice is the honest result, and it
comes directly from the single-session dataset.

**Two classes forces a choice.** Shown something it has never seen — a shaken
fan, a different motor, a fan switched off — the model must still answer
"healthy" or "faulty". It cannot say "this is something else". A real system
needs an anomaly class or a rejection threshold.

**One fault type, one machine.** Rotor imbalance on one USB fan. Nothing here
demonstrates transfer to bearing wear, or to a different motor.

### What would fix it

- Record many short sessions across different days, fan speeds and sensor
  placements, instead of one long one
- Add a "motor off" class so the model can't win by detecting whether the fan
  is running
- Add a second fault type — a rubbing or obstructed blade
- Measure the live detection rate properly: apply the fault N times, count how
  many are caught

---

## Running it

### Collect data

```powershell
# fan running in the condition you want to record
.\tools\capture.ps1 -Label healthy -Seconds 60
```

Writes `data/healthy.1.csv` … `data/healthy.12.csv`. Run it again with
`-Label faulty` after taping a small weight to one blade.

### Live detection

```powershell
cd model\browser
node serve.js
```

Then open `http://localhost:8000/live.html` in Chrome or Edge and click
**Connect Arduino**. Close the Arduino IDE's Serial Monitor first — only one
program can hold the port.

The dashboard shows the verdict and confidence, a live FFT spectrum with the
rotation peak marked, the vibration waveform with gravity removed, a history
strip of recent predictions, and an adjustable alarm threshold.

### Alarm threshold

The slider sets how much evidence is needed before declaring a fault, with
15 points of hysteresis so a marginal signal cannot flicker. Lower it and faults
are caught earlier at the cost of occasional false alarms — usually the right
trade for an alarm, since a missed failure costs more than a false one. Where
that line goes is an engineering decision, not something the model decides.

---

## Build notes

A few things that cost real time and might save someone else some.

**The Edge Impulse CLI will not install on Windows ARM64.** `npm install -g
edge-impulse-cli` fails because node-gyp cannot find a C++ compiler, and it
installs a broken shell that fails at runtime with MODULE_NOT_FOUND. Rather
than install several gigabytes of Visual Studio Build Tools, this project routes
around it entirely: CSVs are uploaded through Edge Impulse Studio's browser
uploader, and the model is deployed as WebAssembly instead of a compiled
library. Nothing is lost, and the whole toolchain stays compiler-free.

**Byte order in the accelerometer read.** The common Arduino idiom
`Wire.read() << 8 | Wire.read()` does not guarantee which `read()` executes
first, so it can silently return byte-swapped values. Reading each byte into its
own variable removes the gamble.

**Edge Impulse infers labels from filenames** up to the first dot, so
`healthy.1.csv` lands as *healthy*. Splitting each recording into numbered
chunks gives both automatic labelling and enough samples to split train/test.

---

## Repository

```
CAD/                      full assembly, STEP and STL
PCB/                      wiring diagram (no custom board — breadboard build)
firmware/                 Arduino sketch — samples at 200 Hz, drives the LED alarm
tools/capture.ps1         serial to CSV logger
data/                     recorded datasets, 5-second chunks
model/browser/            exported WebAssembly model, live dashboard, local server
docs/wiring.svg           wiring diagram
devlogs/                  build log
experiments/ultrasonic/   parked HC-SR04 non-contact sensing experiment
BOM.csv                   parts in the build
BOM-funding.csv           parts requested for the next phase
```

The `experiments/` folder is work that didn't make it into the main result —
kept because the reasoning is useful, and the write-up in
`experiments/ultrasonic/rig-design.md` explains why the approach hits a hard
physical limit.

---

## Licence

MIT.
