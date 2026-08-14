# Devlog 1

**14 Aug 2026** 

## Where the project is

Building a motor fault detector. An accelerometer goes on a small USB fan, and a model reads the vibration and says whether the fan is running fine or running unbalanced. Same idea as predictive maintenance on real motors, just small enough to fit on my desk.

The Uno is only the sensor. It samples the accelerometer and sends the numbers over serial, and the model runs on my laptop. I looked at doing inference on the Uno itself but it doesn't have the memory for it, so splitting it this way was the only real option.

## Today

Last parts arrived (MPU-6050, breadboard, USB fan). I now have everything I need, nothing left on order.

Installed the Arduino IDE and it picked up the Uno straight away. I was expecting to waste an hour on drivers or COM ports and didn't, so that was a nice surprise.

Then I tested the serial link before touching the sensor at all. Uploaded a sketch, watched the numbers come through the serial monitor, checked the upload and read loop worked properly. I wanted this working first because everything else depends on it. If serial is unreliable then debugging the sensor later becomes impossible, since you can't tell whether the sensor is wrong or the connection is.

I also wanted to check the fault is actually there before I spend weeks trying to detect it. Ran the fan clean, then added an imbalance, and I could feel the difference straight away. Big obvious wobble through the whole body of the fan.

That's useful but it also creates a problem. If I can feel it with my hand, then a simple threshold on vibration size would catch it too, and then the ML part of the project is pointless. So I need more than healthy vs broken. I'm going to plan for imbalance at two or three different severities, and ideally a second type of fault, so the model has to do something a threshold can't. Better to design the data collection around that now than redo it later.

## Blocked

The MPU-6050 came with the pin header not soldered on. Board and pin strip are two separate things in the bag, so I can't put it in the breadboard yet and can't collect any data.

No soldering iron here, so I'm taking it to a repair shop tomorrow. Bringing the board, the header, and the breadboard so they can check it lines up. Pins go on the opposite side to the chip, short ends down, so it sits flat. Getting all 8 done even though I only use 4, because the extra ones hold it in place.

When I get it back I'll check the joints and check no two pins are bridged. If SDA and SCL are touching, the sensor just won't appear at all, and that looks exactly like a dead board, so I'd rather find it with my eyes than spend an evening debugging code that was never broken.

## Next

- Soldering done, joints checked
- Wire the MPU-6050 over I2C and confirm the Uno finds it
- Get 3-axis data streaming at a stable sample rate
- Work out what that sample rate should be. Needs to be well above twice the fan's rotation speed or the vibration signature gets aliased
- Decide the fault classes properly
- Build a proper mount for the sensor

Last thing. The mount is more important than it looks. If the sensor sits slightly differently each time I record, the model ends up learning how I taped it down instead of learning the fault, and whatever accuracy I get at the end means nothing. So I want to fix the position once and leave it alone for the whole of data collection.
