# Non-contact sensing experiment — design notes

**Question.** The MPU-6050 detects fan imbalance reliably, but it has to be
bolted to the machine. Can a £2 HC-SR04 ultrasonic rangefinder detect the same
fault without touching anything — and if so, at what angle and standoff?

**Why it's worth doing.** Contact sensors are the standard answer in condition
monitoring, but they need mounting, wiring and access. Non-contact sensing is
the interesting frontier, and knowing precisely *where a cheap sensor stops
working* is a legitimate result. This experiment can fail and still be worth
writing up — as long as the failure is characterised rather than hand-waved.

---

## The honest physics, up front

Work out whether this can possibly succeed before running it, because the answer
shapes what you measure.

**Sample rate.** The HC-SR04 needs roughly 40–60 ms between triggers or the
previous echo is still ringing when the next arrives. That caps you at about
**25 Hz**. Your fan spins at **37.5 Hz**.

Nyquist says you need to sample above 75 Hz to reconstruct a 37.5 Hz signal.
You are sampling at a third of that. The wobble will **alias** — a 37.5 Hz
motion sampled at 25 Hz folds down to |37.5 − 25| = **12.5 Hz**. So you cannot
take an FFT of the ultrasonic data and read the rotation frequency off it. Any
peak you see there is an artefact.

**Resolution.** The HC-SR04 quantises to about 3 mm, with a few mm of noise. A
small fan's housing displacement under imbalance is likely well under 1 mm.

**Therefore:** don't try to reconstruct the vibration waveform. Measure the
**spread of the readings** — standard deviation, peak-to-peak, missed-echo rate.
Undersampled noise is still noise, and if the housing moves more when
unbalanced, the distribution of readings should widen even when the timing is
hopeless. That is the hypothesis worth testing.

**Predicted outcome:** marginal to negative for a small USB fan. If you want it
to have a chance, see *Improving the odds* below.

---

## Wiring

Everything shares one ground.

| From | To | Note |
|---|---|---|
| MPU-6050 VCC / GND | 5V / GND | stays bolted to the fan — this is the reference |
| MPU-6050 SDA / SCL | A4 / A5 | I²C, fixed on the Uno |
| HC-SR04 VCC / GND | 5V / GND | mounted on the servo horn |
| HC-SR04 TRIG | D7 | |
| HC-SR04 ECHO | **D2** | must be D2 or D3 — external interrupt |
| Servo signal | D9 | |
| Servo V+ | **external 5V** if you have one | see below |
| Servo GND | GND, shared | |

**Servo power.** A moving servo pulls hundreds of milliamps and can spike over
an amp. Through the Uno's regulator that sags the 5 V rail, and the MPU-6050
reads the sag as motion — a fault that isn't there. A USB power bank or 4×AA
pack fixes it. If you have neither, the sketch detaches the servo after each
move so it draws nothing while recording. If the HC-SR04 droops under its own
weight when detached, set `HOLD = true` in the sketch and accept the noise, or
brace the arm.

**Aim.** Point the rangefinder at a flat part of the fan body, 5–20 cm away.
Ultrasonic sensors need a surface roughly perpendicular to the beam; angled or
soft surfaces scatter the echo and you'll see a high missed-echo rate.

---

## Two embedded problems the sketch solves

**`pulseIn()` would destroy the sample timing.** The standard HC-SR04 example
blocks for up to 30 ms waiting for the echo. In a 5 ms sampling loop that's six
missing accelerometer samples per ping, which corrupts the FFT and breaks the
model. Instead the trigger is fired and forgotten, and the echo is timed by an
interrupt on D2. Nothing blocks, and the 200 Hz cadence is untouched.

**A holding servo injects electrical noise** into the sensor it's meant to be
carrying. `attach()` → move → settle → `detach()`.

Both are worth a line in the write-up. They're real constraints with standard
answers, and showing you anticipated them reads better than showing clean data.

---

## Protocol

Send `U` and the rig sweeps five angles — 60°, 75°, 90°, 105°, 120° — recording
10 seconds at each with both sensors running simultaneously. Accelerometer at
200 Hz is the reference truth; rangefinder at ~25 Hz is the thing on trial.

Run the whole sweep twice:

```powershell
.\sweep.ps1 -Condition healthy    # fan clean
# add the weight to one blade, nothing else changes
.\sweep.ps1 -Condition faulty
```

Critically: **do not move anything between the two runs except the weight.**
The servo returns to each angle itself, so the geometry is repeatable — that's
the whole reason the servo is in this experiment rather than a bit of Blu Tack.

Output, per condition per angle:

- `<condition>.a<angle>.acc.csv` — `ax,ay,az`
- `<condition>.a<angle>.dist.csv` — `t_ms,mm` (`-1` = no echo returned)

---

## Analysis

For each angle, compare healthy against faulty on:

1. **Standard deviation of distance** — the main test. Does the spread widen?
2. **Peak-to-peak range** — more sensitive to occasional large excursions
3. **Missed-echo rate** — a vibrating surface scatters echoes, so *more misses*
   under fault is itself a signal, and an unexpected one
4. **Accelerometer RMS at the same moment** — the reference. Confirms the fault
   really was present, so a null ultrasonic result means "the sensor can't see
   it", not "there was nothing to see"

That fourth one is what makes this a controlled experiment rather than a guess.

A separation ratio above about 1.5× on any measure is a real effect at this
noise level. Below about 1.2× and you're reading tea leaves.

---

## Improving the odds

If the first sweep shows nothing, these are legitimate escalations, in order:

1. **More weight.** A bigger imbalance means more displacement. Characterising
   *how much* imbalance is needed before the ultrasonic sees it is arguably a
   better result than a yes/no — it gives you a detection threshold.
2. **Mechanical amplification.** Tape a light card to the fan body as the
   target surface. It acts as a lever, so a small housing movement becomes a
   larger movement at the card. This is a real technique, not a cheat — say so
   in the write-up.
3. **Closer standoff.** Shorter range means shorter echo flight time and less
   variance. Don't go below about 3 cm, the sensor's minimum.
4. **Softer mount for the fan.** A fan on foam moves more than one clamped to a
   desk. Arguably more realistic too — real machines sit on mounts.

---

## What to write up either way

- The Nyquist argument, worked out **before** collecting data rather than as an
  excuse afterwards
- The interrupt-driven ranging, and why `pulseIn()` was unusable
- A table of separation ratios by angle, for both sensors
- A clear statement of the detection threshold — the imbalance below which the
  non-contact channel stops working

A characterised limitation is a result. "It didn't work" is not.
