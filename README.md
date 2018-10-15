# motorcycle_instruments

Digital speedometer for a motorcycle. Runs on custom board emulating an Arduino. The mcu is an avr ATmega644. This project is the Arduino sketch.

The original speedometer on my motorcycle had an analog speed dial, with mechanical odometer and a mechanical trip counter. The digital speedometer has these functions:

- Speed
- Tachometer
- 2 trip meters
- Electrical System voltage measurement
- Outside air temperature

Written by Greg Green
GPLv3 license, see LICENSE

The project uses several external libraries that don't come with Arduino:

- Adafruit-GFX-Library
- Adafruit-PCD8544-Nokia-5110-LCD-library
- [AnalogDiff(http://www.github.com/gpgreen/AnalogDiff)]
- Bounce2
- [EEPROMStore(http://www.github.com/gpgreen/EEPROMStore)]
- elapsedMillis
- FreqMeasure
- [MotoPanel(http://www.github.com/gpgreen/MotoPanel)]
- MsTimer2

The hardware design for the project resides on github also.

[Speedometer pcb project(http://www.github.com/gpgreen/speedometer_hardware)]